/**
 * @file     uio.h
 * @brief    POSIX.1-2024 XSI scatter-gather vector I/O types and functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

/* need:: ICD_LIBC_0018 */

#include <stddef.h>
#include <sys/types.h>

#ifndef _STRUCT_IOVEC_DEFINED
#define _STRUCT_IOVEC_DEFINED
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

ssize_t readv(int fildes, const struct iovec *iov, int iovcnt);
ssize_t writev(int fildes, const struct iovec *iov, int iovcnt);

#endif /* _SYS_UIO_H */
