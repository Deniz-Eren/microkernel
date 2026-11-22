/**
 * @file     realpath.c
 * @brief    POSIX.1-2024 XCU realpath utility (resolve a pathname)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "eE")) != -1) {
        if (c != 'e' && c != 'E') {
            (void)fprintf(stderr, "realpath: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (argc - optind != 1) {
        (void)fprintf(stderr, "usage: realpath [-E|-e] file\n");
        return 1;
    }

    char out[PATH_MAX];
    if (!realpath(argv[optind], out)) {
        perror(argv[optind]);
        return 1;
    }
    (void)puts(out);
    return 0;
}
