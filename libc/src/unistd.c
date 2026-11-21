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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 */

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

pid_t fork(void) {
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
        __pthread_tls_t *tls = __libc_tls_self();
        if (tls) {
            tls->tid = (unsigned int)ThreadGetTid();
        }
    }
    return (pid_t)r;
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
