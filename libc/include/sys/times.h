/**
 * @file     times.h
 * @brief    POSIX.1-2024 process CPU time structure and times() interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

/* need:: ICD_LIBC_0006 */

#include <sys/types.h>

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

clock_t times(struct tms *buffer);

#endif /* _SYS_TIMES_H */
