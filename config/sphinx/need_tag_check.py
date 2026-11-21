##
# @file     need_tag_check.py
# @brief    Sphinx extension for in-code tag checks and need-code-trace table
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Sphinx extension validating in-code tags and rendering code trace tables."""

# need:: COMP_DOCS_0002

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
from typing import Any

from docutils import nodes
from docutils.parsers.rst import Directive
from needs_model import (
    CODE_ALLOWED_PREFIXES,
    CONFIG,
    INACTIVE_STATUSES,
    INFORMATIONAL_TAG,
    REPO_ROOT,
    TEST_ALLOWED_PREFIXES,
)
from sphinx.application import Sphinx
from sphinx.util import logging
from sphinx_needs.data import SphinxNeedsData
from tag_scan import TagScanResult, scan_repository

LOGGER = logging.getLogger(__name__)


def _id_prefix(need_id: str) -> str:
    return need_id.partition("_")[0]


class NeedCodeTraceDirective(Directive):
    """Render a table of item IDs and their `file:line` source locations."""

    has_content = False

    def run(self) -> list[nodes.Node]:
        result: TagScanResult = scan_repository(REPO_ROOT)
        repo_url = str(CONFIG["project"].get("repo_url", "")).rstrip("/")
        by_id: dict[str, list[tuple[str, int]]] = defaultdict(list)
        for occ in result.tags:
            for item_id in occ.ids:
                by_id[item_id].append((occ.file, occ.line))

        if not by_id:
            para = nodes.paragraph(
                text="No in-code need:: traceability tags were found."
            )
            return [para]

        table = nodes.table()
        tgroup = nodes.tgroup(cols=2)
        table += tgroup
        tgroup += nodes.colspec(colwidth=35)
        tgroup += nodes.colspec(colwidth=65)

        thead = nodes.thead()
        header_row = nodes.row()
        for heading in ("Need ID", "Source Locations"):
            entry = nodes.entry()
            entry += nodes.paragraph(text=heading)
            header_row += entry
        thead += header_row
        tgroup += thead

        tbody = nodes.tbody()
        for item_id in sorted(by_id):
            row = nodes.row()
            id_entry = nodes.entry()
            id_entry += nodes.paragraph(text=item_id)
            row += id_entry

            loc_entry = nodes.entry()
            loc_para = nodes.paragraph()
            locations = sorted(set(by_id[item_id]))
            for idx, (rel_file, line_no) in enumerate(locations):
                if idx > 0:
                    loc_para += nodes.Text(", ")
                label = f"{rel_file}:{line_no}"
                if repo_url:
                    ref_uri = f"{repo_url}/blob/main/{rel_file}#L{line_no}"
                    loc_para += nodes.reference(
                        "", label, refuri=ref_uri
                    )
                else:
                    loc_para += nodes.literal(text=label)
            loc_entry += loc_para
            row += loc_entry
            tbody += row

        tgroup += tbody
        return [table]


def find_untagged_active_needs(
    needs_view: Any, tagged_ids: set[str]
) -> list[tuple[str, str]]:
    """Return `(need_id, message)` for active ICD/COMP/VER without code tags."""
    inactive = set(INACTIVE_STATUSES)
    missing: list[tuple[str, str]] = []
    for need_id in sorted(needs_view.keys()):
        need = needs_view[need_id]
        status = str(need.get("status", ""))
        if status in inactive:
            continue
        ntype = str(need.get("type", ""))
        tags = list(need.get("tags", []))
        if ntype in ("icd", "comp"):
            if INFORMATIONAL_TAG not in tags and need_id not in tagged_ids:
                missing.append(
                    (
                        need_id,
                        f"active {ntype} '{need_id}' has no in-code need:: tag",
                    )
                )
        elif ntype == "ver":
            if "analysis" not in tags and need_id not in tagged_ids:
                missing.append(
                    (
                        need_id,
                        f"active automated ver '{need_id}' has no in-code "
                        "need:: tag in a test file",
                    )
                )
    return missing


def _check_tags_on_build_finished(
    app: Sphinx, exception: Exception | None
) -> None:
    if exception is not None:
        return
    if getattr(app, "statuscode", 0) != 0:
        return
    if getattr(app, "_need_tag_check_done", False):
        return
    app._need_tag_check_done = True  # type: ignore[attr-defined]

    needs_view = SphinxNeedsData(app.env).get_needs_view()
    known_ids = set(needs_view.keys())

    result = scan_repository(REPO_ROOT)
    test_files_with_ver: set[str] = set()
    tagged_ids: set[str] = set()

    for occ in result.tags:
        loc = f"{occ.file}:{occ.line}"
        if not occ.ids:
            LOGGER.warning(
                "need_tag_check: %s: need:: tag has no ID",
                loc,
            )
            continue
        for item_id in occ.ids:
            if item_id not in known_ids:
                LOGGER.warning(
                    "need_tag_check: %s: unknown need ID '%s'",
                    loc,
                    item_id,
                )
                continue
            prefix = _id_prefix(item_id)
            if prefix in TEST_ALLOWED_PREFIXES:
                if not occ.is_test:
                    LOGGER.warning(
                        "need_tag_check: %s: VER_ ID '%s' is only permitted "
                        "in test files",
                        loc,
                        item_id,
                    )
                else:
                    test_files_with_ver.add(occ.file)
                    tagged_ids.add(item_id)
            elif prefix not in CODE_ALLOWED_PREFIXES:
                LOGGER.warning(
                    "need_tag_check: %s: ID '%s' is not permitted in code "
                    "(only ARC_, ICD_, COMP_, and VER_ in tests are allowed)",
                    loc,
                    item_id,
                )
            else:
                tagged_ids.add(item_id)

    for test_file in result.test_files:
        if test_file not in test_files_with_ver:
            LOGGER.warning(
                "need_tag_check: %s: test file has no VER_ traceability tag",
                test_file,
            )

    for _, message in find_untagged_active_needs(needs_view, tagged_ids):
        LOGGER.warning("need_tag_check: %s", message)

    for rel, line_no, pid in result.planning_refs:
        LOGGER.warning(
            "need_tag_check: %s:%d: transient planning ID '%s' must not be "
            "referenced in source code",
            rel,
            line_no,
            pid,
        )


def _extract_row_need_id(row: nodes.Node) -> str | None:
    """Return the first-cell need ID text of a rendered `needtable` row."""
    for entry in row.findall(nodes.entry):
        text = entry.astext().strip()
        if text:
            return text
    return None


def _order_needtable_rows(
    app: Sphinx, doctree: nodes.document, fromdocname: str
) -> None:
    """Preserve source-file order or reverse rows on configured record pages."""
    if app.builder.format != "html":
        return
    preserve_docs = set(app.config.needtable_preserve_order_docs)
    reverse_docs = set(app.config.needtable_reverse_docs)
    if fromdocname not in preserve_docs and fromdocname not in reverse_docs:
        return

    needs_view = SphinxNeedsData(app.env).get_needs_view()

    for table in doctree.findall(nodes.table):
        classes = table.get("classes", [])
        if not any(name.startswith("NEEDS_") for name in classes):
            continue
        for tbody in table.findall(nodes.tbody):
            if fromdocname in preserve_docs:
                tbody.children = sorted(
                    tbody.children,
                    key=lambda row: _row_source_key(
                        row, needs_view, fromdocname
                    ),
                )
            elif fromdocname in reverse_docs:
                tbody.children = list(reversed(tbody.children))


def _row_source_key(
    row: nodes.Node, needs_view: Any, docname: str
) -> tuple[int, int, str]:
    need_id = _extract_row_need_id(row) or ""
    need = needs_view.get(need_id) if need_id in needs_view else None
    if need is not None and need.get("docname") == docname:
        lineno = need.get("lineno")
        if isinstance(lineno, int):
            return (0, lineno, need_id)
    return (1, 0, need_id)


def setup(app: Sphinx) -> dict[str, Any]:
    """Register directives, table-order preservation, and tag checks."""
    app.add_config_value("needtable_preserve_order_docs", [], "env")
    app.add_config_value("needtable_reverse_docs", [], "env")
    app.add_directive("need-code-trace", NeedCodeTraceDirective)
    app.connect("doctree-resolved", _order_needtable_rows)
    app.connect("build-finished", _check_tags_on_build_finished)
    return {
        "version": "1.1.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
