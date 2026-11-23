/**
 * @file     unistd.c
 * @brief    POSIX.1-2024 errno-preserving unistd syscall wrappers and execvp
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/microkernel.h>
#include <sys/stat.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 COMP_PROC_0007 */

void __libc_run_atfork_prepare(void);
void __libc_run_atfork_parent(void);
void __libc_run_atfork_child(void);

#ifdef MICROKERNEL_COVERAGE
extern void __libc_cov_reset_counters(void);
#endif

ssize_t write(int fd, const void *buf, size_t n) {
    return __libc_fd_write(fd, buf, n);
}

ssize_t read(int fd, void *buf, size_t n) {
    return __libc_fd_read(fd, buf, n);
}

pid_t getpid(void) {
    return (pid_t)__sys0(SYS_PROC_GETPID);
}

pid_t getppid(void) {
    return (pid_t)__sys0(SYS_PROC_GETPPID);
}

int setpgid(pid_t pid, pid_t pgid) {
    long r = __sys3(SYS_PROC_PGID, PGID_OP_SETPGID, (long)pid, (long)pgid);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

pid_t getpgid(pid_t pid) {
    long r = __sys3(SYS_PROC_PGID, PGID_OP_GETPGID, (long)pid, 0);
    if (r < 0) {
        errno = (int)(-r);
        return (pid_t)-1;
    }
    return (pid_t)r;
}

pid_t getpgrp(void) {
    return getpgid(0);
}

pid_t setsid(void) {
    long r = __sys3(SYS_PROC_PGID, PGID_OP_SETSID, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return (pid_t)-1;
    }
    return (pid_t)r;
}

pid_t getsid(pid_t pid) {
    long r = __sys3(SYS_PROC_PGID, PGID_OP_GETSID, (long)pid, 0);
    if (r < 0) {
        errno = (int)(-r);
        return (pid_t)-1;
    }
    return (pid_t)r;
}

pid_t _Fork(void) {
    int pr = __libc_fd_prefork();
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = __sys0(SYS_PROC_FORK);
    if (r < 0) {
        __libc_fd_postfork_rollback();
        errno = (int)(-r);
        return -1;
    }
    if (r == 0) {
#ifdef MICROKERNEL_COVERAGE
        __libc_cov_reset_counters();
#endif
        __pthread_tls_t *tls = __libc_tls_self();
        if (tls) {
            tls->tid = (unsigned int)ThreadGetTid();
        }
    }
    return (pid_t)r;
}

pid_t fork(void) {
    __libc_run_atfork_prepare();
    pid_t r = _Fork();
    if (r < 0) {
        int saved = errno;
        __libc_run_atfork_parent();
        errno = saved;
        return -1;
    }
    if (r == 0) {
        __libc_run_atfork_child();
    } else {
        __libc_run_atfork_parent();
    }
    return r;
}

int pause(void) {
    __libc_cancel_point();
    sigset_t cur;
    if (sigprocmask(SIG_SETMASK, (const sigset_t *)0, &cur) != 0) {
        return -1;
    }
    (void)sigsuspend(&cur);
    errno = EINTR;
    return -1;
}

int fexecve(int fildes, char *const argv[], char *const envp[]) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS ||
        (e.status_flags & (uint16_t)O_ACCMODE) == (uint16_t)O_WRONLY) {
        errno = EBADF;
        return -1;
    }
    char path[PATH_MAX];
    uint32_t mode = 0u;
    int rr = __libc_vfs_resolve_handle(e.srv_handle, path, sizeof(path), &mode);
    if (rr < 0) {
        errno = -rr;
        return -1;
    }
    if (!S_ISREG(mode) || (mode & 0111u) == 0u) {
        errno = EACCES;
        return -1;
    }
    return execve(path, argv, envp);
}

#define EXECL_ARG_MAX 64

static int collect_execl_argv(va_list ap, const char *arg0, const char **argv) {
    int argc = 0;
    const char *arg = arg0;
    while (arg != NULL) {
        if (argc >= EXECL_ARG_MAX) {
            return -1;
        }
        argv[argc++] = arg;
        arg = va_arg(ap, const char *);
    }
    argv[argc] = NULL;
    return argc;
}

int execl(const char *path, const char *arg0, ...) {
    const char *argv[EXECL_ARG_MAX + 1];
    va_list ap;
    va_start(ap, arg0);
    int argc = collect_execl_argv(ap, arg0, argv);
    va_end(ap);
    if (argc < 0) {
        errno = E2BIG;
        return -1;
    }
    return execve(path, (char *const *)argv, environ);
}

