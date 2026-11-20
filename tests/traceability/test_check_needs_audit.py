##
# @file     test_check_needs_audit.py
# @brief    Unit tests for check_needs_audit.py model validation rules
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Unit tests for `tools/traceability/check_needs_audit.py`."""

# need:: VER_DOCS_0002

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "traceability"))

from check_needs_audit import audit_needs  # noqa: E402


def _make_valid_model() -> dict[str, dict[str, object]]:
    return {
        "BDR_0002": {
            "type": "bdr",
            "status": "accepted",
            "docname": "10-product/bdr/BDR_0002",
            "lineno": 5,
            "tags": [],
            "drives": ["SYS_PROC_0001"],
        },
        "SYS_PROC_0001": {
            "type": "sys",
            "status": "active",
            "docname": "20-system/proc",
            "lineno": 5,
            "tags": [],
            "drives_back": ["BDR_0002"],
            "derives_back": ["SWR_PROC_0001"],
            "verifies_back": ["VER_PROC_0001"],
        },
        "SWR_PROC_0001": {
            "type": "swr",
            "status": "active",
            "docname": "20-system/proc",
            "lineno": 15,
            "tags": [],
            "derives": ["SYS_PROC_0001"],
            "verifies_back": ["VER_PROC_0002"],
        },
        "VER_PROC_0001": {
            "type": "ver",
            "status": "active",
            "docname": "60-verification/proc",
            "lineno": 5,
            "tags": ["integration"],
            "verifies": ["SYS_PROC_0001"],
        },
        "VER_PROC_0002": {
            "type": "ver",
            "status": "active",
            "docname": "60-verification/proc",
            "lineno": 15,
            "tags": ["unit"],
            "verifies": ["SWR_PROC_0001"],
        },
    }


class CheckNeedsAuditTests(unittest.TestCase):
    """Tests for `audit_needs` traceability checks."""

    def test_valid_chain_passes(self) -> None:
        self.assertEqual(audit_needs(_make_valid_model()), [])

    def test_disallowed_satisfies_between_requirements_fails(self) -> None:
        model = _make_valid_model()
        model["SWR_PROC_0001"]["satisfies"] = ["SYS_PROC_0001"]
        model["SYS_PROC_0001"]["satisfies_back"] = ["SWR_PROC_0001"]
        findings = audit_needs(model)
        self.assertTrue(
            any("disallowed satisfies" in m for _, _, m in findings)
        )

    def test_verifies_to_component_fails(self) -> None:
        model = _make_valid_model()
        model["COMP_PROC_0001"] = {
            "type": "comp",
            "status": "active",
            "docname": "50-components/proc",
            "lineno": 5,
            "tags": [],
            "satisfies": ["SWR_PROC_0001"],
            "verifies_back": ["VER_PROC_0002"],
        }
        model["SWR_PROC_0001"]["satisfies_back"] = ["COMP_PROC_0001"]
        model["VER_PROC_0002"]["verifies"] = [
            "SWR_PROC_0001",
            "COMP_PROC_0001",
        ]
        findings = audit_needs(model)
        self.assertTrue(any("disallowed verifies" in m for _, _, m in findings))

    def test_wrong_domain_filename_fails(self) -> None:
        model = _make_valid_model()
        model["SYS_PROC_0001"]["docname"] = "20-system/mem"
        findings = audit_needs(model)
        self.assertTrue(
            any("does not match filename" in m for _, _, m in findings)
        )

    def test_wrong_decision_filename_fails(self) -> None:
        model = _make_valid_model()
        model["BDR_0002"]["docname"] = "10-product/bdr/BDR_9999"
        findings = audit_needs(model)
        self.assertTrue(any("decision filename" in m for _, _, m in findings))


if __name__ == "__main__":
    unittest.main()
