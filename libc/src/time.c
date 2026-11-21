/**
 * @file     time.c
 * @brief    POSIX.1-2024 clock, nanosleep, and interval timer libc wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <sys/microkernel.h>
#include <time.h>

/* need:: COMP_PROC_0004 */

int clock_getres(clockid_t clock_id, struct timespec *res) {
    long r = __sys2(SYS_CLOCK_GETRES, (long)clock_id, (long)(uintptr_t)res);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int clock_gettime(clockid_t clock_id, struct timespec *tp) {
    long r = __sys2(SYS_CLOCK_GETTIME, (long)clock_id, (long)(uintptr_t)tp);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int clock_settime(clockid_t clock_id, const struct timespec *tp) {
    long r = __sys2(SYS_CLOCK_SETTIME, (long)clock_id, (long)(uintptr_t)tp);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *rqtp,
                    struct timespec *rmtp) {
    long r = __sys4(SYS_CLOCK_NANOSLEEP, (long)clock_id, (long)flags,
                    (long)(uintptr_t)rqtp, (long)(uintptr_t)rmtp);
    return (r < 0) ? (int)(-r) : 0;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    int err = clock_nanosleep(CLOCK_REALTIME, 0, rqtp, rmtp);
    if (err != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int timer_create(clockid_t clockid, struct sigevent *restrict evp,
                 timer_t *restrict timerid) {
    long r = __sys3(SYS_TIMER_CREATE, (long)clockid, (long)(uintptr_t)evp,
                    (long)(uintptr_t)timerid);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int timer_delete(timer_t timerid) {
    long r = __sys1(SYS_TIMER_DELETE, (long)timerid);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int timer_getoverrun(timer_t timerid) {
    long r = __sys1(SYS_TIMER_GETOVERRUN, (long)timerid);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (int)r;
}

int timer_gettime(timer_t timerid, struct itimerspec *value) {
    long r = __sys2(SYS_TIMER_GETTIME, (long)timerid, (long)(uintptr_t)value);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *restrict value,
                  struct itimerspec *restrict ovalue) {
    long r = __sys4(SYS_TIMER_SETTIME, (long)timerid, (long)flags,
                    (long)(uintptr_t)value, (long)(uintptr_t)ovalue);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}
