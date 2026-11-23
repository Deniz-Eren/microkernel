##
# @file     test_headers.py
# @brief    POSIX.1-2024 standalone header matrix and namespace hygiene test
#
# @copyright
# Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
# SPDX-License-Identifier: MIT
#

"""Standalone C17 header compilation and namespace gate for POSIX.1-2024."""

# need:: VER_LIBC_0002 VER_LIBC_0004 VER_LIBC_0005 VER_LIBC_0006 VER_LIBC_0008
# need:: VER_LIBC_0009 VER_LIBC_0010 VER_SIG_0003 VER_MEM_0002 VER_MEM_0004
# need:: VER_PROC_0002 VER_PROC_0003 VER_PROC_0004 VER_PROC_0005 VER_PROC_0006
# need:: VER_IPC_0003 VER_IPC_0004 VER_IPC_0005 VER_IPC_0006 VER_IPC_0007
# need:: VER_DOCS_0004 VER_BOOT_0003 VER_PROC_0008 VER_PROC_0009 VER_IPC_0010
# need:: VER_LIBC_0011 VER_LIBC_0012 VER_LIBC_0013 VER_LIBC_0014 VER_LIBC_0015
# need:: VER_LIBC_0016

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
    "aio.h",
    "assert.h",
    "complex.h",
    "cpio.h",
    "ctype.h",
    "dirent.h",
    "endian.h",
    "errno.h",
    "fcntl.h",
    "fenv.h",
    "float.h",
    "fnmatch.h",
    "glob.h",
    "grp.h",
    "iconv.h",
    "inttypes.h",
    "iso646.h",
    "langinfo.h",
    "libintl.h",
    "limits.h",
    "locale.h",
    "math.h",
    "monetary.h",
    "nl_types.h",
    "poll.h",
    "pthread.h",
    "pwd.h",
    "regex.h",
    "sched.h",
    "semaphore.h",
    "setjmp.h",
    "signal.h",
    "stdalign.h",
    "stdarg.h",
    "stdatomic.h",
    "stdbool.h",
    "stddef.h",
    "stdint.h",
    "stdio.h",
    "stdlib.h",
    "stdnoreturn.h",
    "string.h",
    "strings.h",
    "tar.h",
    "termios.h",
    "tgmath.h",
    "threads.h",
    "time.h",
    "uchar.h",
    "unistd.h",
    "wchar.h",
    "wctype.h",
    "wordexp.h",
    "sys/mman.h",
    "sys/resource.h",
    "sys/select.h",
    "sys/socket.h",
    "sys/stat.h",
    "sys/statvfs.h",
    "sys/times.h",
    "sys/types.h",
    "sys/un.h",
    "sys/utsname.h",
    "sys/wait.h",
)

FEATURE_PROFILES: tuple[tuple[str, str], ...] = (
    ("POSIX.1-2024 Base", "-D_POSIX_C_SOURCE=202405L"),
    ("XSI Issue 8", "-D_XOPEN_SOURCE=800"),
)

