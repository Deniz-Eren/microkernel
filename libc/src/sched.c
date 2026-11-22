/**
 * @file     sched.c
 * @brief    POSIX.1-2024 Base execution scheduling syscall wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/microkernel.h>
#include <unistd.h>

/* need:: COMP_LIBC_0006 */

#define SCHED_SLICE_TICKS 5ULL
#define SCHED_KERNEL_PRIO_MIN 81
#define SCHED_KERNEL_PRIO_MAX 120

int sched_yield(void) {
    long r = __sys0(SYS_SCHED_YIELD);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int sched_get_priority_max(int policy) {
    switch (policy) {
    case SCHED_FIFO:
    case SCHED_RR:
        return SCHED_KERNEL_PRIO_MAX;
    case SCHED_OTHER:
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int sched_get_priority_min(int policy) {
    switch (policy) {
    case SCHED_FIFO:
    case SCHED_RR:
        return SCHED_KERNEL_PRIO_MIN;
    case SCHED_OTHER:
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}

int sched_rr_get_interval(pid_t pid, struct timespec *interval) {
    if (!interval || pid < 0) {
        errno = EINVAL;
        return -1;
    }
    if (pid > 0 && kill(pid, 0) < 0) {
        return -1;
    }
    long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0) {
        clk = 1000L;
    }
    unsigned long uclk = (unsigned long)clk;
    interval->tv_sec = (time_t)(SCHED_SLICE_TICKS / uclk);
    interval->tv_nsec =
        (long)(((SCHED_SLICE_TICKS % uclk) * 1000000000ULL) / uclk);
    return 0;
}
