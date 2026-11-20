/**
 * @file     errno.h
 * @brief    POSIX.1-2024 error numbers and thread-local errno declaration
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ERRNO_H
#define _ERRNO_H

#include <sys/errno.h>

/* need:: ICD_LIBC_0003 */

int *__errno_location(void);
#define errno (*__errno_location())

#endif /* _ERRNO_H */
