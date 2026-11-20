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


if __name__ == "__main__":
    unittest.main()
