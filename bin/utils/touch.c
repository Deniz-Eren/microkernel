/**
 * @file     touch.c
 * @brief    POSIX.1-2024 XCU touch utility (create or update file timestamps)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

int main(int argc, char **argv) {
    int no_create = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "acm")) != -1) {
        if (c == 'c') {
            no_create = 1;
        } else if (c != 'a' && c != 'm') {
            (void)fprintf(stderr, "touch: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: touch [-acm] file...\n");
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) == 0) {
            if (!S_ISDIR(st.st_mode)) {
                (void)chmod(argv[i], st.st_mode & 07777u);
            }
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
