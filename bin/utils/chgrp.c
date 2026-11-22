/**
 * @file     chgrp.c
 * @brief    POSIX.1-2024 XCU chgrp utility (change file group ownership)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int parse_gid(const char *s, gid_t *out) {
    struct group *gr = getgrnam(s);
    if (gr) {
        *out = gr->gr_gid;
        return 0;
    }
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = (gid_t)v;
    return 0;
}

static int apply_chgrp(const char *path, gid_t gid, int nofollow,
                       int recursive) {
    int rc = 0;
    uid_t keep = (uid_t)-1;
    if ((nofollow ? lchown(path, keep, gid) : chown(path, keep, gid)) != 0) {
        perror(path);
        rc = 1;
    }
    if (recursive) {
        struct stat st;
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *d = opendir(path);
            if (!d) {
                perror(path);
                return 1;
            }
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 ||
                    strcmp(ent->d_name, "..") == 0) {
                    continue;
                }
                char child[PATH_MAX];
                (void)snprintf(child, sizeof(child), "%s/%s", path,
                               ent->d_name);
                if (apply_chgrp(child, gid, nofollow, 1) != 0) {
                    rc = 1;
                }
            }
            (void)closedir(d);
        }
    }
    return rc;
}

int main(int argc, char **argv) {
    int recursive = 0;
    int nofollow = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "RhHLP")) != -1) {
        if (c == 'R') {
            recursive = 1;
        } else if (c == 'h' || c == 'P') {
            nofollow = 1;
        } else if (c == 'H' || c == 'L') {
            nofollow = 0;
        } else {
            (void)fprintf(stderr, "chgrp: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (argc - optind < 2) {
        (void)fprintf(stderr, "usage: chgrp [-Rh] group file...\n");
        return 1;
    }

    gid_t gid = 0u;
    if (parse_gid(argv[optind], &gid) != 0) {
        (void)fprintf(stderr, "chgrp: unknown group: %s\n", argv[optind]);
        return 1;
    }
    int rc = 0;
    for (int i = optind + 1; i < argc; i++) {
        if (apply_chgrp(argv[i], gid, nofollow, recursive) != 0) {
            rc = 1;
        }
    }
    return rc;
}
