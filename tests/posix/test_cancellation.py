##
# @file     test_cancellation.py
# @brief    Gate 3 call-graph check for POSIX.1-2024 thread cancellation points
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify mandatory cancellation points check pending cancel state safely."""

# need:: VER_LIBC_0004 VER_DOCS_0004

from __future__ import annotations

import argparse
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "posix"))

from audit_lib import build_call_graph, reachable  # noqa: E402

DEFAULT_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"

MANDATORY_CANCELLATION_POINTS: tuple[str, ...] = (
    "aio_suspend",
    "clock_nanosleep",
    "close",
    "creat",
    "nanosleep",
    "open",
    "openat",
    "poll",
    "ppoll",
    "pthread_cond_clockwait",
    "pthread_cond_timedwait",
    "pthread_cond_wait",
    "pthread_join",
    "pthread_testcancel",
    "read",
    "sem_clockwait",
    "sem_timedwait",
    "sem_wait",
    "sleep",
    "tcdrain",
    "wait",
    "waitpid",
    "write",
)

NON_CANCELLATION_POINTS: tuple[str, ...] = (
    "pthread_mutex_lock",
    "pthread_mutex_trylock",
    "pthread_mutex_unlock",
    "pthread_spin_lock",
    "pthread_spin_unlock",
    "sem_post",
    "sigaction",
    "sigprocmask",
    "sigpending",
    "fork",
    "getpid",
    "getppid",
    "clock_gettime",
    "lseek",
    "ftruncate",
    "stat",
    "fstat",
    "mkdir",
    "unlink",
    "rmdir",
    "chdir",
)

CANCEL_CHECK_TARGETS: frozenset[str] = frozenset(
    {"__libc_cancel_point", "pthread_exit"}
)

FORBIDDEN_IN_CANCEL_HELPER: frozenset[str] = frozenset(
    {
        "malloc",
        "free",
        "calloc",
        "realloc",
        "reallocarray",
        "flockfile",
        "ftrylockfile",
        "funlockfile",
        "pthread_mutex_lock",
        "pthread_mutex_unlock",
    }
)

_OBJDUMP = "llvm-objdump"
_LIBC_PATH = DEFAULT_LIBC


class CancellationCallGraphTests(unittest.TestCase):
    """Gate 3b: verify XSH 2.9.5 cancellation call-graph invariants."""

    def test_mandatory_cancellation_points_check_cancel_state(self) -> None:
        self.assertTrue(_LIBC_PATH.is_file(), f"missing {_LIBC_PATH}")
        defined, edges = build_call_graph(_OBJDUMP, _LIBC_PATH)

        self.assertIn("__libc_cancel_point", defined)
        helper_reach = reachable("__libc_cancel_point", edges)
        self.assertIn(
            "pthread_exit",
            helper_reach,
            "__libc_cancel_point must terminate via pthread_exit",
        )
        bad_helper = sorted(helper_reach & FORBIDDEN_IN_CANCEL_HELPER)
        self.assertEqual(
            bad_helper,
            [],
            f"__libc_cancel_point reaches unsafe callees: "
            f"{', '.join(bad_helper)}",
        )

        for fn in MANDATORY_CANCELLATION_POINTS:
            with self.subTest(cancellation_point=fn):
                self.assertIn(fn, defined, f"missing symbol '{fn}' in libc.a")
                reach = reachable(fn, edges)
                self.assertTrue(
                    bool(reach & CANCEL_CHECK_TARGETS),
                    f"mandatory cancellation point '{fn}' does not reach "
                    f"__libc_cancel_point or pthread_exit",
                )

    def test_non_cancellation_points_do_not_check_cancel(self) -> None:
        self.assertTrue(_LIBC_PATH.is_file(), f"missing {_LIBC_PATH}")
        defined, edges = build_call_graph(_OBJDUMP, _LIBC_PATH)
        for fn in NON_CANCELLATION_POINTS:
            with self.subTest(non_cancellation_point=fn):
                self.assertIn(fn, defined, f"missing symbol '{fn}' in libc.a")
                reach = reachable(fn, edges)
                self.assertNotIn(
                    "__libc_cancel_point",
                    reach,
                    f"non-cancellation function '{fn}' must not call "
                    f"__libc_cancel_point",
                )


def main(argv: list[str] | None = None) -> int:
    global _OBJDUMP, _LIBC_PATH
    parser = argparse.ArgumentParser(
        description="Audit libc.a call graph for POSIX cancellation points."
    )
    parser.add_argument("--objdump", default="llvm-objdump")
    parser.add_argument("--libc", default=str(DEFAULT_LIBC))
    args, _ = parser.parse_known_args(argv)
    _OBJDUMP = args.objdump or "llvm-objdump"
    _LIBC_PATH = Path(args.libc)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        CancellationCallGraphTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
