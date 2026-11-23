/**
 * @file     time.h
 * @brief    POSIX.1-2024 XSI timeval and file timestamp functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

/* need:: ICD_LIBC_0018 */

#include <sys/select.h>
#include <sys/types.h>

#ifndef _STRUCT_TIMEVAL_DEFINED
#define _STRUCT_TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif /* _STRUCT_TIMEVAL_DEFINED */

int utimes(const char *path, const struct timeval times[2]);

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ||                  \
    defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_GNU_SOURCE)
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *restrict tv, void *restrict tz);
int futimes(int fd, const struct timeval tv[2]);

#define timerisset(tvp) ((tvp)->tv_sec != 0 || (tvp)->tv_usec != 0)
#define timerclear(tvp) ((tvp)->tv_sec = 0, (tvp)->tv_usec = 0)
#define timercmp(a, b, CMP)                                                    \
    (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP(b)->tv_usec)             \
                                  : ((a)->tv_sec CMP(b)->tv_sec))
#define timeradd(a, b, res)                                                    \
    do {                                                                       \
        (res)->tv_sec = (a)->tv_sec + (b)->tv_sec;                             \
        (res)->tv_usec = (a)->tv_usec + (b)->tv_usec;                          \
        if ((res)->tv_usec >= 1000000L) {                                      \
            (res)->tv_sec++;                                                   \
            (res)->tv_usec -= 1000000L;                                        \
        }                                                                      \
    } while (0)
#define timersub(a, b, res)                                                    \
    do {                                                                       \
        (res)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
        (res)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
        if ((res)->tv_usec < 0) {                                              \
            (res)->tv_sec--;                                                   \
            (res)->tv_usec += 1000000L;                                        \
        }                                                                      \
    } while (0)
#endif

#endif /* _SYS_TIME_H */
