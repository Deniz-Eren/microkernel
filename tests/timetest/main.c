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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
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
    return 0;
}
