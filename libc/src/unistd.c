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
#include <unistd.h>
#include <sys/microkernel.h>

/* need:: COMP_LIBC_0001 */

/* Until a console server owns fd 0/1/2, output goes to the kernel early debug
 * log (SYS_DEBUG_LOG). Raw syscalls return negative errno; wrappers set
 * thread-local errno only on failure and never clear it on success. */
ssize_t write(int fd, const void *buf, size_t n) {
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
        long r = __sys2(SYS_DEBUG_LOG, (long)buf, (long)n);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return (ssize_t)r;
    }
    errno = EBADF;
    return -1;
}

ssize_t read(int fd, void *buf, size_t n) {
    (void)buf;
    (void)n;
    if (fd < STDIN_FILENO || fd > STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }
    errno = ENOSYS;
    return -1;
}

void _exit(int status) {
    __sys1(SYS_PROC_EXIT, status);
    for (;;) {
    }
}

unsigned int sleep(unsigned int seconds) {
    (void)seconds;
    return 0;
}
