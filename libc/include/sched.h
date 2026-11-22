/**
 * @file     sched.h
 * @brief    POSIX.1-2024 Base execution scheduling declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SCHED_H
#define _SCHED_H

/* need:: ICD_LIBC_0006 */

#include <sys/types.h>
#include <time.h>

struct sched_param {
    int sched_priority;
};

#define SCHED_OTHER 0
#define SCHED_FIFO 1
#define SCHED_RR 2

int sched_yield(void);
int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_rr_get_interval(pid_t pid, struct timespec *interval);

#endif /* _SCHED_H */
