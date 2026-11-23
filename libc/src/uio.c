/**
 * @file     uio.c
 * @brief    POSIX.1-2024 XSI scatter-gather vector I/O (readv and writev)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/uio.h>
#include <unistd.h>
#include "tls_internal.h"

/* need:: COMP_LIBC_0028 */

ssize_t readv(int fildes, const struct iovec *iov, int iovcnt) {
    __libc_cancel_point();
    if (iovcnt <= 0 || iovcnt > IOV_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!iov) {
        errno = EFAULT;
        return -1;
    }
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0u) {
            continue;
        }
        if (!iov[i].iov_base) {
            if (total > 0) {
                return total;
            }
            errno = EFAULT;
            return -1;
        }
        ssize_t n = read(fildes, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            return total > 0 ? total : -1;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

ssize_t writev(int fildes, const struct iovec *iov, int iovcnt) {
    __libc_cancel_point();
    if (iovcnt <= 0 || iovcnt > IOV_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (!iov) {
        errno = EFAULT;
        return -1;
    }
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0u) {
            continue;
        }
        if (!iov[i].iov_base) {
            if (total > 0) {
                return total;
            }
            errno = EFAULT;
            return -1;
        }
        ssize_t n = write(fildes, iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            return total > 0 ? total : -1;
        }
        total += n;
        if ((size_t)n < iov[i].iov_len) {
            break;
        }
    }
    return total;
}
