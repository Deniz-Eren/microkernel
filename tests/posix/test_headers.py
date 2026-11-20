##
# @file     test_headers.py
# @brief    POSIX.1-2024 standalone header matrix and namespace hygiene test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Standalone C17 header compilation and namespace gate for POSIX.1-2024."""

# need:: VER_LIBC_0002 VER_SIG_0003

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SYSROOT_INC = ROOT / "sysroot" / "usr" / "include"

POSIX_HEADERS: tuple[str, ...] = (
    "errno.h",
    "signal.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "unistd.h",
    "sys/types.h",
)

FEATURE_PROFILES: tuple[tuple[str, str], ...] = (
    ("POSIX.1-2024 Base", "-D_POSIX_C_SOURCE=202405L"),
    ("XSI Issue 8", "-D_XOPEN_SOURCE=800"),
)

_COMPILER = "clang"
_CLANG_INC: Path | None = None
_SYSROOT: Path = SYSROOT_INC


def _detect_clang_include(cc: str) -> Path:
    proc = subprocess.run(
        [cc, "-print-resource-dir"],
        check=True,
        capture_output=True,
        text=True,
    )
    return Path(proc.stdout.strip()) / "include"


def _compile_snippet(source: str, extra_flags: list[str]) -> tuple[int, str]:
    clang_inc = _CLANG_INC or _detect_clang_include(_COMPILER)
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".c", encoding="utf-8", delete=True
    ) as tmp:
        tmp.write(source)
        tmp.flush()
        cmd = [
            _COMPILER,
            "-target",
            "x86_64-unknown-none-elf",
            "-ffreestanding",
            "-nostdinc",
            "-isystem",
            str(clang_inc),
            "-isystem",
            str(_SYSROOT),
            "-std=c17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pedantic",
            "-fsyntax-only",
            *extra_flags,
            tmp.name,
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        return proc.returncode, proc.stderr.strip()


class PosixHeaderMatrixTests(unittest.TestCase):
    """Gate 1: standalone header compilation and namespace hygiene checks."""

    def test_standalone_header_matrix(self) -> None:
        for hdr in POSIX_HEADERS:
            for label, macro in FEATURE_PROFILES:
                with self.subTest(header=hdr, profile=label):
                    src = f"#include <{hdr}>\nint __hdr_ok = 0;\n"
                    rc, err = _compile_snippet(src, [macro])
                    self.assertEqual(
                        rc,
                        0,
                        f"<{hdr}> failed under {macro}:\n{err}",
                    )

    def test_unistd_feature_macros_and_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <unistd.h>",
                '_Static_assert(_POSIX_VERSION == 202405L, "_POSIX_VERSION");',
                '_Static_assert(_POSIX2_VERSION == 202405L, "_POSIX2_VER");',
                '_Static_assert(STDIN_FILENO == 0, "STDIN_FILENO");',
                '_Static_assert(STDOUT_FILENO == 1, "STDOUT_FILENO");',
                '_Static_assert(STDERR_FILENO == 2, "STDERR_FILENO");',
                "#ifdef SYS_DEBUG_LOG",
                '#error "unistd.h leaked SYS_DEBUG_LOG"',
                "#endif",
                "#ifdef SYS_CHANNEL_CREATE",
                '#error "unistd.h leaked SYS_CHANNEL_CREATE"',
                "#endif",
                "#ifdef SYS_PROC_EXIT",
                '#error "unistd.h leaked SYS_PROC_EXIT"',
                "#endif",
                "#ifdef BOOTINFO_MAGIC",
                '#error "unistd.h leaked BOOTINFO_MAGIC"',
                "#endif",
                "int __unistd_hygiene_ok = 0;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"unistd hygiene failed:\n{err}")

    def test_errno_lvalue_and_required_constants(self) -> None:
        src = "\n".join(
            (
                "#include <errno.h>",
                "static int __check_errno_lvalue(void) {",
                "    int *p = &errno;",
                "    *p = EINVAL;",
                "    return errno + EPERM + ENOENT + ESRCH + EINTR + EIO +",
                "           EBADF + EAGAIN + ENOMEM + EACCES + EFAULT +",
                "           EBUSY + EEXIST + ENOSYS + ERANGE + ETIMEDOUT +",
                "           ECANCELED + ENOTSUP;",
                "}",
                "int (*__errno_fn_ref)(void) = __check_errno_lvalue;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"errno check failed:\n{err}")

    def test_signal_header_api_and_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <signal.h>",
                "#ifdef SYS_SIGACTION",
                '#error "signal.h leaked SYS_SIGACTION"',
                "#endif",
                "#ifdef SYS_DEBUG_LOG",
                '#error "signal.h leaked SYS_DEBUG_LOG"',
                "#endif",
                "static int __check_signal_api(void) {",
                "    sig_atomic_t flag = 0;",
                "    sigset_t s;",
                "    struct sigaction sa;",
                "    sa.sa_handler = SIG_DFL;",
                "    sa.sa_mask = 0;",
                "    sa.sa_flags = SA_NODEFER | SA_RESETHAND;",
                "    (void)sigemptyset(&s);",
                "    (void)sigfillset(&s);",
                "    (void)sigaddset(&s, SIGUSR1);",
                "    (void)sigdelset(&s, SIGUSR2);",
                "    (void)sigismember(&s, SIGKILL);",
                "    (void)sigaction(SIGUSR1, &sa, (struct sigaction *)0);",
                "    (void)sigprocmask(SIG_BLOCK, &s, &s);",
                "    (void)pthread_sigmask(SIG_SETMASK, &s, &s);",
                "    (void)kill((pid_t)0, 0);",
                "    (void)pthread_kill((pthread_t)0, 0);",
                "    (void)raise(0);",
                "    (void)signal(SIGSTOP, SIG_IGN);",
                "    return (int)flag;",
                "}",
                "int (*__sig_fn_ref)(void) = __check_signal_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"signal.h check failed:\n{err}")


def main(argv: list[str] | None = None) -> int:
    global _COMPILER, _CLANG_INC, _SYSROOT
    parser = argparse.ArgumentParser(
        description="Run POSIX.1-2024 standalone header compilation gate."
    )
    parser.add_argument("--cc", default="clang", help="C compiler executable.")
    parser.add_argument(
        "--clang-include", default=None, help="Clang resource include dir."
    )
    parser.add_argument(
        "--sysroot",
        default=str(SYSROOT_INC),
        help="Sysroot usr/include directory.",
    )
    args, remaining = parser.parse_known_args(argv)
    _COMPILER = args.cc
    _CLANG_INC = Path(args.clang_include) if args.clang_include else None
    _SYSROOT = Path(args.sysroot)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(
        PosixHeaderMatrixTests
    )
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
