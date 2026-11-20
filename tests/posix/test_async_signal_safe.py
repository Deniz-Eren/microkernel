##
# @file     test_async_signal_safe.py
# @brief    Gate 3 call-graph check for POSIX.1-2024 async-signal-safe libc API
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify POSIX async-signal-safe libc functions avoid unsafe callees."""

# need:: VER_SIG_0003

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import unittest
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"

ASYNC_SIGNAL_SAFE_FNS: tuple[str, ...] = (
    "_exit",
    "write",
    "read",
    "sigaction",
    "sigprocmask",
    "pthread_sigmask",
    "kill",
    "pthread_kill",
    "raise",
    "sigemptyset",
    "sigfillset",
    "sigaddset",
    "sigdelset",
    "sigismember",
    "__errno_location",
)

FORBIDDEN_CALLEES: frozenset[str] = frozenset(
    {
        "malloc",
        "free",
        "calloc",
        "printf",
        "snprintf",
        "puts",
        "putchar",
        "exit",
    }
)

FUNC_HDR_RE = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:\s*$")
CALL_RE = re.compile(r"\b(?:callq?|jmpq?)\s+.*<([^+>]+)(?:\+0x[0-9a-fA-F]+)?>")
RELOC_RE = re.compile(r"\bR_X86_64_\w+\s+([A-Za-z_][A-Za-z0-9_.$]*)")

_OBJDUMP = "llvm-objdump"
_LIBC_PATH = DEFAULT_LIBC


def _build_call_graph(libc_a: Path) -> tuple[set[str], dict[str, set[str]]]:
    proc = subprocess.run(
        [_OBJDUMP, "-dr", str(libc_a)],
        check=True,
        capture_output=True,
        text=True,
    )
    defined: set[str] = set()
    edges: dict[str, set[str]] = defaultdict(set)
    current: str | None = None
    for line in proc.stdout.splitlines():
        m_fn = FUNC_HDR_RE.match(line)
        if m_fn:
            current = m_fn.group(1)
            defined.add(current)
            continue
        if current is None:
            continue
        m_reloc = RELOC_RE.search(line)
        if m_reloc:
            sym = m_reloc.group(1)
            if sym != current and not sym.startswith("."):
                edges[current].add(sym)
            continue
        m_call = CALL_RE.search(line)
        if m_call:
            callee = m_call.group(1)
            if callee != current:
                edges[current].add(callee)
    return defined, edges


def _reachable(start: str, edges: dict[str, set[str]]) -> set[str]:
    seen: set[str] = set()
    stack = [start]
    while stack:
        fn = stack.pop()
        for nxt in edges.get(fn, ()):
            if nxt not in seen:
                seen.add(nxt)
                stack.append(nxt)
    return seen


class AsyncSignalSafetyTests(unittest.TestCase):
    """Gate 3: verify async-signal-safe functions never reach unsafe calls."""

    def test_async_signal_safe_call_graph(self) -> None:
        self.assertTrue(_LIBC_PATH.is_file(), f"missing {_LIBC_PATH}")
        defined, edges = _build_call_graph(_LIBC_PATH)
        self.assertIn(
            "__errno_location",
            edges.get("sigaction", set()),
            "relocation edge sigaction -> __errno_location not captured",
        )
        for fn in ASYNC_SIGNAL_SAFE_FNS:
            with self.subTest(function=fn):
                self.assertIn(fn, defined, f"missing symbol '{fn}' in libc.a")
                reach = _reachable(fn, edges)
                bad = sorted(reach & FORBIDDEN_CALLEES)
                self.assertEqual(
                    bad,
                    [],
                    f"async-signal-safe '{fn}' reaches forbidden callees: "
                    f"{', '.join(bad)}",
                )


def main(argv: list[str] | None = None) -> int:
    global _OBJDUMP, _LIBC_PATH
    parser = argparse.ArgumentParser(
        description="Audit libc.a call graph for POSIX async-signal-safety."
    )
    parser.add_argument("--objdump", default="llvm-objdump")
    parser.add_argument("--libc", default=str(DEFAULT_LIBC))
    args, _ = parser.parse_known_args(argv)
    _OBJDUMP = args.objdump or "llvm-objdump"
    _LIBC_PATH = Path(args.libc)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        AsyncSignalSafetyTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
