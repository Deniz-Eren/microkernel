##
# @file     test_headers.py
# @brief    POSIX.1-2024 standalone header matrix and namespace hygiene test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Standalone C17 header compilation and namespace gate for POSIX.1-2024."""

# need:: VER_LIBC_0002 VER_SIG_0003 VER_MEM_0002
# need:: VER_PROC_0002 VER_PROC_0003 VER_PROC_0004 VER_PROC_0005 VER_IPC_0003

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
    "fcntl.h",
    "poll.h",
    "pthread.h",
    "semaphore.h",
    "signal.h",
    "stdio.h",
    "stdlib.h",
    "string.h",
    "time.h",
    "unistd.h",
    "sys/mman.h",
    "sys/types.h",
    "sys/wait.h",
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

    def test_mman_and_wait_headers_and_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <sys/mman.h>",
                "#include <sys/wait.h>",
                "#ifdef SYS_VM_MAP",
                '#error "sys/mman.h leaked SYS_VM_MAP"',
                "#endif",
                "#ifdef SYS_PROC_WAIT",
                '#error "sys/wait.h leaked SYS_PROC_WAIT"',
                "#endif",
                "static int __check_mman_wait_api(void) {",
                "    void *p = mmap((void *)0, 4096, PROT_READ | PROT_WRITE,",
                "                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);",
                "    if (p == MAP_FAILED) { return -1; }",
                "    (void)mprotect(p, 4096, PROT_READ);",
                "    (void)msync(p, 4096, MS_SYNC);",
                "    (void)munmap(p, 4096);",
                "    int st = 0;",
                "    (void)wait(&st);",
                "    (void)waitpid((pid_t)-1, &st, WNOHANG);",
                "    return WIFEXITED(st) ? WEXITSTATUS(st) : WTERMSIG(st);",
                "}",
                "int (*__mw_fn_ref)(void) = __check_mman_wait_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"mman/wait check failed:\n{err}")

    def test_time_header_api_and_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <time.h>",
                "#ifdef SYS_CLOCK_GETTIME",
                '#error "time.h leaked SYS_CLOCK_GETTIME"',
                "#endif",
                "#ifdef SYS_TIMER_CREATE",
                '#error "time.h leaked SYS_TIMER_CREATE"',
                "#endif",
                "static int __check_time_api(void) {",
                "    struct timespec ts = {0, 0};",
                "    struct itimerspec its = {{0, 0}, {0, 0}};",
                "    struct sigevent ev = {SIGEV_SIGNAL, 0, {0}};",
                "    timer_t tid = 0;",
                "    (void)clock_getres(CLOCK_MONOTONIC, &ts);",
                "    (void)clock_gettime(CLOCK_REALTIME, &ts);",
                "    (void)clock_settime(CLOCK_REALTIME, &ts);",
                "    (void)clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,",
                "                          &ts, (struct timespec *)0);",
                "    (void)nanosleep(&ts, &ts);",
                "    (void)timer_create(CLOCK_MONOTONIC, &ev, &tid);",
                "    (void)timer_settime(tid, 0, &its, &its);",
                "    (void)timer_gettime(tid, &its);",
                "    (void)timer_getoverrun(tid);",
                "    return timer_delete(tid);",
                "}",
                "int (*__time_fn_ref)(void) = __check_time_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"time.h check failed:\n{err}")

    def test_pthread_and_semaphore_headers_and_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <pthread.h>",
                "#include <semaphore.h>",
                "#ifdef SYS_FUTEX",
                '#error "pthread.h/semaphore.h leaked SYS_FUTEX"',
                "#endif",
                "#ifdef SYS_THREAD_SET_FSBASE",
                '#error "pthread.h leaked SYS_THREAD_SET_FSBASE"',
                "#endif",
                "static int __check_pthread_sem_api(void) {",
                "    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;",
                "    pthread_cond_t c = PTHREAD_COND_INITIALIZER;",
                "    pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;",
                "    pthread_once_t o = PTHREAD_ONCE_INIT;",
                "    pthread_barrier_t b;",
                "    pthread_spinlock_t sp;",
                "    sem_t s;",
                "    struct timespec ts = {0, 0};",
                "    (void)pthread_mutex_lock(&m);",
                "    (void)pthread_mutex_unlock(&m);",
                "    (void)pthread_cond_clockwait(&c, &m,",
                "                                 CLOCK_MONOTONIC, &ts);",
                "    (void)pthread_cond_broadcast(&c);",
                "    (void)pthread_barrier_init(&b,",
                "        (pthread_barrierattr_t *)0, 1);",
                "    (void)pthread_barrier_wait(&b);",
                "    (void)pthread_spin_init(&sp, PTHREAD_PROCESS_PRIVATE);",
                "    (void)pthread_rwlock_rdlock(&rw);",
                "    (void)pthread_rwlock_unlock(&rw);",
                "    (void)sem_init(&s, 0, 1u);",
                "    (void)sem_clockwait(&s, CLOCK_MONOTONIC, &ts);",
                "    (void)sem_post(&s);",
                "    return (int)o;",
                "}",
                "int (*__ps_fn_ref)(void) = __check_pthread_sem_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"pthread/sem check failed:\n{err}")

    def test_fcntl_poll_and_fork_pipe_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <fcntl.h>",
                "#include <poll.h>",
                "#include <unistd.h>",
                "#ifdef SYS_FD_OP",
                '#error "fcntl/poll/unistd leaked SYS_FD_OP"',
                "#endif",
                "#ifdef SYS_PROC_FORK",
                '#error "unistd.h leaked SYS_PROC_FORK"',
                "#endif",
                "#ifdef PIPE_OP_CREATE",
                '#error "poll.h/fcntl.h leaked PIPE_OP_CREATE"',
                "#endif",
                "static int __check_fork_pipe_api(void) {",
                "    int fds[2] = {0, 0};",
                "    struct pollfd p = {0, POLLIN | POLLOUT, 0};",
                "    struct timespec ts = {0, 0};",
                "    sigset_t mask = 0;",
                "    char *const argv[] = {(char *)0};",
                "    char *const envp[] = {(char *)0};",
                "    (void)pipe(fds);",
                "    (void)pipe2(fds, O_CLOEXEC | O_NONBLOCK);",
                "    (void)dup(fds[0]);",
                "    (void)dup2(fds[0], fds[1]);",
                "    (void)dup3(fds[0], fds[1], O_CLOEXEC);",
                "    (void)fcntl(fds[0], F_GETFD);",
                "    (void)fcntl(fds[0], F_SETFD, FD_CLOEXEC);",
                "    (void)fcntl(fds[0], F_DUPFD_CLOEXEC, 3);",
                "    (void)poll(&p, (nfds_t)1, 0);",
                "    (void)ppoll(&p, (nfds_t)1, &ts, &mask);",
                "    (void)sigpending(&mask);",
                "    (void)getpid();",
                "    (void)getppid();",
                "    (void)fork();",
                "    (void)execve(\"/bin/sh\", argv, envp);",
                "    return close(fds[0]);",
                "}",
                "int (*__fp_fn_ref)(void) = __check_fork_pipe_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"fcntl/poll/unistd failed:\n{err}")


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
