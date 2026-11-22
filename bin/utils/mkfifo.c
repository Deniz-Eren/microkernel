/**
 * @file     mkfifo.c
 * @brief    POSIX.1-2024 XCU mkfifo utility (make FIFO special files)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

int main(int argc, char **argv) {
    mode_t mode = 0666u;
    int explicit_mode = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "m:")) != -1) {
        if (c == 'm') {
            char *end = NULL;
            errno = 0;
            unsigned long v = strtoul(optarg, &end, 8);
            if (errno != 0 || end == optarg || *end != '\0' || v > 0777UL) {
                (void)fprintf(stderr, "mkfifo: invalid mode: %s\n", optarg);
                return 1;
            }
            mode = (mode_t)v;
            explicit_mode = 1;
        } else {
            (void)fprintf(stderr, "mkfifo: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: mkfifo [-m mode] file...\n");
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (mkfifo(argv[i], mode) != 0) {
            perror(argv[i]);
            rc = 1;
        } else if (explicit_mode && chmod(argv[i], mode) != 0) {
            perror(argv[i]);
            rc = 1;
        }
    }
    return rc;
}
