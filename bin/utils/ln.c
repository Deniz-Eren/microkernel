/**
 * @file     ln.c
 * @brief    POSIX.1-2024 XCU ln utility (link files)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static const char *path_base(const char *p) {
    const char *slash = strrchr(p, '/');
    return slash ? (slash + 1) : p;
}

static int make_link(const char *src, const char *dst, int symbolic, int force,
                     int follow) {
    if (force) {
        (void)unlink(dst);
    }
    if (symbolic) {
        if (symlink(src, dst) != 0) {
            perror(dst);
            return 1;
        }
        return 0;
    }
    int flags = follow ? AT_SYMLINK_FOLLOW : 0;
    if (linkat(AT_FDCWD, src, AT_FDCWD, dst, flags) != 0) {
        perror(dst);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int symbolic = 0;
    int force = 0;
    int follow = 0;

    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "fsLP")) != -1) {
        if (c == 's') {
            symbolic = 1;
        } else if (c == 'f') {
            force = 1;
        } else if (c == 'L') {
            follow = 1;
        } else if (c == 'P') {
            follow = 0;
        } else {
            (void)fprintf(stderr, "ln: invalid option -%c\n", optopt);
            return 1;
        }
    }

    int rem = argc - optind;
    if (rem < 2) {
        (void)fprintf(stderr, "usage: ln [-fs] [-L|-P] source target\n");
        return 1;
    }
    const char *target = argv[argc - 1];
    struct stat st;
    int target_is_dir = (stat(target, &st) == 0 && S_ISDIR(st.st_mode));
    if (rem > 2 && !target_is_dir) {
        (void)fprintf(stderr, "ln: target '%s' is not a directory\n", target);
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc - 1; i++) {
        char dst[PATH_MAX];
        if (target_is_dir) {
            (void)snprintf(dst, sizeof(dst), "%s/%s", target,
                           path_base(argv[i]));
        } else {
            (void)strlcpy(dst, target, sizeof(dst));
        }
        if (make_link(argv[i], dst, symbolic, force, follow) != 0) {
            rc = 1;
        }
    }
    return rc;
}
