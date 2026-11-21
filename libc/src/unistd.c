/**
 * @file     unistd.c
 * @brief    POSIX.1-2024 errno-preserving unistd syscall wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_LIBC_0001 */

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
