/**
 * @file     nice.c
 * @brief    POSIX.1-2024 XCU nice utility (invoke utility with altered nice)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>
#include <unistd.h>

/* need:: COMP_PROC_0010 */

int main(int argc, char **argv) {
    int inc = 10;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "n:")) != -1) {
        if (c == 'n') {
            char *end = NULL;
            errno = 0;
            long v = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0') {
                (void)fprintf(stderr, "nice: invalid increment: %s\n", optarg);
                return 125;
            }
            inc = (int)v;
        } else {
            (void)fprintf(stderr, "nice: invalid option -%c\n", optopt);
            return 125;
        }
    }

    if (optind >= argc) {
        long cur = ProcGetNice(0);
        if (cur < 0) {
            (void)fprintf(stderr, "nice: failed to query nice value\n");
            return 125;
        }
        (void)printf("%ld\n", cur - 20L);
        return 0;
    }

    long r = ProcSetNice(0, inc, 1);
    if (r < 0) {
        (void)fprintf(stderr, "nice: warning: cannot adjust nice (%ld)\n", -r);
    }

    (void)execvp(argv[optind], &argv[optind]);
    int err = errno;
    (void)fprintf(stderr, "nice: cannot execute %s\n", argv[optind]);
    return (err == ENOENT) ? 127 : 126;
}
