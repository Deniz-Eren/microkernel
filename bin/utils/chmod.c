/**
 * @file     chmod.c
 * @brief    POSIX.1-2024 XCU chmod utility (change file mode bits)
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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int parse_mode(const char *spec, mode_t cur_mode, mode_t *out_mode) {
    if (spec[0] >= '0' && spec[0] <= '7') {
        char *end = NULL;
        errno = 0;
        unsigned long v = strtoul(spec, &end, 8);
        if (errno != 0 || end == spec || *end != '\0' || v > 07777UL) {
            return -1;
        }
        *out_mode = (mode_t)v;
        return 0;
    }

    mode_t mode = cur_mode & 07777u;
    mode_t um = umask(0u);
    (void)umask(um);
    const char *p = spec;
    while (*p) {
        unsigned who = 0u;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            if (*p == 'u') {
                who |= 1u;
            } else if (*p == 'g') {
                who |= 2u;
            } else if (*p == 'o') {
                who |= 4u;
            } else {
                who |= 7u;
            }
            p++;
        }
        int explicit_who = (who != 0u);
        if (!explicit_who) {
            who = 7u;
        }
        if (*p != '+' && *p != '-' && *p != '=') {
            return -1;
        }
        while (*p == '+' || *p == '-' || *p == '=') {
            char op = *p++;
            mode_t bits = 0u;
            while (*p == 'r' || *p == 'w' || *p == 'x' || *p == 'X' ||
                   *p == 's' || *p == 't') {
                char c = *p++;
                if (c == 'r') {
                    if (who & 1u) {
                        bits |= S_IRUSR;
                    }
                    if (who & 2u) {
                        bits |= S_IRGRP;
                    }
                    if (who & 4u) {
                        bits |= S_IROTH;
                    }
                } else if (c == 'w') {
                    if (who & 1u) {
                        bits |= S_IWUSR;
                    }
                    if (who & 2u) {
                        bits |= S_IWGRP;
                    }
                    if (who & 4u) {
                        bits |= S_IWOTH;
                    }
                } else if (c == 'x' || (c == 'X' && (S_ISDIR(cur_mode) ||
                                                     (cur_mode & 0111u)))) {
                    if (who & 1u) {
                        bits |= S_IXUSR;
                    }
                    if (who & 2u) {
                        bits |= S_IXGRP;
                    }
                    if (who & 4u) {
                        bits |= S_IXOTH;
                    }
                } else if (c == 's') {
                    if (who & 1u) {
                        bits |= S_ISUID;
                    }
                    if (who & 2u) {
                        bits |= S_ISGID;
                    }
                } else if (c == 't') {
                    bits |= S_ISVTX;
                }
            }
            if (!explicit_who) {
                bits &= ~um;
            }
            mode_t wmask = 0u;
            if (who & 1u) {
                wmask |= S_IRWXU | S_ISUID;
            }
            if (who & 2u) {
                wmask |= S_IRWXG | S_ISGID;
            }
            if (who & 4u) {
                wmask |= S_IRWXO | S_ISVTX;
            }
            if (op == '+') {
                mode |= bits;
            } else if (op == '-') {
                mode &= ~bits;
            } else {
                mode = (mode & ~wmask) | bits;
            }
        }
        if (*p == ',') {
            p++;
        } else if (*p != '\0') {
            return -1;
        }
    }
    *out_mode = mode;
    return 0;
}

static int apply_chmod(const char *path, const char *spec, int recursive) {
    struct stat st;
    if (stat(path, &st) != 0) {
        perror(path);
        return 1;
    }
    mode_t new_mode = 0u;
    if (parse_mode(spec, st.st_mode, &new_mode) != 0) {
        (void)fprintf(stderr, "chmod: invalid mode: %s\n", spec);
        return 1;
    }
    int rc = 0;
    if (chmod(path, new_mode) != 0) {
        perror(path);
        rc = 1;
    }
    if (recursive && S_ISDIR(st.st_mode)) {
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
            (void)snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            if (apply_chmod(child, spec, 1) != 0) {
                rc = 1;
            }
        }
        (void)closedir(d);
    }
    return rc;
}

int main(int argc, char **argv) {
    int recursive = 0;
    int idx = 1;
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-R") == 0) {
            recursive = 1;
            idx++;
        } else {
            /* Could be symbolic mode starting with '-' such as -w. */
            break;
        }
    }
    if (argc - idx < 2) {
        (void)fprintf(stderr, "usage: chmod [-R] mode file...\n");
        return 1;
    }
    const char *spec = argv[idx++];
    int rc = 0;
    for (; idx < argc; idx++) {
        if (apply_chmod(argv[idx], spec, recursive) != 0) {
            rc = 1;
        }
    }
    return rc;
}
