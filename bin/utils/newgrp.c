/**
 * @file     newgrp.c
 * @brief    Ring 3 POSIX.1-2024 XCU newgrp group-identity switch utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

static int user_in_group(const struct group *gr, const char *uname,
                         gid_t primary_gid) {
    if (getuid() == 0 || gr->gr_gid == primary_gid) {
        return 1;
    }
    if (gr->gr_mem && uname) {
        for (size_t i = 0; gr->gr_mem[i] != NULL; i++) {
            if (strcmp(gr->gr_mem[i], uname) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int login_env = 0;
    int opt = 0;
    while ((opt = getopt(argc, argv, "l")) != -1) {
        if (opt == 'l') {
            login_env = 1;
        } else {
            (void)fprintf(stderr, "usage: newgrp [-l] [group]\n");
            return 1;
        }
    }
    if (argc - optind > 1) {
        (void)fprintf(stderr, "usage: newgrp [-l] [group]\n");
        return 1;
    }

    struct passwd *pw = getpwuid(getuid());
    gid_t target_gid = pw ? pw->pw_gid : getgid();
    const char *uname = (pw && pw->pw_name) ? pw->pw_name : "root";

    if (optind < argc) {
        const char *garg = argv[optind];
        struct group *gr = getgrnam(garg);
        if (!gr && isdigit((unsigned char)garg[0])) {
            char *end = NULL;
            unsigned long v = strtoul(garg, &end, 10);
            if (end && *end == '\0') {
                gr = getgrgid((gid_t)v);
            }
        }
        if (!gr) {
            (void)fprintf(stderr, "newgrp: unknown group: %s\n", garg);
            return 1;
        }
        if (!user_in_group(gr, uname, target_gid)) {
            (void)fprintf(stderr, "newgrp: permission denied for group %s\n",
                          garg);
            return 1;
        }
        target_gid = gr->gr_gid;
    }

    if (setgid(target_gid) != 0) {
        perror("newgrp: setgid");
        return 1;
    }

    if (login_env && pw && pw->pw_dir) {
        (void)setenv("HOME", pw->pw_dir, 1);
        (void)chdir(pw->pw_dir);
    }

    const char *sh = getenv("SHELL");
    if (!sh || sh[0] == '\0') {
        sh = (pw && pw->pw_shell && pw->pw_shell[0] != '\0') ? pw->pw_shell
                                                             : "/boot/sh";
    }
    char *sh_argv[2];
    sh_argv[0] = (char *)(login_env ? "-sh" : sh);
    sh_argv[1] = NULL;
    (void)execv(sh, sh_argv);
    perror(sh);
    return 126;
}
