/**
 * @file     pathchk.c
 * @brief    POSIX.1-2024 XCU pathchk utility (check pathnames for portability)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int is_portable_char(unsigned char ch) {
    return isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
}

static int check_path(const char *path, int opt_p, int opt_P) {
    size_t path_lim = opt_p ? (size_t)_POSIX_PATH_MAX : (size_t)PATH_MAX;
    size_t name_lim = opt_p ? (size_t)_POSIX_NAME_MAX : (size_t)NAME_MAX;
    size_t len = strlen(path);

    if (len == 0u) {
        if (opt_P) {
            (void)fprintf(stderr, "pathchk: empty pathname\n");
            return 1;
        }
        return 0;
    }
    if (len >= path_lim) {
        (void)fprintf(stderr, "pathchk: path exceeds limit: %s\n", path);
        return 1;
    }

    const char *p = path;
    while (*p) {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        const char *start = p;
        while (*p && *p != '/') {
            if (opt_p && !is_portable_char((unsigned char)*p)) {
                (void)fprintf(stderr, "pathchk: non-portable char in %s\n",
                              path);
                return 1;
            }
            p++;
        }
        size_t clen = (size_t)(p - start);
        if (clen > name_lim) {
            (void)fprintf(stderr, "pathchk: component exceeds limit in %s\n",
                          path);
            return 1;
        }
        if (opt_P && start[0] == '-') {
            (void)fprintf(stderr, "pathchk: leading '-' in %s\n", path);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int opt_p = 0;
    int opt_P = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "pP")) != -1) {
        if (c == 'p') {
            opt_p = 1;
        } else if (c == 'P') {
            opt_P = 1;
        } else {
            (void)fprintf(stderr, "pathchk: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: pathchk [-p] [-P] pathname...\n");
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (check_path(argv[i], opt_p, opt_P) != 0) {
            rc = 1;
        }
    }
    return rc;
}
