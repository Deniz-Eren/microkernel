##
# @file     test_tag_scan.py
# @brief    Unit tests for tag_scan.py in-code need:: tag extraction
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Unit tests for `config/sphinx/tag_scan.py`."""

# need:: VER_DOCS_0003

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "config" / "sphinx"))

from need_tag_check import find_untagged_active_needs  # noqa: E402
from readme_include import rewrite_readme_for_docs  # noqa: E402
from tag_scan import extract_tags_from_line, scan_repository  # noqa: E402


class TagScanTests(unittest.TestCase):
    """Tests for comment-marker-anchored `need::` extraction."""

    def test_extracts_valid_comment_tags(self) -> None:
        self.assertEqual(
            extract_tags_from_line("# need:: COMP_IPC_0001", ("#",)),
            ("COMP_IPC_0001",),
        )
        self.assertEqual(
            extract_tags_from_line(
                "  /* need:: ARC_IPC_0001 ICD_IPC_0001 */",
                ("//", "/*"),
            ),
            ("ARC_IPC_0001", "ICD_IPC_0001"),
        )
        self.assertEqual(
            extract_tags_from_line(
                "/** \\req{COMP_IPC_0001} */",
                ("//", "/*"),
            ),
            ("COMP_IPC_0001",),
        )
        self.assertEqual(
            extract_tags_from_line("# need::", ("#",)),
            (),
        )

    def test_ignores_prose_mentions_of_need_syntax(self) -> None:
        self.assertIsNone(
            extract_tags_from_line(
                "# Use need:: COMP_IPC_0001 for links", ("#",)
            )
        )
        self.assertIsNone(
            extract_tags_from_line('text = "need:: COMP_IPC_0001"', ("#",))
        )

    def test_scan_repository_detects_test_files_and_tags(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src").mkdir()
            (root / "tests").mkdir()
            (root / "src" / "mod.py").write_text(
                "# need:: COMP_IPC_0001\nx = 1\n", encoding="utf-8"
            )
            (root / "tests" / "test_mod.py").write_text(
                "# need:: VER_IPC_0001\n", encoding="utf-8"
            )
            result = scan_repository(root)
            self.assertEqual(len(result.tags), 2)
            self.assertEqual(result.test_files, ("tests/test_mod.py",))

    def test_bidirectional_coverage_flags_untagged_active_items(self) -> None:
        needs = {
            "ICD_IPC_0001": {"type": "icd", "status": "active", "tags": []},
            "COMP_IPC_0001": {"type": "comp", "status": "active", "tags": []},
            "COMP_IPC_0002": {"type": "comp", "status": "draft", "tags": []},
            "VER_IPC_0001": {
                "type": "ver",
                "status": "active",
                "tags": ["unit"],
            },
            "VER_IPC_0002": {
                "type": "ver",
                "status": "active",
                "tags": ["analysis"],
            },
        }
        missing = find_untagged_active_needs(needs, {"ICD_IPC_0001"})
        ids = [need_id for need_id, _ in missing]
        self.assertEqual(ids, ["COMP_IPC_0001", "VER_IPC_0001"])

    def test_scan_repository_flags_transient_planning_ids_in_code(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src").mkdir()
            (root / "src" / "mod.c").write_text(
                "/* need:: COMP_IPC_0001 */\n"
                "/* TSK_0007 and RDM_0003 should be flagged */\n",
                encoding="utf-8",
            )
            result = scan_repository(root)
            self.assertEqual(
                result.planning_refs,
                (
                    ("src/mod.c", 2, "TSK_0007"),
                    ("src/mod.c", 2, "RDM_0003"),
                ),
            )


    def test_rewrite_readme_links_for_sphinx_index(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "docs" / "records").mkdir(parents=True)
            (root / "docs" / "skills" / "cmake").mkdir(parents=True)
            (root / "docs" / "index.md").write_text(
                "# Index\n", encoding="utf-8"
            )
            (root / "docs" / "records" / "TASKS.md").write_text(
                "# Tasks\n", encoding="utf-8"
            )
            (root / "docs" / "skills" / "cmake" / "SKILL.md").write_text(
                "# Skill\n", encoding="utf-8"
            )
            (root / "AGENTS.md").write_text("# Agents\n", encoding="utf-8")

            src = (
                "See [TASKS](docs/records/TASKS.md), "
                "[index](docs/index.md), "
                "[AGENTS.md](AGENTS.md), and "
                "[cmake\nskill](docs/skills/cmake/SKILL.md).\n\n"
                "```mermaid\n"
                "flowchart TD\n"
                '    A["[keep](docs/records/TASKS.md)"]\n'
                "```\n"
            )
            rewritten = rewrite_readme_for_docs(
                src,
                repo_root=root,
                docs_dir=root / "docs",
                exclude_patterns=["README.md", "skills"],
                repo_url="",
            )
            self.assertIn("[TASKS](records/TASKS.md)", rewritten)
            self.assertIn("[index](#traceability-documentation)", rewritten)
            self.assertIn("`AGENTS.md`", rewritten)
            self.assertIn("(`docs/skills/cmake/SKILL.md`)", rewritten)
            self.assertIn('A["[keep](docs/records/TASKS.md)"]', rewritten)


if __name__ == "__main__":
    unittest.main()