int execle(const char *path, const char *arg0, ...) {
    const char *argv[EXECL_ARG_MAX + 1];
    va_list ap;
    va_start(ap, arg0);
    int argc = collect_execl_argv(ap, arg0, argv);
    char *const *envp = (argc >= 0) ? va_arg(ap, char *const *) : NULL;
    va_end(ap);
    if (argc < 0) {
        errno = E2BIG;
        return -1;
    }
    return execve(path, (char *const *)argv, envp);
}

int execlp(const char *file, const char *arg0, ...) {
    const char *argv[EXECL_ARG_MAX + 1];
    va_list ap;
    va_start(ap, arg0);
    int argc = collect_execl_argv(ap, arg0, argv);
    va_end(ap);
    if (argc < 0) {
        errno = E2BIG;
        return -1;
    }
    return execvp(file, (char *const *)argv);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    if (!path) {
        errno = EFAULT;
        return -1;
    }
    long r = __sys3(SYS_PROC_EXECVE, (long)(uintptr_t)path,
                    (long)(uintptr_t)argv, (long)(uintptr_t)envp);
    errno = (int)(-r);
    return -1;
}

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[]) {
    if (!file || !*file) {
        errno = ENOENT;
        return -1;
    }
    if (strchr(file, '/') != 0) {
        return execve(file, argv, environ);
    }

    const char *path = getenv("PATH");
    if (!path || !*path) {
        path = "/boot:/bin";
    }

    size_t flen = strlen(file);
    int seen_eacces = 0;
    int seen_toolong = 0;
    const char *p = path;

    for (;;) {
        const char *colon = strchr(p, ':');
        size_t dlen = colon ? (size_t)(colon - p) : strlen(p);
        char cand[PATH_MAX];
        size_t total = 0;
        int valid = 1;

        if (dlen == 0) {
            if (flen + 2u >= sizeof(cand)) {
                valid = 0;
                seen_toolong = 1;
            } else {
                cand[0] = '.';
                cand[1] = '/';
                memcpy(cand + 2u, file, flen + 1u);
                total = flen + 2u;
            }
        } else {
            int need_slash = (p[dlen - 1u] != '/');
            total = dlen + (size_t)need_slash + flen;
            if (total >= sizeof(cand)) {
                valid = 0;
                seen_toolong = 1;
            } else {
                memcpy(cand, p, dlen);
                size_t pos = dlen;
                if (need_slash) {
                    cand[pos++] = '/';
                }
                memcpy(cand + pos, file, flen + 1u);
            }
        }

        if (valid && total > 0) {
            (void)execve(cand, argv, environ);
            if (errno == EACCES) {
                seen_eacces = 1;
            } else if (errno != ENOENT && errno != ENOTDIR) {
                return -1;
            }
        }

        if (!colon) {
            break;
        }
        p = colon + 1;
    }

    if (seen_eacces) {
        errno = EACCES;
    } else if (seen_toolong) {
        errno = ENAMETOOLONG;
    } else {
        errno = ENOENT;
    }
    return -1;
}

int getentropy(void *buffer, size_t length) {
    long r = __sys2(SYS_GETENTROPY, (long)(uintptr_t)buffer, (long)length);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

void _exit(int status) {
    __sys1(SYS_PROC_EXIT, status);
    for (;;) {
    }
}

unsigned int sleep(unsigned int seconds) {
    if (seconds == 0u) {
        return 0u;
    }
    struct timespec req = {.tv_sec = (time_t)seconds, .tv_nsec = 0};
    struct timespec rem = {0};
    if (clock_nanosleep(CLOCK_MONOTONIC, 0, &req, &rem) == EINTR) {
        return (unsigned int)rem.tv_sec + (rem.tv_nsec > 0 ? 1u : 0u);
    }
    return 0u;
}

int daemon(int nochdir, int noclose) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        _exit(0);
    }
    if (setsid() == (pid_t)-1) {
        return -1;
    }
    if (!nochdir) {
        (void)chdir("/");
    }
    if (!noclose) {
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            (void)dup2(fd, STDIN_FILENO);
            (void)dup2(fd, STDOUT_FILENO);
            (void)dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO) {
                (void)close(fd);
            }
        }
    }
    return 0;
}

int chroot(const char *path) {
    if (!path) {
        errno = EFAULT;
        return -1;
    }
    if (geteuid() != 0u) {
        errno = EPERM;
        return -1;
    }
    struct stat st = {0};
    if (stat(path, &st) != 0) {
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    return chdir(path);
}
