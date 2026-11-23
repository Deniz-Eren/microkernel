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

/* need:: ICD_PROC_0005 ICD_LIBC_0009 ICD_LIBC_0011 */

#include <stddef.h>
#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

#define CLOCKS_PER_SEC ((clock_t)1000000L)
#define TIME_UTC 1
#define TIMER_ABSTIME 1

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
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

extern char *tzname[2];

clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t time(time_t *tloc);
int timespec_get(struct timespec *ts, int base);
void tzset(void);

struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *restrict timer, struct tm *restrict result);
struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *restrict timer,
                       struct tm *restrict result);
time_t mktime(struct tm *timeptr);

char *asctime(const struct tm *timeptr);
char *asctime_r(const struct tm *restrict tm, char *restrict buf);
char *ctime(const time_t *clock);
char *ctime_r(const time_t *clock, char *buf);
size_t strftime(char *restrict s, size_t maxsize, const char *restrict format,
                const struct tm *restrict timeptr);

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