_COMPILER = "clang"
_TARGET = "x86_64-unknown-none-elf"
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
            _TARGET,
            "-ffreestanding",
            "-nostdinc",
            "-isystem",
            str(_SYSROOT),
            "-isystem",
            str(clang_inc),
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
                "#ifdef SYS_SHM_OP",
                '#error "sys/mman.h leaked SYS_SHM_OP"',
                "#endif",
                "#ifdef SYS_PROC_WAIT",
                '#error "sys/wait.h leaked SYS_PROC_WAIT"',
                "#endif",
                "#if !defined(_POSIX_SHARED_MEMORY_OBJECTS) || \\",
                "    !defined(_POSIX_MEMLOCK) || \\",
                "    !defined(_POSIX_MEMLOCK_RANGE)",
                '#error "sys/mman.h missing SHM/ML/MLR feature macros"',
                "#endif",
                "static int __check_mman_wait_api(void) {",
                "    void *p = mmap((void *)0, 4096, PROT_READ | PROT_WRITE,",
                "                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);",
                "    if (p == MAP_FAILED) { return -1; }",
                "    (void)mprotect(p, 4096, PROT_READ);",
                "    (void)msync(p, 4096, MS_SYNC);",
                "    (void)mlock(p, 4096);",
                "    (void)munlock(p, 4096);",
                "    (void)mlockall(MCL_CURRENT | MCL_FUTURE);",
                "    (void)munlockall();",
                "    (void)shm_open(\"/x\", 0, (mode_t)0600);",
                "    (void)shm_unlink(\"/x\");",
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

    def test_aio_header_and_optional_msg_gate(self) -> None:
        src = "\n".join(
            (
                "#include <aio.h>",
                "#include <fcntl.h>",
                "#include <unistd.h>",
                "#ifdef SYS_FUTEX",
                '#error "aio.h leaked SYS_FUTEX"',
                "#endif",
                "#ifdef _POSIX_MESSAGE_PASSING",
                '#error "_POSIX_MESSAGE_PASSING must stay gated until MSG"',
                "#endif",
                "static int __check_aio_api(void) {",
                "    struct aiocb cb = {0};",
                "    const struct aiocb *cl[1] = {&cb};",
                "    struct aiocb *ml[1] = {&cb};",
                "    struct timespec ts = {0, 0};",
                "    struct sigevent sev = {SIGEV_NONE, 0, {0}};",
                "    cb.aio_lio_opcode = LIO_NOP;",
                "    (void)aio_read(&cb);",
                "    (void)aio_write(&cb);",
                "    (void)aio_fsync(O_SYNC, &cb);",
                "    (void)aio_fsync(O_DSYNC, &cb);",
                "    (void)aio_error(&cb);",
                "    (void)aio_return(&cb);",
                "    (void)aio_cancel(0, &cb);",
                "    (void)aio_suspend(cl, 1, &ts);",
                "    return lio_listio(LIO_WAIT, ml, 1, &sev) + AIO_ALLDONE +",
                "           AIO_CANCELED + AIO_NOTCANCELED + LIO_READ +",
                "           LIO_WRITE + LIO_NOWAIT;",
                "}",
                "int (*__aio_fn_ref)(void) = __check_aio_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"aio.h check failed:\n{err}")

    def test_cancellation_and_stdio_locking_headers(self) -> None:
        src = "\n".join(
            (
                "#include <pthread.h>",
                "#include <stdio.h>",
                "static void __noop_cleanup(void *p) { (void)p; }",
                "static int __check_cancel_stdio(void) {",
                "    int st = 0;",
                "    int ty = 0;",
                "    (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,",
                "                                 &st);",
                "    (void)pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,",
                "                                &ty);",
                "    pthread_cleanup_push(__noop_cleanup, (void *)0);",
                "    pthread_testcancel();",
                "    pthread_cleanup_pop(0);",
                "    (void)pthread_cancel((pthread_t)0);",
                "    flockfile(stdout);",
                "    (void)ftrylockfile(stderr);",
                "    (void)fprintf(stdout, \"%d\", EOF);",
                "    (void)fputs(\"\", stdout);",
                "    (void)fputc('x', stdout);",
                "    (void)putchar_unlocked('x');",
                "    (void)fflush(stdout);",
                "    funlockfile(stdout);",
                "    return PTHREAD_CANCEL_ENABLE +",
                "           PTHREAD_CANCEL_ASYNCHRONOUS +",
                "           (PTHREAD_CANCELED != (void *)0);",
                "}",
                "int (*__cs_fn_ref)(void) = __check_cancel_stdio;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"cancel/stdio check failed:\n{err}")

    def test_issue8_base_functions_and_c17_headers(self) -> None:
        src = "\n".join(
            (
                "#include <stdalign.h>",
                "#include <stdatomic.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "#include <uchar.h>",
                "#include <unistd.h>",
                "#ifdef SYS_GETENTROPY",
                '#error "unistd.h leaked SYS_GETENTROPY"',
                "#endif",
                "static int __check_issue8_libc(void) {",
                "    char buf[16] = {0};",
                "    mbstate_t st = {0};",
                "    char16_t c16 = 0;",
                "    char32_t c32 = 0;",
                "    alignas(8) atomic_int a = 0;",
                "    (void)strlcpy(buf, \"x\", sizeof(buf));",
                "    (void)strlcat(buf, \"y\", sizeof(buf));",
                "    (void)reallocarray((void *)0, 1u, 1u);",
                "    (void)getenv(\"PATH\");",
                "    (void)secure_getenv(\"PATH\");",
                "    (void)getentropy(buf, 4u);",
                "    (void)mbrtoc16(&c16, buf, 1u, &st);",
                "    (void)c16rtomb(buf, c16, &st);",
                "    (void)mbrtoc32(&c32, buf, 1u, &st);",
                "    (void)c32rtomb(buf, c32, &st);",
                "    return atomic_load(&a) + GETENTROPY_MAX;",
                "}",
                "int (*__i8_fn_ref)(void) = __check_issue8_libc;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"issue8 libc check failed:\n{err}")

    def test_vfs_stat_and_dirent_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <dirent.h>",
                "#include <fcntl.h>",
                "#include <sys/stat.h>",
                "#include <unistd.h>",
                "#ifdef VFS_OP_OPEN",
                '#error "VFS headers leaked VFS_OP_OPEN"',
                "#endif",
                "#ifdef SYS_FD_OP",
                '#error "VFS headers leaked SYS_FD_OP"',
                "#endif",
                "static int __check_vfs_api(void) {",
                "    struct stat st = {0};",
                "    char cwd[32] = {0};",
                "    int fd = open(\"/tmp/a\", O_CREAT | O_RDWR, 0644u);",
                "    (void)openat(AT_FDCWD, \"a\", O_RDONLY | O_DIRECTORY);",
                "    (void)creat(\"/tmp/b\", 0600u);",
                "    (void)lseek(fd, (off_t)0, SEEK_SET);",
                "    (void)ftruncate(fd, (off_t)0);",
                "    (void)stat(\"/tmp\", &st);",
                "    (void)lstat(\"/tmp\", &st);",
                "    (void)fstat(fd, &st);",
                "    (void)fstatat(AT_FDCWD, \"/tmp\", &st, 0);",
                "    (void)mkdir(\"/tmp/d\", 0755u);",
                "    (void)mkdirat(AT_FDCWD, \"/tmp/d\", 0755u);",
                "    (void)unlink(\"/tmp/a\");",
                "    (void)unlinkat(AT_FDCWD, \"/tmp/d\", AT_REMOVEDIR);",
                "    (void)rmdir(\"/tmp/d\");",
                "    (void)chdir(\"/tmp\");",
                "    (void)fchdir(fd);",
                "    (void)getcwd(cwd, sizeof(cwd));",
                "    (void)umask(0022u);",
                "    (void)access(\"/tmp\", F_OK | R_OK | W_OK | X_OK);",
                "    DIR *d = opendir(\"/tmp\");",
                "    (void)fdopendir(fd);",
                "    (void)readdir(d);",
                "    rewinddir(d);",
                "    (void)dirfd(d);",
                "    (void)closedir(d);",
                "    return S_ISREG(st.st_mode) + S_ISDIR(st.st_mode);",
                "}",
                "int (*__vfs_fn_ref)(void) = __check_vfs_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"vfs headers check failed:\n{err}")

    def test_termios_and_isatty_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <termios.h>",
                "#include <unistd.h>",
                "#ifdef TTY_OP_READ",
                '#error "termios.h leaked TTY_OP_READ"',
                "#endif",
                "#ifdef SYS_FD_OP",
                '#error "termios.h leaked SYS_FD_OP"',
                "#endif",
                "static int __check_tty_api(void) {",
                "    struct termios t = {0};",
                "    (void)tcgetattr(0, &t);",
                "    (void)cfsetispeed(&t, B115200);",
                "    (void)cfsetospeed(&t, B115200);",
                "    (void)tcsetattr(0, TCSANOW, &t);",
                "    (void)tcflush(0, TCIFLUSH);",
                "    (void)tcdrain(0);",
                "    (void)tcflow(0, TCOON);",
                "    (void)tcsendbreak(0, 0);",
                "    return isatty(0) + (int)cfgetispeed(&t) +",
                "           (int)cfgetospeed(&t);",
                "}",
                "int (*__tty_fn_ref)(void) = __check_tty_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"termios check failed:\n{err}")

    def test_buffered_stdio_and_core_c17_headers_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <assert.h>",
                "#include <ctype.h>",
                "#include <inttypes.h>",
                "#include <limits.h>",
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "#include <strings.h>",
                "#include <unistd.h>",
                "#ifdef SYS_PROC_EXECVE",
                '#error "core headers leaked SYS_PROC_EXECVE"',
                "#endif",
                "static_assert(PATH_MAX >= _POSIX_PATH_MAX, \"PATH_MAX\");",
                "static_assert(NAME_MAX >= _POSIX_NAME_MAX, \"NAME_MAX\");",
                "static_assert(PIPE_BUF >= _POSIX_PIPE_BUF, \"PIPE_BUF\");",
                "static int __check_core_stdio_api(void) {",
                "    assert(BUFSIZ >= 256);",
                "    FILE *f = fopen(\"/tmp/x\", \"r\");",
                "    (void)fdopen(0, \"r\");",
                "    (void)fseeko(f, (off_t)0, SEEK_SET);",
                "    (void)ftello(f);",
                "    (void)fileno(f);",
                "    (void)fclose(f);",
                "    (void)setenv(\"K\", \"V\", 1);",
                "    (void)unsetenv(\"K\");",
                "    char *const av[] = {\"sh\", (char *)0};",
                "    (void)execv(\"/boot/sh\", av);",
                "    (void)execvp(\"sh\", av);",
                "    (void)strdup(\"a\");",
                "    (void)strstr(\"ab\", \"b\");",
                "    (void)strerror(0);",
                "    (void)strcasecmp(\"a\", \"A\");",
                "    (void)imaxabs((intmax_t)-1);",
                "    return isalnum('A') + tolower('A') + toupper('a');",
                "}",
                "int (*__core_fn_ref)(void) = __check_core_stdio_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"core/stdio check failed:\n{err}")

    def test_socket_and_un_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <sys/socket.h>",
                "#include <sys/un.h>",
                "#ifdef SOCK_OP_CREATE",
                '#error "sys/socket.h leaked SOCK_OP_CREATE"',
                "#endif",
                "#ifdef SYS_FD_OP",
                '#error "sys/socket.h leaked SYS_FD_OP"',
                "#endif",
                "static int __check_sock_api(void) {",
                "    struct sockaddr_un un = {0};",
                "    un.sun_family = AF_UNIX;",
                "    socklen_t len = (socklen_t)sizeof(un);",
                "    int sv[2] = {-1, -1};",
                "    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);",
                "    (void)socketpair(AF_UNIX,",
                "                     SOCK_DGRAM | SOCK_NONBLOCK, 0, sv);",
                "    (void)bind(s, (const struct sockaddr *)&un, len);",
                "    (void)listen(s, SOMAXCONN);",
                "    (void)accept(s, (struct sockaddr *)&un, &len);",
                "    (void)accept4(s, (struct sockaddr *)&un, &len,",
                "                  SOCK_CLOEXEC | SOCK_NONBLOCK);",
                "    (void)connect(s, (const struct sockaddr *)&un, len);",
                "    (void)send(s, \"a\", 1, MSG_NOSIGNAL);",
                "    (void)recv(s, &un, 1, MSG_PEEK);",
                "    (void)sendto(s, \"a\", 1, 0,",
                "                 (const struct sockaddr *)&un, len);",
                "    (void)recvfrom(s, &un, 1, 0,",
                "                   (struct sockaddr *)&un, &len);",
                "    (void)shutdown(s, SHUT_RDWR);",
                "    (void)getsockname(s, (struct sockaddr *)&un, &len);",
                "    (void)getpeername(s, (struct sockaddr *)&un, &len);",
                "    int v = 1;",
                "    socklen_t vl = (socklen_t)sizeof(v);",
                "    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &v, vl);",
                "    (void)getsockopt(s, SOL_SOCKET, SO_TYPE, &v, &vl);",
                "    return s;",
                "}",
                "int (*__sock_fn_ref)(void) = __check_sock_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"socket headers check failed:\n{err}")

    def test_select_header_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <sys/select.h>",
                "#ifdef SYS_FD_OP",
                '#error "sys/select.h leaked SYS_FD_OP"',
                "#endif",
                "#ifdef SYS_SYS_STATS",
                '#error "sys/select.h leaked SYS_SYS_STATS"',
                "#endif",
                "#ifndef FD_SET",
                '#error "sys/select.h missing FD_SET"',
                "#endif",
                '_Static_assert(FD_SETSIZE >= 32, "FD_SETSIZE");',
                "static int __check_select_api(void) {",
                "    fd_set rfds;",
                "    FD_ZERO(&rfds);",
                "    FD_SET(0, &rfds);",
                "    int is_set = FD_ISSET(0, &rfds);",
                "    FD_CLR(0, &rfds);",
                "    struct timeval tv = {.tv_sec = 0, .tv_usec = 1000};",
                "    struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000};",
                "    sigset_t mask;",
                "    sigemptyset(&mask);",
                "    int r1 = select(1, &rfds, NULL, NULL, &tv);",
                "    int r2 = pselect(1, &rfds, NULL, NULL, &ts, &mask);",
                "    return is_set + r1 + r2;",
                "}",
                "int (*__select_fn_ref)(void) = __check_select_api;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"sys/select.h check failed:\n{err}")

    def test_high_leverage_base_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <endian.h>",
                "#include <float.h>",
                "#include <fnmatch.h>",
                "#include <iso646.h>",
                "#include <sched.h>",
                "#include <setjmp.h>",
                "#include <stdarg.h>",
                "#include <stdbool.h>",
                "#include <stddef.h>",
                "#include <stdint.h>",
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <stdnoreturn.h>",
                "#include <sys/times.h>",
                "#include <sys/utsname.h>",
                "#include <unistd.h>",
                "#ifdef SYS_SCHED_YIELD",
                '#error "sched.h leaked SYS_SCHED_YIELD"',
                "#endif",
                "#ifdef SYS_PROC_TIMES",
                '#error "sys/times.h leaked SYS_PROC_TIMES"',
                "#endif",
                "#ifdef SYS_ALARM",
                '#error "unistd.h leaked SYS_ALARM"',
                "#endif",
                "static void __noop_exit(void) {}",
                "static int __noop_cmp(const void *a, const void *b) {",
                "    return (a > b) - (a < b);",
                "}",
                "static int __check_base_utils(int ac, char *const av[]) {",
                "    struct utsname u;",
                "    struct tms t;",
                "    struct sched_param sp = {0};",
                "    jmp_buf jb;",
                "    sigjmp_buf sjb;",
                "    int x = 1;",
                "    (void)uname(&u);",
                "    (void)times(&t);",
                "    (void)sched_yield();",
                "    (void)htobe16((uint16_t)1u);",
                "    (void)be64toh(htobe64(1u));",
                "    (void)le32toh(htole32(1u));",
                "    if (setjmp(jb) != 0) { longjmp(jb, 1); }",
                "    if (sigsetjmp(sjb, 1) != 0) { siglongjmp(sjb, 1); }",
                "    (void)fnmatch(\"*\", \"a\", FNM_PATHNAME | FNM_PERIOD |",
                "                  FNM_NOESCAPE | FNM_IGNORECASE);",
                "    (void)atexit(__noop_exit);",
                "    qsort(&x, 1u, sizeof(x), __noop_cmp);",
                "    (void)bsearch(&x, &x, 1u, sizeof(x), __noop_cmp);",
                "    (void)getopt(ac, av, \"a:\");",
                "    (void)alarm(0u);",
                "    (void)setpgid(0, 0);",
                "    (void)getpgid(0);",
                "    (void)getpgrp();",
                "    (void)setsid();",
                "    (void)getsid(0);",
                "    (void)tcgetpgrp(0);",
                "    (void)tcsetpgrp(0, 1);",
                "    (void)rename(\"/tmp/a\", \"/tmp/b\");",
                "    (void)renameat(-1, \"a\", -1, \"b\");",
                "    (void)realpath(\"/tmp\", (char *)0);",
                "    return BYTE_ORDER + sp.sched_priority + optind + opterr +",
                "           optopt + (optarg != (char *)0);",
                "}",
                "int (*__utils_fn_ref)(int, char *const[]) =",
                "    __check_base_utils;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"base utils check failed:\n{err}")

    def test_fenv_and_math_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <fenv.h>",
                "#include <math.h>",
                "#if !defined(_XOPEN_SOURCE) && defined(M_PI)",
                '#error "math.h leaked XSI M_PI under Base profile"',
                "#endif",
                "#ifdef SYS_DEBUG_LOG",
                '#error "fenv.h/math.h leaked SYS_DEBUG_LOG"',
                "#endif",
                "static double __check_fenv_math(double x, float xf) {",
                "    fenv_t env;",
                "    fexcept_t flg = 0u;",
                "    int e = 0;",
                "    double ip = 0.0;",
                "    (void)fegetenv(&env);",
                "    (void)feholdexcept(&env);",
                "    (void)fesetround(FE_TONEAREST);",
                "    (void)fegetround();",
                "    (void)feclearexcept(FE_ALL_EXCEPT);",
                "    (void)feraiseexcept(FE_INEXACT);",
                "    (void)fetestexcept(FE_INEXACT);",
                "    (void)fegetexceptflag(&flg, FE_ALL_EXCEPT);",
                "    (void)fesetexceptflag(&flg, FE_ALL_EXCEPT);",
                "    (void)feupdateenv(&env);",
                "    (void)fesetenv(FE_DFL_ENV);",
                "    double r = fma(x, x, 1.0) + sqrt(x) + cbrt(x) +",
                "               hypot(x, 1.0) + sin(x) + cos(x) + tan(x) +",
                "               asin(x) + acos(x) + atan(x) + atan2(x, 1.0) +",
                "               sinh(x) + cosh(x) + tanh(x) + asinh(x) +",
                "               acosh(x) + atanh(x) + exp(x) + exp2(x) +",
                "               expm1(x) + log(x) + log2(x) + log10(x) +",
                "               log1p(x) + pow(x, 2.0) + erf(x) + erfc(x) +",
                "               tgamma(x) + floor(x) + ceil(x) + trunc(x) +",
                "               round(x) + rint(x) + nearbyint(x) +",
                "               fmod(x, 2.0) + remainder(x, 2.0) +",
                "               remquo(x, 2.0, &e) + modf(x, &ip) +",
                "               frexp(x, &e) + ldexp(x, e) + scalbn(x, 1) +",
                "               scalbln(x, 1L) + logb(x) + nextafter(x, 2.0) +",
                "               nexttoward(x, 2.0L) + fdim(x, 1.0) +",
                "               fmax(x, 1.0) + fmin(x, 1.0) +",
                "               copysign(x, -1.0) + (double)acosf(xf);",
                "    int c = fpclassify(x) + isfinite(x) + isinf(x) +",
                "            isnan(x) + isnormal(x) + signbit(x) +",
                "            isgreater(x, 0.0) + isgreaterequal(x, 0.0) +",
                "            isless(x, 0.0) + islessequal(x, 0.0) +",
                "            islessgreater(x, 0.0) + isunordered(x, 0.0) +",
                "            ilogb(x);",
                "    return r + (double)c;",
                "}",
                "double (*__fm_fn_ref)(double, float) = __check_fenv_math;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"fenv/math check failed:\n{err}")

    def test_sysconf_and_tty_winsize_declarations(self) -> None:
        src = "\n".join(
            (
                "#include <stdio.h>",
                "#include <termios.h>",
                "#include <unistd.h>",
                "#ifdef _SC_NPROCESSORS_ONLN",
                '#error "non-POSIX _SC_NPROCESSORS_ONLN must not be defined"',
                "#endif",
                "#ifdef SYS_DEBUG_LOG",
                '#error "unistd/termios leaked SYS_DEBUG_LOG"',
                "#endif",
                "static long __check_sysconf_tty(int fd) {",
                "    char buf[L_ctermid + 32];",
                "    struct winsize ws = {0, 0};",
                "    long v = sysconf(_SC_PAGESIZE) + sysconf(_SC_CLK_TCK) +",
                "             pathconf(\"/\", _PC_PATH_MAX) +",
                "             fpathconf(fd, _PC_PIPE_BUF);",
                "    size_t n = confstr(_CS_PATH, buf, sizeof(buf));",
                "    (void)gethostname(buf, sizeof(buf));",
                "    (void)ctermid(buf);",
                "    (void)ttyname(fd);",
                "    (void)ttyname_r(fd, buf, sizeof(buf));",
                "    (void)tcgetsid(fd);",
                "    (void)tcgetwinsize(fd, &ws);",
                "    (void)tcsetwinsize(fd, &ws);",
                "    return v + (long)n;",
                "}",
                "long (*__sc_fn_ref)(int) = __check_sysconf_tty;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"sysconf/tty check failed:\n{err}")

    def test_pwd_grp_resource_and_cred_declarations(self) -> None:
        src = "\n".join(
            (
                "#include <grp.h>",
                "#include <pwd.h>",
                "#include <sys/resource.h>",
                "#include <unistd.h>",
                "#ifdef SYS_DEBUG_LOG",
                '#error "pwd/grp/resource leaked SYS_DEBUG_LOG"',
                "#endif",
                "static int __check_cred_rlimit(void) {",
                "    struct passwd pw, *pwp = 0;",
                "    struct group gr, *grp = 0;",
                "    struct rlimit rl = {0, RLIM_INFINITY};",
                "    char buf[64];",
                "    gid_t gids[4];",
                "    (void)getpwuid(getuid());",
                "    (void)getpwnam(\"root\");",
                "    (void)getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &pwp);",
                "    (void)getpwnam_r(\"root\", &pw, buf, sizeof(buf), &pwp);",
                "    (void)getgrgid(getgid());",
                "    (void)getgrnam(\"root\");",
                "    (void)getgrgid_r(getegid(), &gr, buf, sizeof(buf), &grp);",
                "    (void)getgrnam_r(\"root\", &gr, buf, sizeof(buf), &grp);",
                "    (void)setuid(0);",
                "    (void)seteuid(0);",
                "    (void)setgid(0);",
                "    (void)setegid(0);",
                "    (void)getgroups(4, gids);",
                "    (void)getlogin();",
                "    (void)getlogin_r(buf, sizeof(buf));",
                "    (void)getrlimit(RLIMIT_NOFILE, &rl);",
                "    (void)setrlimit(RLIMIT_NOFILE, &rl);",
                "    return (int)RLIMIT_AS;",
                "}",
                "int (*__cred_fn_ref)(void) = __check_cred_rlimit;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"cred/rlimit check failed:\n{err}")

    def test_statvfs_links_fifo_getdents_declarations(self) -> None:
        src = "\n".join(
            (
                "#include <dirent.h>",
                "#include <fcntl.h>",
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <sys/stat.h>",
                "#include <sys/statvfs.h>",
                "#include <unistd.h>",
                "#ifdef SYS_DEBUG_LOG",
                '#error "statvfs/dirent leaked SYS_DEBUG_LOG"',
                "#endif",
                "static long __check_vfs_ext(int fd) {",
                "    struct statvfs sv;",
                "    struct dirent ent, *res = 0;",
                "    struct dirent **list = 0;",
                "    char buf[64];",
                "    (void)chmod(\"/tmp\", 0755);",
                "    (void)fchmod(fd, 0644);",
                "    (void)fchmodat(AT_FDCWD, \"/tmp\", 0755, 0);",
                "    (void)chown(\"/tmp\", 0, 0);",
                "    (void)fchown(fd, 0, 0);",
                "    (void)lchown(\"/tmp\", 0, 0);",
                "    (void)fchownat(AT_FDCWD, \"/tmp\", 0, 0, 0);",
                "    (void)faccessat(AT_FDCWD, \"/tmp\", F_OK, AT_EACCESS);",
                "    (void)link(\"/tmp/a\", \"/tmp/b\");",
                "    (void)linkat(AT_FDCWD, \"/tmp/a\", AT_FDCWD, \"/tmp/b\",",
                "                 AT_SYMLINK_FOLLOW);",
                "    (void)symlink(\"/tmp/a\", \"/tmp/s\");",
                "    (void)symlinkat(\"/tmp/a\", AT_FDCWD, \"/tmp/s\");",
                "    (void)readlink(\"/tmp/s\", buf, sizeof(buf));",
                "    (void)readlinkat(AT_FDCWD, \"/tmp/s\", buf, sizeof(buf));",
                "    (void)mkfifo(\"/tmp/f\", 0600);",
                "    (void)mkfifoat(AT_FDCWD, \"/tmp/f\", 0600);",
                "    (void)pread(fd, buf, sizeof(buf), 0);",
                "    (void)pwrite(fd, buf, sizeof(buf), 0);",
                "    (void)truncate(\"/tmp/a\", 0);",
                "    (void)posix_close(fd, POSIX_CLOSE_RESTART);",
                "    (void)posix_getdents(fd, buf, sizeof(buf), 0);",
                "    (void)scandir(\"/tmp\", &list, 0, alphasort);",
                "    (void)readdir_r(0, &ent, &res);",
                "    (void)statvfs(\"/tmp\", &sv);",
                "    (void)fstatvfs(fd, &sv);",
                "    (void)remove(\"/tmp/a\");",
                "    (void)mkstemp(buf);",
                "    (void)mkostemp(buf, O_CLOEXEC);",
                "    (void)mkdtemp(buf);",
                "    (void)tmpfile();",
                "    (void)tmpnam(buf);",
                "    return (long)sizeof(struct posix_dent);",
                "}",
                "long (*__vfs_ext_ref)(int) = __check_vfs_ext;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"vfs_ext check failed:\n{err}")

    def test_threads_h_namespace_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <threads.h>",
                "#ifdef PTHREAD_MUTEX_NORMAL",
                '#error "threads.h leaked pthread.h macros"',
                "#endif",
                "#ifdef SCHED_FIFO",
                '#error "threads.h leaked sched.h macros"',
                "#endif",
                "static mtx_t __c11_m;",
                "static cnd_t __c11_c;",
                "static once_flag __c11_o = ONCE_FLAG_INIT;",
                "void *__c11_refs[3] = {&__c11_m, &__c11_c, &__c11_o};",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"threads.h hygiene failed:\n{err}")

    def test_signals_threads_sem_and_process_declarations(self) -> None:
        src = "\n".join(
            (
                "#include <pthread.h>",
                "#include <sched.h>",
                "#include <semaphore.h>",
                "#include <signal.h>",
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "#include <sys/wait.h>",
                "#include <threads.h>",
                "#include <unistd.h>",
                "#ifdef SYS_SIGQUEUE",
                '#error "signal.h leaked SYS_SIGQUEUE"',
                "#endif",
                "static int __check_sig_thr_proc(void) {",
                "    sigset_t set;",
                "    siginfo_t si;",
                "    struct timespec ts = {0, 1};",
                "    char name[SIG2STR_MAX];",
                "    int num = 0;",
                "    union sigval sv = {0};",
                "    (void)sigemptyset(&set);",
                "    (void)sigsuspend(&set);",
                "    (void)sigwait(&set, &num);",
                "    (void)sigwaitinfo(&set, &si);",
                "    (void)sigtimedwait(&set, &si, &ts);",
                "    (void)sigqueue(1, SIGUSR1, sv);",
                "    (void)sig2str(SIGTERM, name);",
                "    (void)str2sig(\"TERM\", &num);",
                "    psignal(SIGPIPE, \"msg\");",
                "    psiginfo(&si, \"msg\");",
                "    (void)strsignal(SIGPIPE);",
                "    (void)_Fork();",
                "    (void)pause();",
                "    (void)execl(\"/bin/true\", \"true\", (char *)0);",
                "    (void)execle(\"/bin/true\", \"true\", (char *)0,",
                "                 (char *)0);",
                "    (void)execlp(\"true\", \"true\", (char *)0);",
                "    (void)fexecve(0, (char **)0, (char **)0);",
                "    (void)waitid(P_ALL, 0, &si, WEXITED | WNOHANG);",
                "    (void)system(\"true\");",
                "    FILE *pp = popen(\"true\", \"r\");",
                "    (void)pclose(pp);",
                "    (void)sched_get_priority_max(SCHED_FIFO);",
                "    (void)sched_get_priority_min(SCHED_RR);",
                "    (void)sched_rr_get_interval(0, &ts);",
                "    thrd_t thr;",
                "    (void)thrd_create(&thr, 0, 0);",
                "    (void)thrd_equal(thr, thrd_current());",
                "    (void)thrd_detach(thr);",
                "    (void)thrd_sleep(&ts, &ts);",
                "    thrd_yield();",
                "    (void)thrd_join(thr, &num);",
                "    mtx_t m;",
                "    (void)mtx_init(&m, mtx_plain);",
                "    (void)mtx_lock(&m);",
                "    (void)mtx_trylock(&m);",
                "    (void)mtx_timedlock(&m, &ts);",
                "    (void)mtx_unlock(&m);",
                "    mtx_destroy(&m);",
                "    cnd_t c;",
                "    (void)cnd_init(&c);",
                "    (void)cnd_signal(&c);",
                "    (void)cnd_broadcast(&c);",
                "    cnd_destroy(&c);",
                "    once_flag of = ONCE_FLAG_INIT;",
                "    call_once(&of, 0);",
                "    tss_t k;",
                "    (void)tss_create(&k, 0);",
                "    (void)tss_get(k);",
                "    (void)tss_set(k, 0);",
                "    tss_delete(k);",
                "    (void)pthread_atfork(0, 0, 0);",
                "    pthread_attr_t pa;",
                "    size_t gs = 0;",
                "    struct sched_param sp;",
                "    void *saddr = 0;",
                "    size_t ssize_ = 0;",
                "    (void)pthread_attr_getguardsize(&pa, &gs);",
                "    (void)pthread_attr_setguardsize(&pa, gs);",
                "    (void)pthread_attr_getschedparam(&pa, &sp);",
                "    (void)pthread_attr_setschedparam(&pa, &sp);",
                "    (void)pthread_attr_getstack(&pa, &saddr, &ssize_);",
                "    (void)pthread_attr_setstack(&pa, saddr, ssize_);",
                "    pthread_mutexattr_t ma;",
                "    int ceil_ = 0;",
                "    int rob = PTHREAD_MUTEX_STALLED;",
                "    (void)pthread_mutexattr_getprioceiling(&ma, &ceil_);",
                "    (void)pthread_mutexattr_setprioceiling(&ma, ceil_);",
                "    (void)pthread_mutexattr_getrobust(&ma, &rob);",
                "    (void)pthread_mutexattr_setrobust(",
                "        &ma, PTHREAD_MUTEX_ROBUST);",
                "    pthread_mutex_t mx;",
                "    (void)pthread_mutex_getprioceiling(&mx, &ceil_);",
                "    (void)pthread_mutex_setprioceiling(&mx, ceil_, &ceil_);",
                "    (void)pthread_mutex_consistent(&mx);",
                "    sem_t *sm = sem_open(\"/x\", 0);",
                "    (void)sem_close(sm);",
                "    (void)sem_unlink(\"/x\");",
                "    return (int)sizeof(siginfo_t);",
                "}",
                "int (*__sig_thr_proc_ref)(void) = __check_sig_thr_proc;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"sig/thr/proc check failed:\n{err}")


    def test_base_stdio_stdlib_string_time_completion(self) -> None:
        src = "\n".join(
            (
                "#include <stdarg.h>",
                "#include <stdio.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "#include <time.h>",
                "static void __noop_qe(void) {}",
                "static int __cmp_r(const void *a, const void *b, void *ctx) {",
                "    (void)ctx;",
                "    return (a > b) - (a < b);",
                "}",
                "static int __check_base_std(FILE *fp, va_list ap) {",
                "    char buf[64];",
                "    char *p = buf;",
                "    size_t sz = sizeof(buf);",
                "    fpos_t pos;",
                "    (void)freopen(\"/tmp/a\", \"r\", fp);",
                "    (void)fmemopen(buf, sizeof(buf), \"r+\");",
                "    (void)open_memstream(&p, &sz);",
                "    (void)getdelim(&p, &sz, ':', fp);",
                "    (void)getline(&p, &sz, fp);",
                "    (void)fgetpos(fp, &pos);",
                "    (void)fsetpos(fp, &pos);",
                "    (void)dprintf(1, \"%d\", 1);",
                "    (void)vdprintf(1, \"%d\", ap);",
                "    (void)asprintf(&p, \"%d\", 1);",
                "    (void)vasprintf(&p, \"%d\", ap);",
                "    int x = 0;",
                "    (void)scanf(\"%d\", &x);",
                "    (void)fscanf(fp, \"%d\", &x);",
                "    (void)sscanf(\"1\", \"%d\", &x);",
                "    (void)vscanf(\"%d\", ap);",
                "    (void)vfscanf(fp, \"%d\", ap);",
                "    (void)vsscanf(\"1\", \"%d\", ap);",
                "    (void)strtod(\"1.0\", &p);",
                "    (void)strtof(\"1.0\", &p);",
                "    (void)strtold(\"1.0\", &p);",
                "    (void)atof(\"1.0\");",
                "    (void)aligned_alloc(64u, 64u);",
                "    (void)at_quick_exit(__noop_qe);",
                "    div_t d1 = div(7, 3);",
                "    ldiv_t d2 = ldiv(7L, 3L);",
                "    lldiv_t d3 = lldiv(7LL, 3LL);",
                "    srand(1u);",
                "    int r = rand() % RAND_MAX;",
                "    qsort_r(&x, 1u, sizeof(x), __cmp_r, (void *)0);",
                "    char *const ks[] = {\"a\", (char *)0};",
                "    (void)getsubopt(&p, ks, &p);",
                "    (void)stpcpy(buf, \"a\");",
                "    (void)stpncpy(buf, \"a\", 2u);",
                "    (void)memmem(buf, 2u, \"a\", 1u);",
                "    (void)strerror_r(0, buf, sizeof(buf));",
                "    time_t t = time((time_t *)0);",
                "    (void)difftime(t, 0);",
                "    clock_t cl = clock();",
                "    struct timespec ts = {0, 0};",
                "    (void)timespec_get(&ts, TIME_UTC);",
                "    struct tm tm0 = {0};",
                "    (void)gmtime(&t);",
                "    (void)gmtime_r(&t, &tm0);",
                "    (void)localtime(&t);",
                "    (void)localtime_r(&t, &tm0);",
                "    (void)mktime(&tm0);",
                "    (void)asctime(&tm0);",
                "    (void)asctime_r(&tm0, buf);",
                "    (void)ctime(&t);",
                "    (void)ctime_r(&t, buf);",
                "    (void)strftime(buf, sizeof(buf), \"%F %z %Z\", &tm0);",
                "    return x + d1.quot + (int)d2.rem + (int)d3.quot + r +",
                "           (int)(cl / CLOCKS_PER_SEC) + (int)tm0.tm_gmtoff;",
                "}",
                "int (*__base_std_ref)(FILE *, va_list) = __check_base_std;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"base std check failed:\n{err}")


    def test_complex_and_tgmath_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <complex.h>",
                "#include <tgmath.h>",
                "#ifdef SYS_DEBUG_LOG",
                '#error "complex.h/tgmath.h leaked SYS_DEBUG_LOG"',
                "#endif",
                "static double __check_complex_tg(double x, float xf) {",
                "    double complex z = CMPLX(x, 1.0);",
                "    float complex zf = CMPLXF(xf, 1.0f);",
                "    long double complex zl = CMPLXL(1.0L, 1.0L);",
                "    double complex w = cexp(z) + clog(z) + csqrt(z) +",
                "                       cpow(z, z) + csin(z) + ccos(z) +",
                "                       ctan(z) + casin(z) + cacos(z) +",
                "                       catan(z) + csinh(z) + ccosh(z) +",
                "                       ctanh(z) + casinh(z) + cacosh(z) +",
                "                       catanh(z) + conj(z) + cproj(z) +",
                "                       (double complex)I;",
                "    float complex wf = cexpf(zf) + clogf(zf) + csqrtf(zf) +",
                "                       cpowf(zf, zf) + csinf(zf) +",
                "                       ccosf(zf) + ctanf(zf) + casinf(zf) +",
                "                       cacosf(zf) + catanf(zf) +",
                "                       csinhf(zf) + ccoshf(zf) +",
                "                       ctanhf(zf) + casinhf(zf) +",
                "                       cacoshf(zf) + catanhf(zf) +",
                "                       conjf(zf) + cprojf(zf);",
                "    long double complex wl = cexpl(zl) + clogl(zl) +",
                "                             csqrtl(zl) + cpowl(zl, zl) +",
                "                             csinl(zl) + ccosl(zl) +",
                "                             ctanl(zl) + casinl(zl) +",
                "                             cacosl(zl) + catanl(zl) +",
                "                             csinhl(zl) + ccoshl(zl) +",
                "                             ctanhl(zl) + casinhl(zl) +",
                "                             cacoshl(zl) + catanhl(zl) +",
                "                             conjl(zl) + cprojl(zl);",
                "    double tg = fabs(w) + creal(sin(z)) + cimag(cos(z)) +",
                "                carg(w) + (double)cabsf(wf) +",
                "                (double)cabsl(wl) + sqrt(x) +",
                "                (double)sqrt(xf) + (double)creall(wl);",
                "    return tg + cabs(w) + cargf(wf) + (double)cargl(wl);",
                "}",
                "double (*__cx_tg_ref)(double, float) = __check_complex_tg;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, ["-fhosted", macro])
                self.assertEqual(rc, 0, f"complex/tgmath check failed:\n{err}")

    def test_base_utimens_lgamma_and_tzset_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <fcntl.h>",
                "#include <math.h>",
                "#include <sys/stat.h>",
                "#include <tgmath.h>",
                "#include <time.h>",
                "#ifdef VFS_OP_UTIMENS",
                '#error "sys/stat.h leaked VFS_OP_UTIMENS"',
                "#endif",
                "#if !defined(_XOPEN_SOURCE)",
                "static int signgam = 0;",
                "static int timezone = 0;",
                "static int daylight = 0;",
                "#endif",
                "static double __check_utimens_lgamma_tz(int fd) {",
                "    struct timespec ts[2] = {{0, UTIME_NOW},",
                "                             {0, UTIME_OMIT}};",
                "    (void)futimens(fd, ts);",
                "    (void)utimensat(AT_FDCWD, \"/tmp\", ts,",
                "                    AT_SYMLINK_NOFOLLOW);",
                "    tzset();",
                "    const char *z0 = tzname[0];",
                "    const char *z1 = tzname[1];",
                "    double a = lgamma(2.5) + (double)lgammaf(2.5f) +",
                "               (double)lgammal(2.5L);",
                "#if !defined(_XOPEN_SOURCE)",
                "    a += (double)(signgam + timezone + daylight);",
                "#endif",
                "    return a + (double)(z0 != (const char *)0) +",
                "           (double)(z1 != (const char *)0);",
                "}",
                "double (*__ult_ref)(int) = __check_utimens_lgamma_tz;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, ["-fhosted", macro])
                self.assertEqual(
                    rc, 0, f"utimens/lgamma/tzset check failed:\n{err}"
                )

    def test_locale_wchar_and_wctype_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <ctype.h>",
                "#include <inttypes.h>",
                "#include <locale.h>",
                "#include <stdlib.h>",
                "#include <string.h>",
                "#include <strings.h>",
                "#include <time.h>",
                "#include <wchar.h>",
                "#include <wctype.h>",
                "#ifdef SYS_DEBUG_LOG",
                '#error "locale/wchar/wctype leaked SYS_DEBUG_LOG"',
                "#endif",
                "static int __check_loc_wc(void) {",
                "    locale_t loc = newlocale(LC_ALL_MASK, \"C.UTF-8\",",
                "                             (locale_t)0);",
                "    locale_t dup = duplocale(loc);",
                "    locale_t old = uselocale(dup);",
                "    const char *nm = getlocalename_l(LC_CTYPE, dup);",
                "    struct lconv *lc = localeconv();",
                "    (void)setlocale(LC_ALL, (const char *)0);",
                "    mbstate_t st = {0};",
                "    wchar_t wbuf[16] = {0};",
                "    char cbuf[16] = {0};",
                "    struct tm tm0 = {0};",
                "    (void)mbsinit(&st);",
                "    (void)btowc('A');",
                "    (void)wctob((wint_t)'A');",
                "    (void)mblen(\"a\", 1u);",
                "    (void)mbtowc(wbuf, \"a\", 1u);",
                "    (void)wctomb(cbuf, L'a');",
                "    (void)mbstowcs(wbuf, \"a\", 16u);",
                "    (void)wcstombs(cbuf, L\"a\", 16u);",
                "    (void)isalpha_l('a', dup);",
                "    (void)strcasecmp_l(\"a\", \"A\", dup);",
                "    (void)strcoll_l(\"a\", \"b\", dup);",
                "    (void)strxfrm_l(cbuf, \"a\", sizeof(cbuf), dup);",
                "    (void)strerror_l(0, dup);",
                "    (void)strftime_l(cbuf, sizeof(cbuf), \"%Y\", &tm0, dup);",
                "    (void)iswalpha_l((wint_t)'a', dup);",
                "    (void)towlower_l((wint_t)'A', dup);",
                "    (void)iswctype_l((wint_t)'a', wctype_l(\"alpha\", dup),",
                "                     dup);",
                "    (void)towctrans_l((wint_t)'A',",
                "                      wctrans_l(\"tolower\", dup), dup);",
                "    (void)wcscasecmp_l(L\"a\", L\"A\", dup);",
                "    (void)wcscoll_l(L\"a\", L\"b\", dup);",
                "    (void)wcsxfrm_l(wbuf, L\"a\", 16u, dup);",
                "    (void)wcstoimax(L\"42\", (wchar_t **)0, 10);",
                "    (void)wcstoumax(L\"42\", (wchar_t **)0, 10);",
                "    (void)wcstod(L\"1.5\", (wchar_t **)0);",
                "    (void)wcsftime(wbuf, 16u, L\"%Y\", &tm0);",
                "    (void)uselocale(old);",
                "    freelocale(dup);",
                "    freelocale(loc);",
                "    return (int)MB_CUR_MAX + (nm != (const char *)0) +",
                "           (lc != (struct lconv *)0) + (WEOF != (wint_t)0);",
                "}",
                "int (*__lwc_ref)(void) = __check_loc_wc;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(
                    rc, 0, f"locale/wchar/wctype check failed:\n{err}"
                )

    def test_wide_stdio_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <wchar.h>",
                "#ifdef BUFSIZ",
                '#error "wchar.h leaked stdio.h BUFSIZ"',
                "#endif",
                "static int __check_wide_stdio(FILE *fp, va_list ap) {",
                "    wchar_t wbuf[16] = {0};",
                "    wchar_t *mp = (wchar_t *)0;",
                "    size_t msz = 0u;",
                "    (void)fwide(fp, 0);",
                "    (void)open_wmemstream(&mp, &msz);",
                "    (void)fgetwc(fp);",
                "    (void)fgetws(wbuf, 16, fp);",
                "    (void)fputwc(L'a', fp);",
                "    (void)fputws(L\"a\", fp);",
                "    (void)getwc(fp);",
                "    (void)getwchar();",
                "    (void)putwc(L'a', fp);",
                "    (void)putwchar(L'a');",
                "    (void)ungetwc((wint_t)L'a', fp);",
                "    (void)fwprintf(fp, L\"%d\", 1);",
                "    (void)wprintf(L\"%d\", 1);",
                "    (void)swprintf(wbuf, 16u, L\"%d\", 1);",
                "    (void)vfwprintf(fp, L\"%d\", ap);",
                "    (void)vwprintf(L\"%d\", ap);",
                "    (void)vswprintf(wbuf, 16u, L\"%d\", ap);",
                "    (void)fwscanf(fp, L\"%ls\", wbuf);",
                "    (void)wscanf(L\"%ls\", wbuf);",
                "    (void)swscanf(wbuf, L\"%ls\", wbuf);",
                "    (void)vfwscanf(fp, L\"%ls\", ap);",
                "    (void)vwscanf(L\"%ls\", ap);",
                "    return vswscanf(wbuf, L\"%ls\", ap);",
                "}",
                "int (*__ws_ref)(FILE *, va_list) = __check_wide_stdio;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"wide stdio check failed:\n{err}")

    def test_regex_glob_wordexp_tar_cpio_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <cpio.h>",
                "#include <glob.h>",
                "#include <regex.h>",
                "#include <tar.h>",
                "#include <wordexp.h>",
                "#ifdef SYS_FD_OP",
                '#error "pattern/archive headers leaked SYS_FD_OP"',
                "#endif",
                "#ifdef BUFSIZ",
                '#error "pattern/archive headers leaked stdio.h BUFSIZ"',
                "#endif",
                '_Static_assert(TMAGLEN == 6, "TMAGLEN");',
                '_Static_assert(TVERSLEN == 2, "TVERSLEN");',
                '_Static_assert(C_ISREG == 0100000, "C_ISREG");',
                "static int __check_pat_arch(void) {",
                "    regex_t re;",
                "    regmatch_t pm[2];",
                "    char ebuf[32];",
                "    glob_t gl;",
                "    wordexp_t we;",
                "    int rc = regcomp(&re, \"a+\", REG_EXTENDED | REG_ICASE);",
                "    rc += regexec(&re, \"aaa\", 2u, pm, REG_NOTBOL);",
                "    (void)regerror(REG_NOMATCH, &re, ebuf, sizeof(ebuf));",
                "    regfree(&re);",
                "    rc += glob(\"*.c\", GLOB_ERR | GLOB_MARK | GLOB_NOCHECK,",
                "               (int (*)(const char *, int))0, &gl);",
                "    globfree(&gl);",
                "    rc += wordexp(\"a b\", &we, WRDE_NOCMD | WRDE_UNDEF);",
                "    wordfree(&we);",
                "    return rc + (int)(TMAGIC[0]) + (int)(MAGIC[0]);",
                "}",
                "int (*__pat_arch_ref)(void) = __check_pat_arch;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(
                    rc, 0, f"regex/glob/wordexp/tar/cpio check failed:\n{err}"
                )

    def test_i18n_headers_and_hygiene(self) -> None:
        src = "\n".join(
            (
                "#include <iconv.h>",
                "#include <langinfo.h>",
                "#include <libintl.h>",
                "#include <monetary.h>",
                "#include <nl_types.h>",
                "#ifdef SYS_FD_OP",
                '#error "i18n headers leaked SYS_FD_OP"',
                "#endif",
                "#ifdef BUFSIZ",
                '#error "i18n headers leaked stdio.h BUFSIZ"',
                "#endif",
                "static int __check_i18n(locale_t loc) {",
                "    char buf[32] = {0};",
                "    char *ip = buf;",
                "    char *op = buf;",
                "    size_t il = 0u;",
                "    size_t ol = sizeof(buf);",
                "    (void)nl_langinfo(CODESET);",
                "    (void)nl_langinfo_l(ALTMON_1, loc);",
                "    (void)strfmon(buf, sizeof(buf), \"%n\", 1.0);",
                "    (void)strfmon_l(buf, sizeof(buf), loc, \"%i\", 1.0);",
                "    nl_catd c = catopen(\"demo\", NL_CAT_LOCALE);",
                "    (void)catgets(c, NL_SETD, 1, \"d\");",
                "    (void)catclose(c);",
                "    iconv_t cd = iconv_open(\"UTF-8\", \"US-ASCII\");",
                "    (void)iconv(cd, &ip, &il, &op, &ol);",
                "    (void)iconv_close(cd);",
                "    (void)textdomain(\"demo\");",
                "    (void)bindtextdomain(\"demo\", \"/tmp\");",
                "    (void)bind_textdomain_codeset(\"demo\", \"UTF-8\");",
                "    (void)gettext(\"a\");",
                "    (void)gettext_l(\"a\", loc);",
                "    (void)dgettext(\"demo\", \"a\");",
                "    (void)dgettext_l(\"demo\", \"a\", loc);",
                "    (void)dcgettext(\"demo\", \"a\", 0);",
                "    (void)dcgettext_l(\"demo\", \"a\", 0, loc);",
                "    (void)ngettext(\"a\", \"b\", 1ul);",
                "    (void)ngettext_l(\"a\", \"b\", 1ul, loc);",
                "    (void)dngettext(\"demo\", \"a\", \"b\", 1ul);",
                "    (void)dngettext_l(\"demo\", \"a\", \"b\", 1ul, loc);",
                "    (void)dcngettext(\"demo\", \"a\", \"b\", 1ul, 0);",
                "    (void)dcngettext_l(\"demo\", \"a\", \"b\", 1ul, 0, loc);",
                "    return 0;",
                "}",
                "int (*__i18n_ref)(locale_t) = __check_i18n;",
                "",
            )
        )
        for label, macro in FEATURE_PROFILES:
            with self.subTest(profile=label):
                rc, err = _compile_snippet(src, [macro])
                self.assertEqual(rc, 0, f"i18n headers check failed:\n{err}")


def main(argv: list[str] | None = None) -> int:
    global _COMPILER, _TARGET, _CLANG_INC, _SYSROOT
    parser = argparse.ArgumentParser(
        description="Run POSIX.1-2024 standalone header compilation gate."
    )
    parser.add_argument("--cc", default="clang", help="C compiler executable.")
    parser.add_argument(
        "--target",
        default="x86_64-unknown-none-elf",
        help="Clang target triple.",
    )
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
    _TARGET = args.target
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
