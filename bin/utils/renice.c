/**
 * @file     renice.c
 * @brief    POSIX.1-2024 XCU renice utility (set nice values of processes)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>
#include <unistd.h>

/* need:: COMP_PROC_0010 */

int main(int argc, char **argv) {
    int has_n = 0;
    int inc = 0;
    int mode = 'p'; /* 'p' = pid, 'g' = pgid, 'u' = user */

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "n:gpu")) != -1) {
        switch (c) {
        case 'n': {
            char *end = NULL;
            errno = 0;
            long v = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0') {
                (void)fprintf(stderr, "renice: invalid increment: %s\n",
                              optarg);
                return 1;
            }
            inc = (int)v;
            has_n = 1;
            break;
        }
        case 'g':
        case 'p':
        case 'u':
            mode = c;
            break;
        default:
            (void)fprintf(stderr, "renice: invalid option -%c\n", optopt);
            return 1;
        }
    }

    if (!has_n || optind >= argc) {
        (void)fprintf(stderr, "usage: renice -n increment [-g|-p|-u] ID...\n");
        return 1;
    }

    struct __k_proc_info list[PROC_LIST_MAX];
    memset(list, 0, sizeof(list));
    long nproc = 0;
    if (mode != 'p') {
        nproc = ProcList(list, PROC_LIST_MAX);
        if (nproc < 0) {
            (void)fprintf(stderr, "renice: cannot list processes\n");
            return 1;
        }
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (mode == 'p') {
            char *end = NULL;
            errno = 0;
            unsigned long pid = strtoul(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' || pid == 0UL) {
                (void)fprintf(stderr, "renice: invalid pid: %s\n", argv[i]);
                rc = 1;
                continue;
            }
            if (ProcSetNice((int)pid, inc, 1) < 0) {
                (void)fprintf(stderr, "renice: failed for pid %lu\n", pid);
                rc = 1;
            }
        } else if (mode == 'g') {
            char *end = NULL;
            errno = 0;
            unsigned long pgid = strtoul(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0') {
                (void)fprintf(stderr, "renice: invalid pgid: %s\n", argv[i]);
                rc = 1;
                continue;
            }
            int matched = 0;
            for (long j = 0; j < nproc; j++) {
                if ((unsigned long)list[j].pgid == pgid) {
                    matched = 1;
                    if (ProcSetNice((int)list[j].pid, inc, 1) < 0) {
                        rc = 1;
                    }
                }
            }
            if (!matched) {
                rc = 1;
            }
        } else {
            uid_t target_uid = 0u;
            struct passwd *pw = getpwnam(argv[i]);
            if (pw) {
                target_uid = pw->pw_uid;
            } else {
                char *end = NULL;
                errno = 0;
                unsigned long u = strtoul(argv[i], &end, 10);
                if (errno != 0 || end == argv[i] || *end != '\0') {
                    (void)fprintf(stderr, "renice: unknown user: %s\n",
                                  argv[i]);
                    rc = 1;
                    continue;
                }
                target_uid = (uid_t)u;
            }
            int matched = 0;
            for (long j = 0; j < nproc; j++) {
                if (list[j].uid == target_uid) {
                    matched = 1;
                    if (ProcSetNice((int)list[j].pid, inc, 1) < 0) {
                        rc = 1;
                    }
                }
            }
            if (!matched) {
                rc = 1;
            }
        }
    }
    return rc;
}
