/**
 * @file     sched.h
 * @brief    POSIX.1-2024 Base execution scheduling parameters and sched_yield()
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SCHED_H
#define _SCHED_H

/* need:: ICD_LIBC_0006 */

#include <time.h>

struct sched_param {
    int sched_priority;
};

int sched_yield(void);

#endif /* _SCHED_H */
