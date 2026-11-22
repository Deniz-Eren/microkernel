/**
 * @file     rmdir.c
 * @brief    POSIX.1-2024 XCU rmdir utility (remove empty directories)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int remove_parents(const char *path) {
    char buf[PATH_MAX];
    (void)strlcpy(buf, path, sizeof(buf));
    for (;;) {
        size_t len = strlen(buf);
        while (len > 1u && buf[len - 1u] == '/') {
            buf[--len] = '\0';
        }
        if (rmdir(buf) != 0) {
            perror(buf);
            return 1;
        }
        char *slash = strrchr(buf, '/');
        if (!slash || slash == buf) {
            break;
        }
        *slash = '\0';
    }
    return 0;
}

int main(int argc, char **argv) {
    int parents = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "p")) != -1) {
        if (c == 'p') {
            parents = 1;
        } else {
            (void)fprintf(stderr, "rmdir: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: rmdir [-p] dir...\n");
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (parents) {
            if (remove_parents(argv[i]) != 0) {
                rc = 1;
            }
        } else if (rmdir(argv[i]) != 0) {
            perror(argv[i]);
            rc = 1;
        }
    }
    return rc;
}
