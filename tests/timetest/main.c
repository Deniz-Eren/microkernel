/**
 * @file     main.c
 * @brief    Ring 3 POSIX.1-2024 clocks, nanosleep, and interval timers test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_timer_hits;

static void on_timer(int sig) {
    if (sig == SIGUSR1) {
        g_timer_hits++;
    }
}

static uint64_t ts_ns(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

int main(void) {
    /* 1. Verify clock_getres for CLOCK_MONOTONIC and CLOCK_REALTIME. */
    struct timespec res_m = {0};
    struct timespec res_r = {0};
    if (clock_getres(CLOCK_MONOTONIC, &res_m) != 0 ||
        clock_getres(CLOCK_REALTIME, &res_r) != 0 ||
        res_m.tv_nsec != 1000000L || res_r.tv_nsec != 1000000L) {
        printf("[FAIL] timetest clock_getres failed\n");
        return 1;
    }

    /* 2. Verify monotonic progression across relative clock_nanosleep. */
    struct timespec t0 = {0};
    struct timespec t1 = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        printf("[FAIL] timetest clock_gettime(MONO) failed\n");
        return 1;
    }
    struct timespec req = {.tv_sec = 0, .tv_nsec = 25000000L}; /* 25 ms */
    int rc = clock_nanosleep(CLOCK_MONOTONIC, 0, &req, (struct timespec *)0);
    if (rc != 0 || clock_gettime(CLOCK_MONOTONIC, &t1) != 0) {
        printf("[FAIL] timetest relative clock_nanosleep failed rc=%d\n", rc);
        return 1;
    }
    if (ts_ns(&t1) < ts_ns(&t0) + 20000000ULL) {
        printf("[FAIL] timetest monotonic delta too small\n");
        return 1;
    }

    /* 3. Verify CLOCK_MONOTONIC rejects clock_settime with EINVAL, while
     * CLOCK_REALTIME updates independently of CLOCK_MONOTONIC. */
    errno = 0;
    if (clock_settime(CLOCK_MONOTONIC, &t1) == 0 || errno != EINVAL) {
        printf("[FAIL] timetest clock_settime(MONO) did not give EINVAL\n");
        return 1;
    }
    struct timespec set_rt = {.tv_sec = 1700000000L, .tv_nsec = 500000000L};
    struct timespec got_rt = {0};
    if (clock_settime(CLOCK_REALTIME, &set_rt) != 0 ||
        clock_gettime(CLOCK_REALTIME, &got_rt) != 0 ||
        got_rt.tv_sec < 1700000000L || got_rt.tv_sec > 1700000010L) {
        printf("[FAIL] timetest clock_settime(REALTIME) mismatch\n");
        return 1;
    }

    /* 4. Verify TIMER_ABSTIME clock_nanosleep and nanosleep. */
    struct timespec abs_req = t1;
    abs_req.tv_nsec += 15000000L;
    if (abs_req.tv_nsec >= 1000000000L) {
        abs_req.tv_sec++;
        abs_req.tv_nsec -= 1000000000L;
    }
    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &abs_req,
                        (struct timespec *)0) != 0) {
        printf("[FAIL] timetest TIMER_ABSTIME sleep failed\n");
        return 1;
    }
    struct timespec t2 = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &t2) != 0 ||
        ts_ns(&t2) < ts_ns(&abs_req)) {
        printf("[FAIL] timetest TIMER_ABSTIME woke early\n");
        return 1;
    }
    struct timespec short_req = {.tv_sec = 0, .tv_nsec = 10000000L};
    if (nanosleep(&short_req, (struct timespec *)0) != 0) {
        printf("[FAIL] timetest nanosleep failed\n");
        return 1;
    }
    printf("[OK] CLOCK_MONOTONIC, CLOCK_REALTIME, and clock_nanosleep "
           "verified\n");

    /* 5. Verify timer_create, periodic timer_settime, timer_gettime,
     * timer_getoverrun, and timer_delete delivering SIGUSR1. */
    struct sigaction sa = {0};
    sa.sa_handler = on_timer;
    if (sigaction(SIGUSR1, &sa, (struct sigaction *)0) != 0) {
        printf("[FAIL] timetest sigaction failed\n");
        return 1;
    }
    struct sigevent ev = {0};
    ev.sigev_notify = SIGEV_SIGNAL;
    ev.sigev_signo = SIGUSR1;
    timer_t tid = -1;
    if (timer_create(CLOCK_MONOTONIC, &ev, &tid) != 0 || tid < 0) {
        printf("[FAIL] timetest timer_create failed errno=%d\n", errno);
        return 1;
    }
    struct itimerspec its = {
        .it_interval = {.tv_sec = 0, .tv_nsec = 15000000L},
        .it_value = {.tv_sec = 0, .tv_nsec = 15000000L},
    };
    if (timer_settime(tid, 0, &its, (struct itimerspec *)0) != 0) {
        printf("[FAIL] timetest timer_settime failed errno=%d\n", errno);
        return 1;
    }
    struct itimerspec cur = {0};
    if (timer_gettime(tid, &cur) != 0 || cur.it_interval.tv_nsec != 15000000L) {
        printf("[FAIL] timetest timer_gettime failed\n");
        return 1;
    }

    for (int i = 0; i < 40 && g_timer_hits < 2; i++) {
        struct timespec wait_slice = {.tv_sec = 0, .tv_nsec = 20000000L};
        (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &wait_slice,
                              (struct timespec *)0);
    }
    int overruns = timer_getoverrun(tid);
    if (timer_delete(tid) != 0) {
        printf("[FAIL] timetest timer_delete failed\n");
        return 1;
    }
    if (g_timer_hits < 2 || overruns < 0) {
        printf("[FAIL] timetest timer hits=%d overruns=%d\n", (int)g_timer_hits,
               overruns);
        return 1;
    }
    printf("[OK] POSIX timer_create/timer_settime fired %d SIGUSR1 ticks\n",
           (int)g_timer_hits);

    /* 6. Verify Base <time.h>: time, difftime, clock, timespec_get, gmtime*,
     * localtime*, mktime, asctime*, ctime*, and strftime. */
    time_t now_arg = 0;
    time_t now_ret = time(&now_arg);
    struct timespec utc_ts = {0, 0};
    if (now_ret != now_arg || now_ret < 1700000000L ||
        timespec_get(&utc_ts, TIME_UTC) != TIME_UTC ||
        timespec_get(&utc_ts, 0) != 0 ||
        fabs(difftime(now_ret + 15L, now_ret) - 15.0) > 1e-9) {
        printf("[FAIL] timetest time/difftime/timespec_get failed\n");
        return 1;
    }
    clock_t c0 = clock();
    if (c0 == (clock_t)-1 || c0 < 0) {
        printf("[FAIL] timetest clock() failed c0=%ld\n", (long)c0);
        return 1;
    }

    time_t epoch0 = 0;
    struct tm tm0 = {0};
    if (!gmtime_r(&epoch0, &tm0) || tm0.tm_year != 70 || tm0.tm_mon != 0 ||
        tm0.tm_mday != 1 || tm0.tm_wday != 4 || tm0.tm_yday != 0 ||
        tm0.tm_hour != 0 || tm0.tm_min != 0 || tm0.tm_sec != 0 ||
        tm0.tm_gmtoff != 0L) {
        printf("[FAIL] timetest gmtime_r(0) mismatch\n");
        return 1;
    }

    /* Leap day 2024-02-29 12:34:56 UTC = 1709210096. */
    time_t leap_t = 1709210096L;
    struct tm *tmg = gmtime(&leap_t);
    if (!tmg || tmg->tm_year != 124 || tmg->tm_mon != 1 || tmg->tm_mday != 29 ||
        tmg->tm_wday != 4 || tmg->tm_yday != 59 || tmg->tm_hour != 12 ||
        tmg->tm_min != 34 || tmg->tm_sec != 56) {
        printf("[FAIL] timetest gmtime(2024-02-29) mismatch\n");
        return 1;
    }

    /* Pre-1970 negative time_t: -86400 = Wed 1969-12-31 00:00:00 UTC. */
    time_t neg_t = -86400L;
    struct tm tmn = {0};
    if (!gmtime_r(&neg_t, &tmn) || tmn.tm_year != 69 || tmn.tm_mon != 11 ||
        tmn.tm_mday != 31 || tmn.tm_wday != 3 || tmn.tm_yday != 364) {
        printf("[FAIL] timetest gmtime_r(-86400) mismatch\n");
        return 1;
    }

    /* tzset() and tzname[] with POSIX TZ strings (STD, STD+DST, quoted). */
    (void)unsetenv("TZ");
    tzset();
    if (!tzname[0] || !tzname[1] || strcmp(tzname[0], "UTC") != 0 ||
        strcmp(tzname[1], "UTC") != 0) {
        printf("[FAIL] timetest tzset(unset) mismatch\n");
        return 1;
    }
    (void)setenv("TZ", "EST5EDT", 1);
    tzset();
    if (strcmp(tzname[0], "EST") != 0 || strcmp(tzname[1], "EDT") != 0) {
        printf("[FAIL] timetest tzset(EST5EDT) got '%s'/'%s'\n", tzname[0],
               tzname[1]);
        return 1;
    }
    (void)setenv("TZ", "<+0530>-5:30", 1);
    tzset();
    struct tm tm_ist = {0};
    if (strcmp(tzname[0], "+0530") != 0 || !localtime_r(&epoch0, &tm_ist) ||
        tm_ist.tm_hour != 5 || tm_ist.tm_min != 30 ||
        tm_ist.tm_gmtoff != 19800L) {
        printf("[FAIL] timetest tzset(<+0530>-5:30) mismatch\n");
        return 1;
    }

    /* localtime_r + mktime with POSIX TZ offset (EST5 = UTC-5). */
    (void)setenv("TZ", "EST5", 1);
    struct tm tml = {0};
    if (!localtime_r(&leap_t, &tml) || tml.tm_hour != 7 ||
        tml.tm_gmtoff != -18000L || !tml.tm_zone ||
        strcmp(tml.tm_zone, "EST") != 0 || strcmp(tzname[0], "EST") != 0) {
        printf("[FAIL] timetest localtime_r(EST5) hour=%d off=%ld\n",
               tml.tm_hour, tml.tm_gmtoff);
        return 1;
    }
    struct tm norm = tml;
    norm.tm_mday += 1; /* Feb 29 + 1 -> Mar 1 */
    time_t mk_res = mktime(&norm);
    if (mk_res != leap_t + 86400L || norm.tm_mon != 2 || norm.tm_mday != 1) {
        printf("[FAIL] timetest mktime normalization mismatch\n");
        return 1;
    }
    (void)unsetenv("TZ");
    tzset();

    char abuf[32] = {0};
    char cbuf[32] = {0};
    if (!asctime_r(&tm0, abuf) ||
        strcmp(abuf, "Thu Jan  1 00:00:00 1970\n") != 0 ||
        strcmp(asctime(&tm0), abuf) != 0 || !ctime_r(&epoch0, cbuf) ||
        strcmp(cbuf, abuf) != 0 || strcmp(ctime(&epoch0), abuf) != 0) {
        printf("[FAIL] timetest asctime/ctime got '%s'\n", abuf);
        return 1;
    }

    char sfbuf[128] = {0};
    size_t sfn = strftime(sfbuf, sizeof(sfbuf),
                          "%Y-%m-%d %H:%M:%S %a %b %j %u %G-W%V %z %Z %%", tmg);
    if (sfn == 0u ||
        strcmp(sfbuf, "2024-02-29 12:34:56 Thu Feb 060 4 2024-W09 +0000 UTC "
                      "%") != 0) {
        printf("[FAIL] timetest strftime got '%s'\n", sfbuf);
        return 1;
    }
    if (strftime(sfbuf, 4u, "%Y-%m-%d", tmg) != 0u) {
        printf("[FAIL] timetest strftime overflow did not return 0\n");
        return 1;
    }
    printf("[OK] Base time.h calendar, tzset/tzname, mktime, asctime/ctime, "
           "strftime, clock, and timespec_get verified\n");
    return 0;
}
