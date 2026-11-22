/**
 * @file     rm.c
 * @brief    Ring 3 rm utility targeting POSIX.1-2024 XCU (-f, -r, -R)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

static int remove_path(const char *path, int recursive, int force) {
    struct stat st = {0};
    if (lstat(path, &st) != 0) {
        if (force && errno == ENOENT) {
            return 0;
        }
        perror(path);
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            (void)fprintf(stderr, "rm: %s: is a directory\n", path);
            return -1;
        }
        for (;;) {
            DIR *dp = opendir(path);
            if (!dp) {
                perror(path);
                return -1;
            }
            char child[PATH_MAX];
            child[0] = '\0';
            struct dirent *de;
            while ((de = readdir(dp)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 ||
                    strcmp(de->d_name, "..") == 0) {
                    continue;
                }
                int n =
                    snprintf(child, sizeof(child), "%s/%s", path, de->d_name);
                if (n > 0 && (size_t)n < sizeof(child)) {
                    break;
                }
                child[0] = '\0';
            }
            (void)closedir(dp);
            if (child[0] == '\0') {
                break;
            }
            if (remove_path(child, recursive, force) != 0) {
                return -1;
            }
        }
        if (rmdir(path) != 0) {
            perror(path);
            return -1;
        }
        return 0;
    }
    if (unlink(path) != 0) {
        if (force && errno == ENOENT) {
            return 0;
        }
        perror(path);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int recursive = 0;
    int force = 0;
    int opt;
    while ((opt = getopt(argc, argv, "frR")) != -1) {
        if (opt == 'f') {
            force = 1;
        } else if (opt == 'r' || opt == 'R') {
            recursive = 1;
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        if (force) {
            return 0;
        }
        (void)fprintf(stderr, "rm: missing operand\n");
        return 1;
    }
    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (remove_path(argv[i], recursive, force) != 0) {
            rc = 1;
        }
    }
    return rc;
}
