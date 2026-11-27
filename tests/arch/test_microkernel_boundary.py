##
# @file     test_microkernel_boundary.py
# @brief    CTest gate enforcing the Ring 0 microkernel boundary allowlist
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

# need:: VER_DOCS_0007
"""Automated microkernel Ring 0 boundary gate (CNV_0007 / WDR_0010).

Verifies that kernel/core/*.c modules, SYS_* syscalls in
libc/include/sys/microkernel.h, and fields in struct __k_fd_entry match
tests/arch/microkernel_allowlist.json and cite accepted ADRs.
"""

import json
from pathlib import Path
import re
import sys
import unittest

_ROOT = Path(__file__).resolve().parents[2]
_ALLOWLIST = Path(__file__).resolve().parent / "microkernel_allowlist.json"
_KERNEL_CORE = _ROOT / "kernel" / "core"
_MICROKERNEL_H = _ROOT / "libc" / "include" / "sys" / "microkernel.h"
_FD_H = _ROOT / "libc" / "include" / "sys" / "fd.h"
_ADR_DIR = _ROOT / "docs" / "30-architecture" / "adr"

_SYS_RE = re.compile(r"^#define\s+(SYS_[A-Z0-9_]+)\s+(\d+)\b", re.MULTILINE)
_FD_STRUCT_RE = re.compile(r"struct\s+__k_fd_entry\s*\{([^}]*)\};", re.DOTALL)
_FIELD_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;")
_DEBUG_LOG_CALL_RE = re.compile(r"\bSYS_DEBUG_LOG\b")
_SYS_DEBUG_LOG_ALLOWED = frozenset(
    {
        "libc/include/sys/microkernel.h",
        "libc/ldso/ldso.c",
        "libc/src/syslog.c",
        "libc/src/tty.c",
    }
)


class MicrokernelBoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data = json.loads(_ALLOWLIST.read_text(encoding="utf-8"))

    def test_kernel_core_modules_match_allowlist(self) -> None:
        actual = sorted(p.name for p in _KERNEL_CORE.glob("*.c"))
        allowed = sorted(self.data["kernel_core_modules"].keys())
        extra = sorted(set(actual) - set(allowed))
        missing = sorted(set(allowed) - set(actual))
        self.assertEqual(
            extra,
            [],
            "Unregistered Ring 0 module(s) in kernel/core/: "
            f"{extra}. Implement in Ring 3 (libc/ or servers/) per CNV_0007, "
            "or justify in an accepted ADR_ and update "
            "tests/arch/microkernel_allowlist.json.",
        )
        self.assertEqual(
            missing,
            [],
            f"Stale kernel/core/ entries in microkernel_allowlist.json: "
            f"{missing}.",
        )

    def test_syscalls_match_allowlist(self) -> None:
        text = _MICROKERNEL_H.read_text(encoding="utf-8")
        actual = {name: int(num) for name, num in _SYS_RE.findall(text)}
        allowed = {
            name: int(meta["number"])
            for name, meta in self.data["syscalls"].items()
        }
        extra = sorted(set(actual) - set(allowed))
        missing = sorted(set(allowed) - set(actual))
        self.assertEqual(
            extra,
            [],
            f"Unregistered SYS_* syscall(s): {extra}. Compose in Ring 3 per "
            "CNV_0007, or justify in an accepted ADR_ and update "
            "tests/arch/microkernel_allowlist.json.",
        )
        self.assertEqual(
            missing,
            [],
            f"Stale SYS_* entries in microkernel_allowlist.json: {missing}.",
        )
        self.assertEqual(
            actual,
            allowed,
            "SYS_* syscall numbers drifted from microkernel_allowlist.json.",
        )

    def test_all_referenced_adrs_exist_and_are_accepted(self) -> None:
        adrs = set(self.data["kernel_core_modules"].values())
        for meta in self.data["syscalls"].values():
            adrs.add(meta["adr"])
        for adr in sorted(adrs):
            self.assertRegex(adr, r"^ADR_\d{4}$")
            path = _ADR_DIR / f"{adr}.md"
            self.assertTrue(
                path.is_file(),
                f"Allowlist cites missing ADR file {path.relative_to(_ROOT)}.",
            )
            content = path.read_text(encoding="utf-8")
            self.assertIn(
                ":status: accepted",
                content,
                f"Allowlist cites {adr} without ':status: accepted'.",
            )

    def test_fd_entry_stays_minimal(self) -> None:
        text = _FD_H.read_text(encoding="utf-8")
        m = _FD_STRUCT_RE.search(text)
        self.assertIsNotNone(m, "struct __k_fd_entry not found in sys/fd.h.")
        body = m.group(1)
        fields = [
            f.group(1)
            for line in body.splitlines()
            if (f := _FIELD_RE.search(line))
        ]
        allowed = self.data["fd_entry_allowed_fields"]
        self.assertEqual(
            fields,
            allowed,
            "struct __k_fd_entry drifted from minimal Ring 0 FD dispatch "
            f"fields ({allowed}): got {fields}. Keep paths and server state "
            "in Ring 3 per CNV_0007.",
        )

    def test_sys_debug_log_restricted_to_fallbacks(self) -> None:
        violations: list[str] = []
        for sub in ("bin", "servers", "libc"):
            for path in sorted((_ROOT / sub).rglob("*")):
                if path.suffix not in (".c", ".h") or not path.is_file():
                    continue
                rel = path.relative_to(_ROOT).as_posix()
                text = path.read_text(encoding="utf-8", errors="ignore")
                if not _DEBUG_LOG_CALL_RE.search(text):
                    continue
                if rel not in _SYS_DEBUG_LOG_ALLOWED:
                    violations.append(rel)
                elif rel == "libc/ldso/ldso.c":
                    # Only the fatal fallback inside ldso_exit may call
                    # SYS_DEBUG_LOG; normal ldd output must use fd 1/fd 2.
                    calls = re.findall(r"__sys2\s*\(\s*SYS_DEBUG_LOG\b", text)
                    self.assertLessEqual(
                        len(calls),
                        1,
                        "libc/ldso/ldso.c has more than one SYS_DEBUG_LOG "
                        "call; route ldd output through ldso_fd_write(1/2).",
                    )
        self.assertEqual(
            violations,
            [],
            "User-space files must write to fd 1/fd 2 (stdout/stderr), not "
            f"SYS_DEBUG_LOG directly: {violations}",
        )


if __name__ == "__main__":
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
