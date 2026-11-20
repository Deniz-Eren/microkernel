##
# @file     check_needs_audit.py
# @brief    Audit exported needs.json against all traceability model rules
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Standalone auditor for exported Sphinx-Needs `needs.json`."""

# need:: VER_POWER_0001

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
SPHINX_CFG_DIR = REPO_ROOT / "config" / "sphinx"
if str(SPHINX_CFG_DIR) not in sys.path:
    sys.path.insert(0, str(SPHINX_CFG_DIR))

from needs_model import (  # noqa: E402
    ALLOWED_LINK_MATRIX,
    ENABLED_DECISION_TYPES,
    INACTIVE_STATUSES,
    INFORMATIONAL_TAG,
    NEEDS_LINKS,
    PLANNING_TYPES,
    VER_METHOD_TAGS,
)

DOMAIN_LAYER_DIRS = frozenset(
    {
        "20-system",
        "30-architecture",
        "40-interfaces",
        "50-components",
        "60-verification",
    }
)
DOMAIN_SCOPED_TYPES = frozenset({"sys", "swr", "arc", "icd", "comp", "ver"})
SKIP_DOMAIN_FILENAMES = frozenset({"index", "traceability", "code-trace"})


def _load_needs_map(needs_json_path: Path) -> dict[str, dict[str, Any]]:
    data = json.loads(needs_json_path.read_text(encoding="utf-8"))
    versions = data.get("versions", {})
    if not versions:
        return {}
    current_ver = data.get("current_version")
    if current_ver and current_ver in versions:
        return dict(versions[current_ver].get("needs", {}))
    first_key = next(iter(versions))
    return dict(versions[first_key].get("needs", {}))


def _doc_rel_path(need: dict[str, Any]) -> str:
    docname = str(need.get("docname", ""))
    return f"docs/{docname}.md" if docname else "<unknown>"


def _need_line(need: dict[str, Any]) -> int:
    ln = need.get("lineno")
    return int(ln) if isinstance(ln, int) and ln > 0 else 1


def _extract_domain(need_id: str) -> str | None:
    parts = need_id.split("_")
    if len(parts) < 3:
        return None
    return parts[1]


def audit_needs(needs: dict[str, dict[str, Any]]) -> list[tuple[str, int, str]]:
    """Audit needs dictionary and return list of `(file, line, message)`."""
    findings: list[tuple[str, int, str]] = []
    link_names = list(NEEDS_LINKS.keys())
    inactive = set(INACTIVE_STATUSES)
    ver_tags_set = set(VER_METHOD_TAGS)
    domains_by_doc: dict[str, tuple[int, set[str]]] = {}

    for need_id, need in sorted(needs.items()):
        ntype = str(need.get("type", ""))
        status = str(need.get("status", ""))
        tags = list(need.get("tags", []))
        doc_path = _doc_rel_path(need)
        docname = str(need.get("docname", ""))
        line_no = _need_line(need)

        # 1-3. Forward links, back-links, missing targets, and link matrix
        for link in link_names:
            targets = list(need.get(link, []))
            back_key = f"{link}_back"
            allowed_pairs = ALLOWED_LINK_MATRIX.get(link, frozenset())
            for target_id in targets:
                if target_id not in needs:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: {link} targets missing ID "
                            f"'{target_id}'",
                        )
                    )
                    continue
                target = needs[target_id]
                target_type = str(target.get("type", ""))
                if (ntype, target_type) not in allowed_pairs:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: disallowed {link} link from "
                            f"'{ntype}' to '{target_type}' ({target_id})",
                        )
                    )
                target_back = list(target.get(back_key, []))
                if need_id not in target_back:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: {link} -> {target_id} missing "
                            f"back-link in {back_key}",
                        )
                    )

            for src_id in list(need.get(back_key, [])):
                if src_id not in needs:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: {back_key} references missing ID "
                            f"'{src_id}'",
                        )
                    )
                elif need_id not in list(needs[src_id].get(link, [])):
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: {back_key} <- {src_id} has no "
                            f"matching forward {link}",
                        )
                    )

        # 4. Coverage, method tags, and planning direction
        if status not in inactive:
            if (
                ntype in ("sys", "swr")
                and INFORMATIONAL_TAG not in tags
                and not need.get("verifies_back")
            ):
                findings.append(
                    (
                        doc_path,
                        line_no,
                        f"{need_id}: active {ntype} has no verifying item",
                    )
                )
            if ntype == "swr" and not need.get("derives"):
                findings.append(
                    (
                        doc_path,
                        line_no,
                        f"{need_id}: active swr has no derives link",
                    )
                )
            if (
                ntype in ("arc", "icd", "comp")
                and not need.get("satisfies")
                and not need.get("refines")
            ):
                findings.append(
                    (
                        doc_path,
                        line_no,
                        f"{need_id}: floating {ntype} has neither satisfies "
                        "nor refines link",
                    )
                )
            if ntype == "ver":
                if not need.get("verifies"):
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: orphan ver has no verifies link",
                        )
                    )
                method_tags = [t for t in tags if t in ver_tags_set]
                if len(method_tags) != 1:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: ver must have exactly 1 method tag "
                            f"(found {len(method_tags)})",
                        )
                    )
            if "bdr" in ENABLED_DECISION_TYPES:
                if (
                    ntype == "bdr"
                    and status == "accepted"
                    and not need.get("drives")
                    and not need.get("governs")
                ):
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: accepted bdr has no drives or "
                            "governs link",
                        )
                    )
                if (
                    ntype == "sys"
                    and status == "active"
                    and not need.get("drives_back")
                ):
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: active sys has no upstream bdr "
                            "(drives_back)",
                        )
                    )

            if ntype not in PLANNING_TYPES:
                for target_id in list(need.get("traces", [])):
                    if target_id.startswith(("TSK_", "RDM_")):
                        findings.append(
                            (
                                doc_path,
                                line_no,
                                f"{need_id}: normative/record item must not "
                                f"trace planning entry '{target_id}'",
                            )
                        )

        if (
            ntype in ENABLED_DECISION_TYPES
            and status == "superseded"
            and not need.get("supersedes_back")
        ):
            findings.append(
                (
                    doc_path,
                    line_no,
                    f"{need_id}: superseded decision has no successor",
                )
            )

        # 5. Missing parent item (supports both parent_need and parent_needs)
        parents: list[str] = []
        single_parent = need.get("parent_need")
        if isinstance(single_parent, str) and single_parent:
            parents.append(single_parent)
        for multi_parent in need.get("parent_needs") or []:
            if isinstance(multi_parent, str) and multi_parent not in parents:
                parents.append(multi_parent)
        for parent in parents:
            if parent not in needs:
                findings.append(
                    (
                        doc_path,
                        line_no,
                        f"{need_id}: parent_need '{parent}' does not exist",
                    )
                )

        # 6. Domain file check
        doc_parts = Path(docname).parts if docname else ()
        if (
            ntype in DOMAIN_SCOPED_TYPES
            and len(doc_parts) == 2
            and doc_parts[0] in DOMAIN_LAYER_DIRS
            and doc_parts[1] not in SKIP_DOMAIN_FILENAMES
        ):
            dom = _extract_domain(need_id)
            if dom is not None:
                entry = domains_by_doc.setdefault(docname, (line_no, set()))
                entry[1].add(dom)
                if dom.lower() != doc_parts[1]:
                    findings.append(
                        (
                            doc_path,
                            line_no,
                            f"{need_id}: domain '{dom}' does not match "
                            f"filename '{doc_parts[1]}.md'",
                        )
                    )

        # 7. Decision filename check
        if ntype in ENABLED_DECISION_TYPES and doc_parts:
            stem = doc_parts[-1]
            if stem != need_id:
                findings.append(
                    (
                        doc_path,
                        line_no,
                        f"{need_id}: decision filename '{stem}.md' does not "
                        f"match ID '{need_id}'",
                    )
                )

    for docname, (first_line, doms) in sorted(domains_by_doc.items()):
        if len(doms) > 1:
            findings.append(
                (
                    f"docs/{docname}.md",
                    first_line,
                    f"document mixes multiple domains: "
                    f"{', '.join(sorted(doms))}",
                )
            )

    return sorted(findings)


def main(argv: list[str] | None = None) -> int:
    """CLI entry point returning 0 (clean), 1 (findings), or 2 (error)."""
    parser = argparse.ArgumentParser(
        description="Audit exported Sphinx-Needs needs.json."
    )
    parser.add_argument(
        "--needs-json",
        required=True,
        help="Path to build/docs/needs/needs.json.",
    )
    args = parser.parse_args(argv)
    needs_path = Path(args.needs_json)
    if not needs_path.is_file():
        print(f"error: needs.json not found: {needs_path}", file=sys.stderr)
        return 2
    try:
        needs = _load_needs_map(needs_path)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"error: failed to read {needs_path}: {exc}", file=sys.stderr)
        return 2

    findings = audit_needs(needs)
    for file_path, line_no, msg in findings:
        print(f"{file_path}:{line_no}: {msg}")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
