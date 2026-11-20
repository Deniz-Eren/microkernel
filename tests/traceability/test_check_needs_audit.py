##
# @file     test_check_needs_audit.py
# @brief    Unit tests for check_needs_audit.py model validation rules
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Unit tests for `tools/traceability/check_needs_audit.py`."""

# need:: VER_POWER_0003

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "traceability"))

from check_needs_audit import audit_needs  # noqa: E402


def _make_valid_model() -> dict[str, dict[str, object]]:
    return {
        "BDR_MAGIC_LINKS": {
            "type": "bdr",
            "status": "accepted",
            "docname": "10-product/bdr/BDR_MAGIC_LINKS",
            "lineno": 5,
            "tags": [],
            "drives": ["SYS_POWER_SUPPLY"],
        },
        "SYS_POWER_SUPPLY": {
            "type": "sys",
            "status": "active",
            "docname": "20-system/power",
            "lineno": 5,
            "tags": [],
            "drives_back": ["BDR_MAGIC_LINKS"],
            "derives_back": ["SWR_POWER_MONITOR"],
            "verifies_back": ["VER_POWER_SUPPLY"],
        },
        "SWR_POWER_MONITOR": {
            "type": "swr",
            "status": "active",
            "docname": "20-system/power",
            "lineno": 15,
            "tags": [],
            "derives": ["SYS_POWER_SUPPLY"],
            "verifies_back": ["VER_POWER_MONITOR"],
        },
        "VER_POWER_SUPPLY": {
            "type": "ver",
            "status": "active",
            "docname": "60-verification/power",
            "lineno": 5,
            "tags": ["integration"],
            "verifies": ["SYS_POWER_SUPPLY"],
        },
        "VER_POWER_MONITOR": {
            "type": "ver",
            "status": "active",
            "docname": "60-verification/power",
            "lineno": 15,
            "tags": ["unit"],
            "verifies": ["SWR_POWER_MONITOR"],
        },
    }


class CheckNeedsAuditTests(unittest.TestCase):
    """Tests for `audit_needs` traceability checks."""

    def test_valid_chain_passes(self) -> None:
        self.assertEqual(audit_needs(_make_valid_model()), [])

    def test_disallowed_satisfies_between_requirements_fails(self) -> None:
        model = _make_valid_model()
        model["SWR_POWER_MONITOR"]["satisfies"] = ["SYS_POWER_SUPPLY"]
        model["SYS_POWER_SUPPLY"]["satisfies_back"] = ["SWR_POWER_MONITOR"]
        findings = audit_needs(model)
        self.assertTrue(
            any("disallowed satisfies" in m for _, _, m in findings)
        )

    def test_verifies_to_component_fails(self) -> None:
        model = _make_valid_model()
        model["COMP_POWER_MONITOR"] = {
            "type": "comp",
            "status": "active",
            "docname": "50-components/power",
            "lineno": 5,
            "tags": [],
            "satisfies": ["SWR_POWER_MONITOR"],
            "verifies_back": ["VER_POWER_MONITOR"],
        }
        model["SWR_POWER_MONITOR"]["satisfies_back"] = ["COMP_POWER_MONITOR"]
        model["VER_POWER_MONITOR"]["verifies"] = [
            "SWR_POWER_MONITOR",
            "COMP_POWER_MONITOR",
        ]
        findings = audit_needs(model)
        self.assertTrue(any("disallowed verifies" in m for _, _, m in findings))

    def test_wrong_domain_filename_fails(self) -> None:
        model = _make_valid_model()
        model["SYS_POWER_SUPPLY"]["docname"] = "20-system/thermal"
        findings = audit_needs(model)
        self.assertTrue(
            any("does not match filename" in m for _, _, m in findings)
        )

    def test_wrong_decision_filename_fails(self) -> None:
        model = _make_valid_model()
        model["BDR_MAGIC_LINKS"]["docname"] = "10-product/bdr/BDR_WRONG"
        findings = audit_needs(model)
        self.assertTrue(any("decision filename" in m for _, _, m in findings))


if __name__ == "__main__":
    unittest.main()
