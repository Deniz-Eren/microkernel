/**
 * @file     mv.c
 * @brief    Ring 3 mv utility targeting POSIX.1-2024 XCU using rename()
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "f")) != -1) {
        if (opt != 'f') {
            return 1;
        }
    }
    int npaths = argc - optind;
    if (npaths < 2) {
        (void)fprintf(stderr, "mv: missing file operand\n");
        return 1;
    }
    const char *dst = argv[argc - 1];
    struct stat dst_st = {0};
    int dst_is_dir = (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));
    if (npaths > 2 && !dst_is_dir) {
        (void)fprintf(stderr, "mv: target '%s' is not a directory\n", dst);
        return 1;
    }
    int rc = 0;
    for (int i = optind; i < argc - 1; i++) {
        char target[PATH_MAX];
        if (dst_is_dir) {
            int n = snprintf(target, sizeof(target), "%s/%s", dst,
                             base_name(argv[i]));
            if (n < 0 || (size_t)n >= sizeof(target)) {
                rc = 1;
                continue;
            }
        } else {
            (void)strlcpy(target, dst, sizeof(target));
        }
        if (rename(argv[i], target) != 0) {
            perror(argv[i]);
            rc = 1;
        }
    }
    return rc;
}
