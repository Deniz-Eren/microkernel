/**
 * @file     touch.c
 * @brief    POSIX.1-2024 XCU touch utility (create or update file timestamps)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 COMP_LIBC_0011 */

int main(int argc, char **argv) {
    int no_create = 0;
    int set_a = 0;
    int set_m = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "acm")) != -1) {
        if (c == 'c') {
            no_create = 1;
        } else if (c == 'a') {
            set_a = 1;
        } else if (c == 'm') {
            set_m = 1;
        } else {
            (void)fprintf(stderr, "touch: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: touch [-acm] file...\n");
        return 1;
    }
    if (!set_a && !set_m) {
        set_a = 1;
        set_m = 1;
    }

    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = set_a ? UTIME_NOW : UTIME_OMIT;
    times[1].tv_sec = 0;
    times[1].tv_nsec = set_m ? UTIME_NOW : UTIME_OMIT;

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (utimensat(AT_FDCWD, argv[i], times, 0) == 0) {
            continue;
        }
        if (errno != ENOENT) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        if (no_create) {
            continue;
        }
        int fd = open(argv[i], O_WRONLY | O_CREAT | O_CLOEXEC, 0666u);
        if (fd < 0) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        (void)close(fd);
    }
    return rc;
}
