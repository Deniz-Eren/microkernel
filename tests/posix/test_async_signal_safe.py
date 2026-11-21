##
# @file     test_async_signal_safe.py
# @brief    Gate 3 call-graph check for POSIX.1-2024 async-signal-safe libc API
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Verify POSIX async-signal-safe libc functions avoid unsafe callees."""

# need:: VER_SIG_0003 VER_DOCS_0004

from __future__ import annotations

import argparse
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "posix"))

from audit_lib import build_call_graph, reachable  # noqa: E402

DEFAULT_LIBC = ROOT / "sysroot" / "usr" / "lib" / "libc.a"

ASYNC_SIGNAL_SAFE_FNS: tuple[str, ...] = (
    "_exit",
    "_Exit",
    "write",
    "read",
    "getentropy",
    "close",
    "dup",
    "dup2",
    "dup3",
    "pipe",
    "pipe2",
    "fcntl",
    "poll",
    "ppoll",
    "fork",
    "execve",
    "execv",
    "getpid",
    "getppid",
    "sigaction",
    "sigprocmask",
    "sigpending",
    "pthread_sigmask",
    "kill",
    "pthread_kill",
    "raise",
    "sigemptyset",
    "sigfillset",
    "sigaddset",
    "sigdelset",
    "sigismember",
    "wait",
    "waitpid",
    "clock_gettime",
    "timer_getoverrun",
    "timer_gettime",
    "timer_settime",
    "sem_post",
    "aio_error",
    "aio_return",
    "aio_suspend",
    "open",
    "openat",
    "creat",
    "lseek",
    "ftruncate",
    "stat",
    "fstat",
    "lstat",
    "fstatat",
    "mkdir",
    "mkdirat",
    "unlink",
    "unlinkat",
    "rmdir",
    "chdir",
    "fchdir",
    "umask",
    "access",
    "cfgetispeed",
    "cfgetospeed",
    "cfsetispeed",
    "cfsetospeed",
    "tcdrain",
    "tcflow",
    "tcflush",
    "tcgetattr",
    "tcsendbreak",
    "tcsetattr",
    "__errno_location",
)

FORBIDDEN_CALLEES: frozenset[str] = frozenset(
    {
        "malloc",
        "free",
        "calloc",
        "realloc",
        "reallocarray",
        "printf",
        "fprintf",
        "snprintf",
        "puts",
        "fputs",
        "putchar",
        "fputc",
        "fflush",
        "flockfile",
        "ftrylockfile",
        "funlockfile",
        "pthread_mutex_lock",
        "pthread_mutex_unlock",
        "exit",
    }
)

_OBJDUMP = "llvm-objdump"
_LIBC_PATH = DEFAULT_LIBC


class AsyncSignalSafetyTests(unittest.TestCase):
    """Gate 3a: verify async-signal-safe functions never reach unsafe calls."""

    def test_async_signal_safe_call_graph(self) -> None:
        self.assertTrue(_LIBC_PATH.is_file(), f"missing {_LIBC_PATH}")
        defined, edges = build_call_graph(_OBJDUMP, _LIBC_PATH)
        self.assertIn(
            "__errno_location",
            edges.get("sigaction", set()),
            "relocation edge sigaction -> __errno_location not captured",
        )
        for fn in ASYNC_SIGNAL_SAFE_FNS:
            with self.subTest(function=fn):
                self.assertIn(fn, defined, f"missing symbol '{fn}' in libc.a")
                reach = reachable(fn, edges)
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
