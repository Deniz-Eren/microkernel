##
# @file     needs_model.py
# @brief    Single source of truth for the Sphinx-Needs traceability model
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Pure-Python need model loaded from `.docs-traceability-setup.yml`."""

# need:: ICD_DOCS_0001

from __future__ import annotations

import fnmatch
import re
from pathlib import Path
from typing import Any

CONFIG_FILENAME = ".docs-traceability-setup.yml"
REPO_ROOT = Path(__file__).resolve().parents[2]

DOMAIN_RE = re.compile(r"^[A-Z0-9]+$")
VALID_DECISION_TYPES = frozenset({"adr", "bdr", "wdr"})
VALID_ID_STYLES = frozenset({"slug", "number"})
VALID_MARKUPS = frozenset({"myst", "rst"})
VALID_CMAKE_MODES = frozenset({"subdirectory", "standalone"})
VALID_GRAPHVIZ_MODES = frozenset({"auto", "on", "off"})

SCHEMA_KEYS: dict[str, frozenset[str]] = {
    "project": frozenset({"name", "repo_url", "domains"}),
    "id": frozenset({"style", "digits"}),
    "authoring": frozenset({"primary_markup", "modal_verb"}),
    "model": frozenset(
        {
            "decision_types",
            "ver_method_tags",
            "test_file_patterns",
            "code_tag_exclude",
        }
    ),
    "build": frozenset(
        {
            "cmake_mode",
            "python_manifest",
            "docs_auto_install",
            "graphviz",
        }
    ),
    "checks": frozenset({"file_headers", "prose_packing"}),
}

DOMAIN_TYPE_SPECS: tuple[dict[str, str], ...] = (
    {
        "directive": "sys",
        "title": "System Requirement",
        "prefix": "SYS_",
        "color": "#bfd8d2",
        "style": "node",
    },
    {
        "directive": "swr",
        "title": "Software Requirement",
        "prefix": "SWR_",
        "color": "#dfecd7",
        "style": "node",
    },
    {
        "directive": "arc",
        "title": "Architecture Element",
        "prefix": "ARC_",
        "color": "#d7d2ec",
        "style": "node",
    },
    {
        "directive": "icd",
        "title": "Interface",
        "prefix": "ICD_",
        "color": "#f2cfa5",
        "style": "node",
    },
    {
        "directive": "comp",
        "title": "Component Spec",
        "prefix": "COMP_",
        "color": "#e8e4c9",
        "style": "node",
    },
    {
        "directive": "ver",
        "title": "Verification Item",
        "prefix": "VER_",
        "color": "#a3d2ca",
        "style": "node",
    },
)

DECISION_TYPE_SPECS: tuple[dict[str, str], ...] = (
    {
        "directive": "adr",
        "title": "Architecture Decision (ADR)",
        "prefix": "ADR_",
        "color": "#ffb6b9",
        "style": "node",
    },
    {
        "directive": "bdr",
        "title": "Business Decision Record (BDR)",
        "prefix": "BDR_",
        "color": "#ffd166",
        "style": "node",
    },
    {
        "directive": "wdr",
        "title": "Workflow Decision Record",
        "prefix": "WDR_",
        "color": "#118ab2",
        "style": "node",
    },
)

RECORD_TYPE_SPECS: tuple[dict[str, str], ...] = (
    {
        "directive": "con",
        "title": "Convention",
        "prefix": "CNV_",
        "color": "#a9cce3",
        "style": "node",
    },
    {
        "directive": "task",
        "title": "Task",
        "prefix": "TSK_",
        "color": "#f5cba7",
        "style": "node",
    },
    {
        "directive": "roadmap",
        "title": "Roadmap Item",
        "prefix": "RDM_",
        "color": "#d5d8dc",
        "style": "node",
    },
)

NEEDS_LINKS: dict[str, dict[str, Any]] = {
    "derives": {
        "incoming": "derived into",
        "outgoing": "derived from",
        "copy": False,
        "color": "#4a7c59",
    },
    "satisfies": {
        "incoming": "satisfied by",
        "outgoing": "satisfies",
        "copy": False,
        "color": "#3d5a80",
    },
    "refines": {
        "incoming": "refined by",
        "outgoing": "refines",
        "copy": False,
        "color": "#6c5b7b",
    },
    "verifies": {
        "incoming": "verified by",
        "outgoing": "verifies",
        "copy": False,
        "color": "#2a9d8f",
    },
    "drives": {
        "incoming": "driven by",
        "outgoing": "drives",
        "copy": False,
        "color": "#e9c46a",
    },
    "governs": {
        "incoming": "governed by",
        "outgoing": "governs",
        "copy": False,
        "color": "#264653",
    },
    "traces": {
        "incoming": "traced by",
        "outgoing": "traces",
        "copy": False,
        "color": "#7f8c8d",
    },
    "supersedes": {
        "incoming": "superseded by",
        "outgoing": "supersedes",
        "copy": False,
        "color": "#e76f51",
    },
}

NORMATIVE_TYPES = frozenset({"sys", "swr", "arc", "icd", "comp", "ver"})
DECISION_TYPES_ALL = frozenset({"adr", "bdr", "wdr"})
PLANNING_TYPES = frozenset({"task", "roadmap"})
ALL_TYPES_SET = NORMATIVE_TYPES | DECISION_TYPES_ALL | {"con"} | PLANNING_TYPES

ALLOWED_LINK_MATRIX: dict[str, frozenset[tuple[str, str]]] = {
    "derives": frozenset(
        {
            ("swr", "sys"),
            ("sys", "sys"),
            ("swr", "swr"),
        }
    ),
    "satisfies": frozenset(
        {
            ("arc", "sys"),
            ("arc", "swr"),
            ("icd", "swr"),
            ("comp", "swr"),
        }
    ),
    "refines": frozenset(
        {
            ("arc", "arc"),
            ("icd", "arc"),
            ("comp", "arc"),
            ("comp", "icd"),
        }
    ),
    "verifies": frozenset(
        {
            ("ver", "sys"),
            ("ver", "swr"),
        }
    ),
    "drives": frozenset(
        {
            ("bdr", "sys"),
            ("bdr", "swr"),
        }
    ),
    "governs": frozenset(
        {
            ("bdr", "arc"),
            ("bdr", "adr"),
            ("wdr", "arc"),
            ("wdr", "adr"),
        }
    ),
    "traces": frozenset(
        {
            *(
                (s, t)
                for s in (NORMATIVE_TYPES | {"con"})
                for t in (DECISION_TYPES_ALL | {"con"})
            ),
            *((s, t) for s in PLANNING_TYPES for t in ALL_TYPES_SET),
        }
    ),
    "supersedes": frozenset(
        {
            ("adr", "adr"),
            ("bdr", "bdr"),
            ("wdr", "wdr"),
        }
    ),
}

STATUS_GROUPS: dict[str, tuple[str, ...]] = {
    "normative": ("draft", "active", "obsolete"),
    "decision": ("proposed", "accepted", "superseded", "deprecated"),
    "planning": ("open", "in_progress", "done", "cancelled"),
}

ALL_STATUSES: list[str] = [
    *STATUS_GROUPS["normative"],
    *STATUS_GROUPS["decision"],
    *STATUS_GROUPS["planning"],
]

INACTIVE_STATUSES: tuple[str, ...] = (
    "draft",
    "obsolete",
    "superseded",
    "deprecated",
    "proposed",
)

INFORMATIONAL_TAG = "informational"
CODE_ALLOWED_PREFIXES = frozenset({"ARC", "ICD", "COMP"})
TEST_ALLOWED_PREFIXES = frozenset({"VER"})
CODE_FORBIDDEN_PREFIXES = frozenset(
    {"SYS", "SWR", "ADR", "BDR", "WDR", "CNV", "TSK", "RDM"}
)

COMMENT_MARKERS: dict[str, tuple[str, ...]] = {
    ".py": ("#",),
    ".sh": ("#",),
    ".cmake": ("#",),
    "CMakeLists.txt": ("#",),
    ".rb": ("#",),
    ".pl": ("#",),
    ".r": ("#",),
    ".yaml": ("#",),
    ".yml": ("#",),
    ".editorconfig": ("#",),
    ".c": ("//", "/*"),
    ".h": ("//", "/*"),
    ".cc": ("//", "/*"),
    ".cpp": ("//", "/*"),
    ".cxx": ("//", "/*"),
    ".hpp": ("//", "/*"),
    ".hh": ("//", "/*"),
    ".hxx": ("//", "/*"),
    ".rs": ("//", "/*"),
    ".go": ("//", "/*"),
    ".js": ("//", "/*"),
    ".mjs": ("//", "/*"),
    ".ts": ("//", "/*"),
    ".tsx": ("//", "/*"),
    ".jsx": ("//", "/*"),
    ".java": ("//", "/*"),
    ".kt": ("//", "/*"),
    ".cs": ("//", "/*"),
    ".swift": ("//", "/*"),
    ".sql": ("--",),
    ".lua": ("--",),
    ".hs": ("--",),
    ".vhd": ("--",),
    ".vhdl": ("--",),
    ".adb": ("--",),
    ".ads": ("--",),
    ".rst": ("..",),
}


def _strip_comment(raw: str) -> str:
    in_single = False
    in_double = False
    escaped = False
    for idx, ch in enumerate(raw):
        if escaped:
            escaped = False
            continue
        if ch == "\\" and in_double:
            escaped = True
            continue
        if ch == "'" and not in_double:
            in_single = not in_single
        elif ch == '"' and not in_single:
            in_double = not in_double
        elif ch == "#" and not in_single and not in_double:
            if idx == 0 or raw[idx - 1].isspace():
                return raw[:idx].rstrip()
    return raw.rstrip()


def _parse_scalar(text: str, line_no: int) -> Any:
    val = text.strip()
    if val == "":
        return ""
    if val.startswith("[") and val.endswith("]"):
        inner = val[1:-1].strip()
        if not inner:
            return []
        items: list[Any] = []
        buf: list[str] = []
        in_single = False
        in_double = False
        for ch in inner:
            if ch == "'" and not in_double:
                in_single = not in_single
                buf.append(ch)
            elif ch == '"' and not in_single:
                in_double = not in_double
                buf.append(ch)
            elif ch == "," and not in_single and not in_double:
                items.append(_parse_scalar("".join(buf), line_no))
                buf = []
            else:
                buf.append(ch)
        if in_single or in_double:
            raise ValueError(f"line {line_no}: unterminated quote in list")
        items.append(_parse_scalar("".join(buf), line_no))
        return items
    if (val.startswith('"') and val.endswith('"')) or (
        val.startswith("'") and val.endswith("'")
    ):
        if len(val) < 2:
            raise ValueError(f"line {line_no}: unterminated string")
        body = val[1:-1]
        if val[0] == '"':
            return body.replace('\\"', '"').replace("\\\\", "\\")
        return body.replace("''", "'")
    if val.startswith('"') or val.startswith("'"):
        raise ValueError(f"line {line_no}: unterminated string")
    if val == "true":
        return True
    if val == "false":
        return False
    if re.fullmatch(r"-?[0-9]+", val):
        return int(val)
    return val


def _parse_yaml(text: str) -> dict[str, Any]:
    data: dict[str, Any] = {}
    current_section: str | None = None
    current_list_key: tuple[str, str] | None = None
    list_indent: int | None = None
    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        cleaned = _strip_comment(raw_line)
        if not cleaned.strip():
            continue
        indent = len(cleaned) - len(cleaned.lstrip(" "))
        stripped = cleaned.lstrip(" ")
        if indent == 0:
            current_list_key = None
            list_indent = None
            key, sep, rest = stripped.partition(":")
            key = key.strip()
            if not sep or not key or rest.strip():
                raise ValueError(f"line {line_no}: invalid section header")
            if key in data:
                raise ValueError(f"line {line_no}: duplicate section '{key}'")
            data[key] = {}
            current_section = key
            continue
        if current_section is None:
            raise ValueError(f"line {line_no}: indented line before section")
        if stripped.startswith("- "):
            if current_list_key is None:
                raise ValueError(f"line {line_no}: unexpected list item")
            if list_indent is None:
                list_indent = indent
            elif indent != list_indent:
                raise ValueError(f"line {line_no}: inconsistent list indent")
            sec, k = current_list_key
            data[sec][k].append(_parse_scalar(stripped[2:], line_no))
            continue
        if indent != 2 or ":" not in stripped:
            raise ValueError(f"line {line_no}: expected 2-space 'key: value'")
        current_list_key = None
        list_indent = None
        key, _, rest = stripped.partition(":")
        key = key.strip()
        rest = rest.strip()
        if not key or key in data[current_section]:
            raise ValueError(f"line {line_no}: invalid or duplicate key")
        if rest == "":
            data[current_section][key] = []
            current_list_key = (current_section, key)
        else:
            data[current_section][key] = _parse_scalar(rest, line_no)
    return data


def load_config(root: Path = REPO_ROOT) -> dict[str, Any]:
    """Read and validate `.docs-traceability-setup.yml` at `root`."""
    path = root / CONFIG_FILENAME
    if not path.is_file():
        raise ValueError(f"missing configuration file: {path}")
    raw = _parse_yaml(path.read_text(encoding="utf-8"))
    unknown_sec = set(raw.keys()) - set(SCHEMA_KEYS.keys())
    if unknown_sec:
        raise ValueError(
            f"unknown section(s): {', '.join(sorted(unknown_sec))}"
        )
    missing_sec = set(SCHEMA_KEYS.keys()) - set(raw.keys())
    if missing_sec:
        raise ValueError(
            f"missing section(s): {', '.join(sorted(missing_sec))}"
        )
    for sec, allowed in SCHEMA_KEYS.items():
        unknown = set(raw[sec].keys()) - allowed
        if unknown:
            raise ValueError(
                f"unknown key(s) in '{sec}': {', '.join(sorted(unknown))}"
            )
    id_style = raw["id"].get("style")
    if id_style not in VALID_ID_STYLES:
        raise ValueError(f"invalid id.style: {id_style!r}")
    if id_style == "number":
        digits = raw["id"].get("digits")
        if (
            isinstance(digits, bool)
            or not isinstance(digits, int)
            or digits < 1
        ):
            raise ValueError("id.digits must be a positive integer")
    dec = raw["model"].get("decision_types", [])
    if not isinstance(dec, list) or any(
        d not in VALID_DECISION_TYPES for d in dec
    ):
        raise ValueError(f"invalid model.decision_types: {dec!r}")
    domains = raw["project"].get("domains", [])
    if not domains or any(not DOMAIN_RE.fullmatch(d) for d in domains):
        raise ValueError(f"invalid project.domains: {domains!r}")
    if raw["authoring"].get("primary_markup") not in VALID_MARKUPS:
        raise ValueError("invalid authoring.primary_markup")
    if raw["build"].get("cmake_mode") not in VALID_CMAKE_MODES:
        raise ValueError("invalid build.cmake_mode")
    if raw["build"].get("graphviz") not in VALID_GRAPHVIZ_MODES:
        raise ValueError("invalid build.graphviz")
    return raw


def build_needs_types(config: dict[str, Any]) -> list[dict[str, str]]:
    """Build active `needs_types` list from configuration."""
    enabled_dec = set(config["model"]["decision_types"])
    types = [dict(t) for t in DOMAIN_TYPE_SPECS]
    for t in DECISION_TYPE_SPECS:
        if t["directive"] in enabled_dec:
            types.append(dict(t))
    types.extend(dict(t) for t in RECORD_TYPE_SPECS)
    return types


def build_id_regex(config: dict[str, Any]) -> str:
    """Build anchored `needs_id_regex` from `id.style`, `id.digits`, types."""
    domain_prefixes = "|".join(
        t["prefix"].rstrip("_") for t in DOMAIN_TYPE_SPECS
    )
    enabled_dec = set(config["model"]["decision_types"])
    flat_specs = [
        t for t in DECISION_TYPE_SPECS if t["directive"] in enabled_dec
    ] + list(RECORD_TYPE_SPECS)
    flat_prefixes = "|".join(t["prefix"].rstrip("_") for t in flat_specs)
    style = config["id"]["style"]
    if style == "slug":
        suffix = "[A-Z][A-Z0-9]*(_[A-Z0-9]+)*"
    else:
        digits = int(config["id"]["digits"])
        suffix = f"[0-9]{{{digits}}}"
    return (
        f"^({domain_prefixes})_[A-Z0-9]+_{suffix}$|"
        f"^({flat_prefixes})_{suffix}$"
    )


def build_tag_id_pattern(config: dict[str, Any]) -> re.Pattern[str]:
    """Compile an unanchored token regex matching valid IDs in code tags."""
    anchored = build_id_regex(config)
    parts = [p.lstrip("^").rstrip("$") for p in anchored.split("|")]
    return re.compile(f"^(?:{'|'.join(parts)})$")


def is_test_file(rel_path: str | Path, patterns: list[str]) -> bool:
    """Return True if the filename matches any configured test file pattern."""
    name = Path(rel_path).name
    return any(fnmatch.fnmatch(name, pat) for pat in patterns)


def get_comment_markers(path: str | Path) -> tuple[str, ...]:
    """Return comment markers for a source file, or empty tuple if unknown."""
    p = Path(path)
    if p.name in COMMENT_MARKERS:
        return COMMENT_MARKERS[p.name]
    return COMMENT_MARKERS.get(p.suffix.lower(), ())


CONFIG = load_config(REPO_ROOT)
NEEDS_TYPES = build_needs_types(CONFIG)
NEEDS_ID_REGEX = build_id_regex(CONFIG)
TAG_ID_RE = build_tag_id_pattern(CONFIG)
VER_METHOD_TAGS: list[str] = list(CONFIG["model"]["ver_method_tags"])
TEST_FILE_PATTERNS: list[str] = list(CONFIG["model"]["test_file_patterns"])
CODE_TAG_EXCLUDE: list[str] = list(CONFIG["model"]["code_tag_exclude"])
ENABLED_DECISION_TYPES: set[str] = set(CONFIG["model"]["decision_types"])


def get_prose_directives() -> tuple[str, ...]:
    """Return active Sphinx-Needs directive names for prose packing."""
    return tuple(t["directive"] for t in NEEDS_TYPES)
