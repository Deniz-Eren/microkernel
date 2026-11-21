##
# @file     tag_scan.py
# @brief    Scan repository source files for in-code need:: traceability tags
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Single in-code traceability tag scanner for the target project."""

# need:: COMP_DOCS_0002

from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from needs_model import (
    CODE_TAG_EXCLUDE,
    TEST_FILE_PATTERNS,
    get_comment_markers,
    is_test_file,
)

EXCLUDED_DIRS = frozenset(
    {
        ".git",
        ".venv",
        "venv",
        "build",
        "__pycache__",
        ".ruff_cache",
        ".pytest_cache",
        ".mypy_cache",
        "node_modules",
        "docs",
    }
)


@dataclass(frozen=True)
class TagOccurrence:
    """One `need::` comment tag found in a source or test file."""

    file: str
    line: int
    ids: tuple[str, ...]
    is_test: bool


@dataclass(frozen=True)
class TagScanResult:
    """Aggregated tag occurrences, test files, and stray planning-ID refs."""

    tags: tuple[TagOccurrence, ...]
    test_files: tuple[str, ...]
    planning_refs: tuple[tuple[str, int, str], ...] = ()


def _is_excluded(rel_posix: str, extra_excludes: Iterable[str]) -> bool:
    parts = Path(rel_posix).parts
    if any(part in EXCLUDED_DIRS for part in parts):
        return True
    for exc in extra_excludes:
        norm = exc.strip().strip("/")
        if not norm:
            continue
        if rel_posix == norm or rel_posix.startswith(f"{norm}/"):
            return True
    return False


def _iter_repo_files(
    root: Path, extra_excludes: Iterable[str]
) -> list[str]:
    tracked: list[str] | None = None
    if (root / ".git").exists():
        try:
            proc = subprocess.run(
                ["git", "-C", str(root), "ls-files", "-z"],
                check=False,
                capture_output=True,
            )
            if proc.returncode == 0:
                raw = proc.stdout.decode("utf-8", errors="surrogateescape")
                tracked = [e for e in raw.split("\0") if e]
        except OSError:
            tracked = None

    if tracked is not None:
        return sorted(
            f
            for f in tracked
            if not _is_excluded(f, extra_excludes) and (root / f).is_file()
        )

    found: list[str] = []
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = Path(dirpath).relative_to(root).as_posix()
        prefix = "" if rel_dir == "." else f"{rel_dir}/"
        dirnames[:] = [
            d
            for d in sorted(dirnames)
            if not _is_excluded(f"{prefix}{d}", extra_excludes)
        ]
        for fname in sorted(filenames):
            rel = f"{prefix}{fname}"
            if not _is_excluded(rel, extra_excludes):
                found.append(rel)
    return found


DOXYGEN_REQ_RE = re.compile(r"^\s*(?:/\*\*|\*)\s*\\req\{([^}]*)\}?")
PLANNING_ID_RE = re.compile(r"\b(?:TSK|RDM)_[0-9]+\b")


def extract_tags_from_line(
    line: str, markers: tuple[str, ...]
) -> tuple[str, ...] | None:
    """Return tuple of IDs if `need::` or `\\req{}` follows a comment marker."""
    stripped = line.lstrip()
    if "/*" in markers:
        m_req = DOXYGEN_REQ_RE.match(stripped)
        if m_req:
            payload = m_req.group(1).strip()
            return tuple(payload.split()) if payload else ()
    for marker in markers:
        if not stripped.startswith(marker):
            continue
        after = stripped[len(marker) :].lstrip()
        m = re.match(r"^need::(?:\s+(.*)|\s*)$", after)
        if not m:
            continue
        rest = (m.group(1) or "").strip()
        if marker == "/*" and rest.endswith("*/"):
            rest = rest[:-2].strip()
        if not rest:
            return ()
        return tuple(rest.split())
    return None


def scan_repository(
    root: Path,
    *,
    test_patterns: list[str] | None = None,
    extra_excludes: Iterable[str] | None = None,
) -> TagScanResult:
    """Scan tracked or walked files under `root` for in-code `need::` tags."""
    patterns = (
        TEST_FILE_PATTERNS if test_patterns is None else list(test_patterns)
    )
    excludes = (
        CODE_TAG_EXCLUDE if extra_excludes is None else list(extra_excludes)
    )
    tags: list[TagOccurrence] = []
    test_files: list[str] = []
    planning_refs: list[tuple[str, int, str]] = []

    for rel in _iter_repo_files(root, excludes):
        markers = get_comment_markers(rel)
        if not markers:
            continue
        file_is_test = is_test_file(rel, patterns)
        if file_is_test:
            test_files.append(rel)
        try:
            text = (root / rel).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        check_planning = not rel.startswith("tests/traceability/")
        for line_no, raw_line in enumerate(text.splitlines(), start=1):
            ids = extract_tags_from_line(raw_line, markers)
            if ids is not None:
                tags.append(
                    TagOccurrence(
                        file=rel,
                        line=line_no,
                        ids=ids,
                        is_test=file_is_test,
                    )
                )
            elif check_planning:
                for pid in PLANNING_ID_RE.findall(raw_line):
                    planning_refs.append((rel, line_no, pid))

    return TagScanResult(
        tags=tuple(tags),
        test_files=tuple(test_files),
        planning_refs=tuple(planning_refs),
    )
