/**
 * @file     sched.c
 * @brief    POSIX.1-2024 Base sched_yield() syscall wrapper
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <sched.h>
#include <sys/microkernel.h>

/* need:: COMP_LIBC_0006 */

int sched_yield(void) {
    long r = __sys0(SYS_SCHED_YIELD);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}
