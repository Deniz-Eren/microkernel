/**
 * @file     du.c
 * @brief    POSIX.1-2024 XCU du utility (estimate file space usage)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int g_opt_a;
static int g_opt_s;
static int g_opt_k;
static int g_opt_x;

static unsigned long scale_blocks(unsigned long b512) {
    if (g_opt_k) {
        return (b512 + 1UL) / 2UL;
    }
    return b512;
}

static int walk_du(const char *path, dev_t root_dev, int depth,
                   unsigned long *out_blocks) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        perror(path);
        *out_blocks = 0UL;
        return 1;
    }
    if (depth == 0) {
        root_dev = st.st_dev;
    } else if (g_opt_x && st.st_dev != root_dev) {
        *out_blocks = 0UL;
        return 0;
    }

    unsigned long total = (unsigned long)st.st_blocks;
    if (total == 0UL && st.st_size > 0) {
        total = ((unsigned long)st.st_size + 511UL) / 512UL;
    }

    int rc = 0;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) {
            perror(path);
            rc = 1;
        } else {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                if (strcmp(ent->d_name, ".") == 0 ||
                    strcmp(ent->d_name, "..") == 0) {
                    continue;
                }
                char child[PATH_MAX];
                if (strcmp(path, "/") == 0) {
                    (void)snprintf(child, sizeof(child), "/%s", ent->d_name);
                } else {
                    (void)snprintf(child, sizeof(child), "%s/%s", path,
                                   ent->d_name);
                }
                unsigned long sub = 0UL;
                if (walk_du(child, root_dev, depth + 1, &sub) != 0) {
                    rc = 1;
                }
                total += sub;
            }
            (void)closedir(d);
        }
        if (depth == 0 || !g_opt_s) {
            (void)printf("%lu\t%s\n", scale_blocks(total), path);
        }
    } else if (depth == 0 || (g_opt_a && !g_opt_s)) {
        (void)printf("%lu\t%s\n", scale_blocks(total), path);
    }

    *out_blocks = total;
    return rc;
}

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "aksxHL")) != -1) {
        if (c == 'a') {
            g_opt_a = 1;
        } else if (c == 'k') {
            g_opt_k = 1;
        } else if (c == 's') {
            g_opt_s = 1;
        } else if (c == 'x') {
            g_opt_x = 1;
        } else if (c != 'H' && c != 'L') {
            (void)fprintf(stderr, "du: invalid option -%c\n", optopt);
            return 1;
        }
    }

    if (optind >= argc) {
        unsigned long total = 0UL;
        return walk_du(".", 0UL, 0, &total);
    }
    int rc = 0;
    for (int i = optind; i < argc; i++) {
        unsigned long total = 0UL;
        if (walk_du(argv[i], 0UL, 0, &total) != 0) {
            rc = 1;
        }
    }
    return rc;
}
