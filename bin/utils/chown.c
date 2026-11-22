/**
 * @file     chown.c
 * @brief    POSIX.1-2024 XCU chown utility (change file ownership)
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
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int parse_uid(const char *s, uid_t *out) {
    struct passwd *pw = getpwnam(s);
    if (pw) {
        *out = pw->pw_uid;
        return 0;
    }
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        return -1;
    }
    *out = (uid_t)v;
    return 0;
}

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

static int apply_chown(const char *path, uid_t uid, gid_t gid, int nofollow,
                       int recursive) {
    int rc = 0;
    if ((nofollow ? lchown(path, uid, gid) : chown(path, uid, gid)) != 0) {
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
                if (apply_chown(child, uid, gid, nofollow, 1) != 0) {
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
            (void)fprintf(stderr, "chown: invalid option -%c\n", optopt);
            return 1;
        }
    }
    if (argc - optind < 2) {
        (void)fprintf(stderr, "usage: chown [-Rh] owner[:group] file...\n");
        return 1;
    }

    char spec[64];
    (void)strlcpy(spec, argv[optind++], sizeof(spec));
    char *colon = strchr(spec, ':');
    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;
    if (colon) {
        *colon = '\0';
        if (colon[1] != '\0' && parse_gid(colon + 1, &gid) != 0) {
            (void)fprintf(stderr, "chown: unknown group: %s\n", colon + 1);
            return 1;
        }
    }
    if (spec[0] != '\0' && parse_uid(spec, &uid) != 0) {
        (void)fprintf(stderr, "chown: unknown user: %s\n", spec);
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (apply_chown(argv[i], uid, gid, nofollow, recursive) != 0) {
            rc = 1;
        }
    }
    return rc;
}
