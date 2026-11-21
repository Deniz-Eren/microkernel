/**
 * @file     time.h
 * @brief    POSIX.1-2024 Base monotonic/realtime clocks and timer declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TIME_H
#define _TIME_H

/* need:: ICD_PROC_0005 */

#include <stddef.h>
#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

#define TIMER_ABSTIME 1

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

#ifndef _SIGEVENT_DEFINED
#define _SIGEVENT_DEFINED
#define SIGEV_NONE 0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

union sigval {
    int sival_int;
    void *sival_ptr;
};

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
};
#endif /* _SIGEVENT_DEFINED */

int clock_getres(clockid_t clock_id, struct timespec *res);
int clock_gettime(clockid_t clock_id, struct timespec *tp);
int clock_settime(clockid_t clock_id, const struct timespec *tp);
int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *rqtp,
                    struct timespec *rmtp);
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

int timer_create(clockid_t clockid, struct sigevent *restrict evp,
                 timer_t *restrict timerid);
int timer_delete(timer_t timerid);
int timer_getoverrun(timer_t timerid);
int timer_gettime(timer_t timerid, struct itimerspec *value);
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *restrict value,
                  struct itimerspec *restrict ovalue);

#endif /* _TIME_H */
