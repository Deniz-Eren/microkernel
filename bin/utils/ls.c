/**
 * @file     ls.c
 * @brief    POSIX.1-2024 XCU ls utility listing directory entries
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* need:: COMP_LIBC_0005 */

static void print_entry(const char *dir, const char *name, int long_fmt) {
    if (!long_fmt) {
        printf("%s\n", name);
        return;
    }
    char full[PATH_MAX];
    if (strcmp(dir, "/") == 0) {
        (void)snprintf(full, sizeof(full), "/%s", name);
    } else if (dir[0]) {
        (void)snprintf(full, sizeof(full), "%s/%s", dir, name);
    } else {
        (void)strlcpy(full, name, sizeof(full));
    }
    struct stat st = {0};
    if (lstat(full, &st) == 0) {
        char kind = S_ISDIR(st.st_mode) ? 'd' : '-';
        printf("%c %8ld %s\n", kind, (long)st.st_size, name);
    } else {
        printf("? %8d %s\n", 0, name);
    }
}

static int list_path(const char *path, int show_all, int long_fmt) {
    struct stat st = {0};
    if (stat(path, &st) != 0) {
        perror(path);
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        print_entry("", path, long_fmt);
        return 0;
    }
    DIR *d = opendir(path);
    if (!d) {
        perror(path);
        return 1;
    }

    char **names = NULL;
    size_t count = 0;
    size_t cap = 0;
    struct dirent *de = NULL;
    while ((de = readdir(d)) != NULL) {
        if (!show_all && de->d_name[0] == '.') {
            continue;
        }
        if (count == cap) {
            size_t ncap = (cap == 0u) ? 64u : cap * 2u;
            char **nn = realloc(names, ncap * sizeof(*nn));
            if (!nn) {
                break;
            }
            names = nn;
            cap = ncap;
        }
        char *dup = strdup(de->d_name);
        if (!dup) {
            break;
        }
        names[count++] = dup;
    }
    (void)closedir(d);

    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1u; j < count; j++) {
            if (strcmp(names[i], names[j]) > 0) {
                char *tmp = names[i];
                names[i] = names[j];
                names[j] = tmp;
            }
        }
    }

    for (size_t i = 0; i < count; i++) {
        print_entry(path, names[i], long_fmt);
        free(names[i]);
    }
    free(names);
    return 0;
}

int main(int argc, char **argv) {
    int show_all = 0;
    int long_fmt = 0;
    int idx = 1;
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        for (size_t k = 1; argv[idx][k]; k++) {
            if (argv[idx][k] == 'a') {
                show_all = 1;
            } else if (argv[idx][k] == 'l') {
                long_fmt = 1;
            } else {
                fprintf(stderr, "ls: unknown option -%c\n", argv[idx][k]);
                return 1;
            }
        }
        idx++;
    }
    if (idx >= argc) {
        return list_path(".", show_all, long_fmt);
    }
    int status = 0;
    for (; idx < argc; idx++) {
        if (list_path(argv[idx], show_all, long_fmt) != 0) {
            status = 1;
        }
    }
    return status;
}
