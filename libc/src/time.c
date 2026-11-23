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
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include "tls_internal.h"

/* need:: COMP_PROC_0004 COMP_LIBC_0006 COMP_LIBC_0009 COMP_LIBC_0028 */

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
    __libc_cancel_point();
    long r = __sys4(SYS_CLOCK_NANOSLEEP, (long)clock_id, (long)flags,
                    (long)(uintptr_t)rqtp, (long)(uintptr_t)rmtp);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
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

clock_t times(struct tms *buffer) {
    if (!buffer) {
        errno = EFAULT;
        return (clock_t)-1;
    }
    struct __k_tms kt = {0};
    long r = __sys1(SYS_PROC_TIMES, (long)(uintptr_t)&kt);
    if (r < 0) {
        errno = (int)(-r);
        return (clock_t)-1;
    }
    buffer->tms_utime = (clock_t)kt.utime;
    buffer->tms_stime = (clock_t)kt.stime;
    buffer->tms_cutime = (clock_t)kt.cutime;
    buffer->tms_cstime = (clock_t)kt.cstime;
    return (clock_t)r;
}

unsigned int alarm(unsigned int seconds) {
    long r = __sys1(SYS_ALARM, (long)seconds);
    return (r < 0) ? 0u : (unsigned int)r;
}

time_t time(time_t *tloc) {
    struct timespec ts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return (time_t)-1;
    }
    if (tloc) {
        *tloc = ts.tv_sec;
    }
    return ts.tv_sec;
}

double difftime(time_t time1, time_t time0) {
    return (double)time1 - (double)time0;
}

clock_t clock(void) {
    struct tms t = {0};
    if (times(&t) == (clock_t)-1) {
        return (clock_t)-1;
    }
    clock_t ticks = t.tms_utime + t.tms_stime;
    return ticks * (CLOCKS_PER_SEC / 1000L);
}

int timespec_get(struct timespec *ts, int base) {
    if (!ts || base != TIME_UTC) {
        return 0;
    }
    if (clock_gettime(CLOCK_REALTIME, ts) != 0) {
        return 0;
    }
    return TIME_UTC;
}

int gettimeofday(struct timeval *restrict tv, void *restrict tz) {
    (void)tz;
    if (!tv) {
        errno = EFAULT;
        return -1;
    }
    struct timespec ts = {0, 0};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return -1;
    }
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = (suseconds_t)(ts.tv_nsec / 1000L);
    return 0;
}
