/**
 * @file     readlink.c
 * @brief    POSIX.1-2024 XCU readlink utility (display symlink target)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

int main(int argc, char **argv) {
    int no_nl = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "n")) != -1) {
        if (c == 'n') {
            no_nl = 1;
        } else {
            (void)fprintf(stderr, "readlink: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (argc - optind != 1) {
        (void)fprintf(stderr, "usage: readlink [-n] file\n");
        return 1;
    }

    char buf[PATH_MAX];
    ssize_t n = readlink(argv[optind], buf, sizeof(buf) - 1u);
    if (n < 0) {
        perror(argv[optind]);
        return 1;
    }
    buf[n] = '\0';
    (void)fputs(buf, stdout);
    if (!no_nl) {
        (void)putchar('\n');
    }
    return 0;
}
