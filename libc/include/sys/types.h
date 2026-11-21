/**
 * @file     types.h
 * @brief    POSIX.1-2024 basic system data type definitions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

/* need:: ICD_LIBC_0001 */

#include <stddef.h>

typedef long ssize_t;
typedef long off_t;
typedef long time_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned int mode_t;
typedef unsigned int nlink_t;
typedef long blksize_t;
typedef long blkcnt_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int pid_t;
typedef unsigned int pthread_t;
typedef int clockid_t;
typedef int timer_t;

#endif /* _SYS_TYPES_H */
