/**
 * @file     mkdir.c
 * @brief    Ring 3 mkdir utility targeting POSIX.1-2024 XCU (-p, -m)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

static int mkdir_parents(const char *path, mode_t mode) {
    char buf[PATH_MAX];
    if (strlcpy(buf, path, sizeof(buf)) >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    size_t len = strlen(buf);
    while (len > 1u && buf[len - 1u] == '/') {
        buf[--len] = '\0';
    }
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0777u) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(buf, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int parents = 0;
    mode_t mode = 0777u;
    int opt;
    while ((opt = getopt(argc, argv, "pm:")) != -1) {
        if (opt == 'p') {
            parents = 1;
        } else if (opt == 'm') {
            mode = (mode_t)strtoul(optarg, NULL, 8);
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "mkdir: missing operand\n");
        return 1;
    }
    int rc = 0;
    for (int i = optind; i < argc; i++) {
        int r = parents ? mkdir_parents(argv[i], mode) : mkdir(argv[i], mode);
        if (r != 0) {
            perror(argv[i]);
            rc = 1;
        }
    }
    return rc;
}
