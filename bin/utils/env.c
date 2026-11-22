/**
 * @file     env.c
 * @brief    Ring 3 env utility targeting POSIX.1-2024 XCU (-i, NAME=VALUE, cmd)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

int main(int argc, char **argv) {
    int ignore_env = 0;
    int idx = 1;
    while (idx < argc) {
        if (strcmp(argv[idx], "-i") == 0 || strcmp(argv[idx], "-") == 0) {
            ignore_env = 1;
            idx++;
        } else if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        } else {
            break;
        }
    }
    if (ignore_env) {
        static char *empty_env[1] = {NULL};
        environ = empty_env;
    }
    while (idx < argc && strchr(argv[idx], '=') != NULL) {
        char *eq = strchr(argv[idx], '=');
        *eq = '\0';
        (void)setenv(argv[idx], eq + 1, 1);
        *eq = '=';
        idx++;
    }
    if (idx < argc) {
        (void)execvp(argv[idx], &argv[idx]);
        int err = errno;
        perror(argv[idx]);
        return err == ENOENT ? 127 : 126;
    }
    if (environ) {
        for (char **e = environ; *e; e++) {
            (void)puts(*e);
        }
    }
    return 0;
}
