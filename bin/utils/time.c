/**
 * @file     time.c
 * @brief    POSIX.1-2024 XCU time utility (report command timing statistics)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0008 */

static void print_secs(const char *label, unsigned long ms) {
    unsigned long whole = ms / 1000UL;
    unsigned long milli = ms % 1000UL;
    unsigned long cents = (milli + 5UL) / 10UL;
    if (cents >= 100UL) {
        whole += 1UL;
        cents = 0UL;
    }
    (void)fprintf(stderr, "%s %lu.%02lu\n", label, whole, cents);
}

int main(int argc, char **argv) {
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        if (strcmp(argv[argi], "--") == 0) {
            argi++;
            break;
        }
        for (const char *p = argv[argi] + 1; *p; p++) {
            if (*p != 'p') {
                (void)fprintf(stderr, "usage: time [-p] utility [arg...]\n");
                return 2;
            }
        }
        argi++;
    }
    if (argi >= argc) {
        (void)fprintf(stderr, "usage: time [-p] utility [arg...]\n");
        return 2;
    }

    struct timespec t0 = {0};
    struct tms before = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &t0);
    (void)times(&before);

    pid_t pid = fork();
    if (pid < 0) {
        perror("time: fork");
        return 1;
    }
    if (pid == 0) {
        (void)execvp(argv[argi], &argv[argi]);
        int exec_err = errno;
        (void)fprintf(stderr, "time: cannot execute %s\n", argv[argi]);
        _exit(exec_err == ENOENT ? 127 : 126);
    }

    int wstatus = 0;
    while (waitpid(pid, &wstatus, 0) < 0 && errno == EINTR) {
    }

    struct timespec t1 = {0};
    struct tms after = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &t1);
    (void)times(&after);

    long long diff_ns = (long long)(t1.tv_sec - t0.tv_sec) * 1000000000LL +
                        (long long)(t1.tv_nsec - t0.tv_nsec);
    if (diff_ns < 0) {
        diff_ns = 0;
    }
    unsigned long real_ms = (unsigned long)(diff_ns / 1000000LL);
    unsigned long user_ms =
        (after.tms_cutime >= before.tms_cutime)
            ? (unsigned long)(after.tms_cutime - before.tms_cutime)
            : 0UL;
    unsigned long sys_ms =
        (after.tms_cstime >= before.tms_cstime)
            ? (unsigned long)(after.tms_cstime - before.tms_cstime)
            : 0UL;
    print_secs("real", real_ms);
    print_secs("user", user_ms);
    print_secs("sys", sys_ms);

    if (WIFEXITED(wstatus)) {
        return WEXITSTATUS(wstatus);
    }
    if (WIFSIGNALED(wstatus)) {
        return 128 + WTERMSIG(wstatus);
    }
    return 0;
}
