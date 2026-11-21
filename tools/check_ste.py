##
# @file     check_ste.py
# @brief    Repository-wide ASD-STE100 Issue 9 gate for README.md and docs/
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Run ste_check.py across README.md and all non-immutable docs/ Markdown."""

# need:: COMP_DOCS_0001 VER_DOCS_0001

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STE_DIR = Path(
    os.environ.get(
        "ASD_STE100_SKILL",
        str(Path.home() / ".gemini/skills/asd-ste100"),
    )
)
DEFAULT_STE = DEFAULT_STE_DIR / "scripts" / "ste_check.py"

NEED_DIRECTIVES = frozenset(
    {
        "sys",
        "swr",
        "arc",
        "icd",
        "comp",
        "ver",
        "bdr",
        "wdr",
        "adr",
        "task",
        "roadmap",
        "con",
    }
)


def _extract_markdown(text: str) -> str:
    """Expose prose inside Sphinx-Needs blocks while keeping code fenced."""
    out: list[str] = []
    in_need = False
    in_yaml = False
    in_skip = False
    for line in text.splitlines():
        if not in_need and not in_skip:
            m = re.match(r"^```\{([a-zA-Z0-9_-]+)\}", line)
            if m:
                kind = m.group(1)
                if kind in NEED_DIRECTIVES:
                    in_need = True
                    in_yaml = False
                    out.append("")
                    continue
                if kind in {"needtable", "needflow", "needpie", "toctree"}:
                    in_skip = True
                    out.append("")
                    continue
            out.append(re.sub(r"\{need\}`[^`]+`", "item", line))
            continue

        if in_skip:
            if line.strip() == "```":
                in_skip = False
            out.append("")
            continue

        if line.strip() == "```":
            in_need = False
            in_yaml = False
            out.append("")
            continue
        if line.strip() == "---":
            in_yaml = not in_yaml
            out.append("")
            continue
        if in_yaml or line.startswith(":"):
            out.append("")
            continue
        out.append(re.sub(r"\{need\}`[^`]+`", "item", line))
    return "\n".join(out) + "\n"


def _iter_docs(root: Path) -> list[Path]:
    files: list[Path] = []
    readme = root / "README.md"
    if readme.is_file():
        files.append(readme)
    docs_dir = root / "docs"
    if docs_dir.is_dir():
        for p in sorted(docs_dir.rglob("*.md")):
            if "references" in p.parts:
                continue
            files.append(p)
    return files


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check README.md and docs/**/*.md with ste_check.py."
    )
    parser.add_argument("--root", default=str(ROOT))
    parser.add_argument("--ste-script", default=str(DEFAULT_STE))
    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    ste_script = Path(args.ste_script)
    if not ste_script.is_file():
        print(
            f"STE: FAIL (missing ste_check.py at {ste_script})",
            file=sys.stderr,
        )
        return 2

    terms = root / "technical_terms.txt"
    phrases = root / "allowed_phrases.txt"
    files = _iter_docs(root)
    failures = 0

    for path in files:
        extracted = _extract_markdown(path.read_text(encoding="utf-8"))
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".md", encoding="utf-8", delete=False
        ) as tf:
            tf.write(extracted)
            tmp_path = Path(tf.name)
        try:
            res = subprocess.run(
                [
                    sys.executable,
                    str(ste_script),
                    "scan",
                    "--terms",
                    str(terms),
                    "--phrases",
                    str(phrases),
                    str(tmp_path),
                ],
                capture_output=True,
                text=True,
            )
        finally:
            tmp_path.unlink(missing_ok=True)
        if res.returncode != 0:
            failures += 1
            rel = path.relative_to(root)
            print(f"STE ERROR: {rel}", file=sys.stderr)
            for line in res.stdout.splitlines():
                if " ERROR " in line:
                    print(f"  {line.strip()}", file=sys.stderr)

    if failures:
        print(f"STE: FAIL ({failures} file(s) with errors)", file=sys.stderr)
        return 1
    print(f"STE: PASS ({len(files)} files checked, 0 errors)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
