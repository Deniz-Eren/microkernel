##
# @file     conf.py
# @brief    Sphinx and Sphinx-Needs 9 configuration for traceability builds
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Sphinx configuration driven by `.docs-traceability-setup.yml`."""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

CONF_DIR = Path(__file__).resolve().parent
if str(CONF_DIR) not in sys.path:
    sys.path.insert(0, str(CONF_DIR))

from needs_model import (  # noqa: E402
    ALL_STATUSES,
    CODE_TAG_EXCLUDE,
    CONFIG,
    ENABLED_DECISION_TYPES,
    INACTIVE_STATUSES,
    INFORMATIONAL_TAG,
    NEEDS_ID_REGEX,
    NEEDS_LINKS,
    NEEDS_TYPES,
    VER_METHOD_TAGS,
)

project = str(CONFIG["project"]["name"])
html_title = f"{project} Documentation"

extensions = [
    "myst_parser",
    "sphinx_needs",
    "need_tag_check",
]

_graphviz_mode = CONFIG["build"]["graphviz"]
_has_graphviz = _graphviz_mode == "on" or (
    _graphviz_mode == "auto" and shutil.which("dot") is not None
)
if _has_graphviz:
    extensions.append("sphinx.ext.graphviz")


def setup(app: object) -> None:
    """Expose Graphviz availability as a Sphinx tag for conditional flows."""
    if _has_graphviz and hasattr(app, "tags"):
        app.tags.add("has_graphviz")

myst_enable_extensions = ["colon_fence"]

exclude_patterns = [
    "README.md",
    "_build",
    "Thumbs.db",
    ".DS_Store",
    *CODE_TAG_EXCLUDE,
]

html_theme = "alabaster"

needs_types = NEEDS_TYPES
needs_id_regex = NEEDS_ID_REGEX
needs_id_required = True
needs_links = NEEDS_LINKS
needs_fields = {
    "status": {
        "schema": {
            "enum": ALL_STATUSES,
        },
    },
}
needs_role_need_template = "{{ id }}"

_inactive_repr = repr(list(INACTIVE_STATUSES))
_active_guard = f"status not in {_inactive_repr}"
_ver_tags_repr = repr(VER_METHOD_TAGS)
_dec_types = sorted(ENABLED_DECISION_TYPES)
_model_types_no_plan = sorted(
    {"sys", "swr", "arc", "icd", "comp", "ver", "con", *_dec_types}
)

needs_warnings: dict[str, str] = {
    "unverified": (
        f"type in ['sys', 'swr'] and {_active_guard} "
        f"and '{INFORMATIONAL_TAG}' not in tags and len(verifies_back) == 0"
    ),
    "uncovered_swr": (
        f"type == 'swr' and {_active_guard} and len(derives) == 0"
    ),
    "requirement_satisfies": (
        f"type in ['sys', 'swr'] and {_active_guard} and len(satisfies) > 0"
    ),
    "floating": (
        f"type in ['arc', 'icd', 'comp'] and {_active_guard} "
        "and len(satisfies) == 0 and len(refines) == 0"
    ),
    "orphan_ver": (
        f"type == 'ver' and {_active_guard} and len(verifies) == 0"
    ),
    "ver_method_tag": (
        f"type == 'ver' and {_active_guard} "
        f"and len([t for t in tags if t in {_ver_tags_repr}]) != 1"
    ),
    "missing_status": "not status",
    "model_references_planning": (
        f"type in {_model_types_no_plan!r} and {_active_guard} "
        "and len([t for t in traces "
        "if t.startswith('TSK_') or t.startswith('RDM_')]) > 0"
    ),
    "superseded_without_successor": (
        f"type in {_dec_types!r} and status == 'superseded' "
        "and len(supersedes_back) == 0"
    ),
}

if "bdr" in ENABLED_DECISION_TYPES:
    needs_warnings["bdr_missing_downstream"] = (
        "type == 'bdr' and status == 'accepted' "
        "and len(drives) == 0 and len(governs) == 0"
    )
    needs_warnings["sys_missing_upstream_bdr"] = (
        "type == 'sys' and status == 'active' and len(drives_back) == 0"
    )

needs_warnings_always_warn = True

needtable_preserve_order_docs = [
    "records/HISTORY",
    "records/TASKS",
    "records/ROADMAP",
]

needtable_reverse_docs = (
    [
        "00-overview/wdr/index",
        "10-product/bdr/index",
        "30-architecture/adr/index",
    ]
    if CONFIG["id"]["style"] == "number"
    else []
)
