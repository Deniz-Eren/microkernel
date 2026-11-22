/**
 * @file     cp.c
 * @brief    Ring 3 cp utility targeting POSIX.1-2024 XCU (-r, -R)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

static int copy_file(const char *src, const char *dst, mode_t mode) {
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        perror(src);
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                   mode & 0777u ? (mode & 0777u) : 0644u);
    if (out < 0) {
        perror(dst);
        (void)close(in);
        return -1;
    }
    char buf[512];
    ssize_t n;
    int rc = 0;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(out, buf + off, (size_t)(n - off));
            if (w <= 0) {
                perror(dst);
                rc = -1;
                break;
            }
            off += w;
        }
        if (rc != 0) {
            break;
        }
    }
    if (n < 0) {
        perror(src);
        rc = -1;
    }
    (void)close(out);
    (void)close(in);
    return rc;
}

static int copy_tree(const char *src, const char *dst, int recursive) {
    struct stat st = {0};
    if (stat(src, &st) != 0) {
        perror(src);
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (!recursive) {
            (void)fprintf(stderr, "cp: omitting directory '%s'\n", src);
            return -1;
        }
        if (mkdir(dst, 0755u) != 0 && errno != EEXIST) {
            perror(dst);
            return -1;
        }
        DIR *dp = opendir(src);
        if (!dp) {
            perror(src);
            return -1;
        }
        int rc = 0;
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                continue;
            }
            char s_child[PATH_MAX];
            char d_child[PATH_MAX];
            int n1 =
                snprintf(s_child, sizeof(s_child), "%s/%s", src, de->d_name);
            int n2 =
                snprintf(d_child, sizeof(d_child), "%s/%s", dst, de->d_name);
            if (n1 < 0 || (size_t)n1 >= sizeof(s_child) || n2 < 0 ||
                (size_t)n2 >= sizeof(d_child)) {
                rc = -1;
                continue;
            }
            if (copy_tree(s_child, d_child, recursive) != 0) {
                rc = -1;
            }
        }
        (void)closedir(dp);
        return rc;
    }
    return copy_file(src, dst, st.st_mode);
}

int main(int argc, char **argv) {
    int recursive = 0;
    int opt;
    while ((opt = getopt(argc, argv, "rR")) != -1) {
        if (opt == 'r' || opt == 'R') {
            recursive = 1;
        } else {
            return 1;
        }
    }
    int npaths = argc - optind;
    if (npaths < 2) {
        (void)fprintf(stderr, "cp: missing file operand\n");
        return 1;
    }
    const char *dst = argv[argc - 1];
    struct stat dst_st = {0};
    int dst_is_dir = (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode));
    if (npaths > 2 && !dst_is_dir) {
        (void)fprintf(stderr, "cp: target '%s' is not a directory\n", dst);
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
        if (copy_tree(argv[i], target, recursive) != 0) {
            rc = 1;
        }
    }
    return rc;
}
