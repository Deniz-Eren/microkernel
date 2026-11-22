/**
 * @file     getopt.c
 * @brief    POSIX.1-2024 getopt() command-line option parser
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0006 */

char *optarg = 0;
int opterr = 1;
int optind = 1;
int optopt = 0;

static int g_optpos = 1;

int getopt(int argc, char *const argv[], const char *optstring) {
    if (optind == 0) {
        optind = 1;
        g_optpos = 1;
    }
    optarg = 0;
    if (!argv || !optstring || optind >= argc || !argv[optind]) {
        g_optpos = 1;
        return -1;
    }

    const char *cur = argv[optind];
    if (g_optpos == 1) {
        if (cur[0] != '-' || cur[1] == '\0') {
            return -1;
        }
        if (cur[1] == '-' && cur[2] == '\0') {
            optind++;
            return -1;
        }
    }

    int ch = (unsigned char)cur[g_optpos];
    if (ch == '\0') {
        optind++;
        g_optpos = 1;
        return getopt(argc, argv, optstring);
    }

    int silent = (optstring[0] == ':');
    const char *spec = (ch == ':') ? 0 : strchr(optstring, ch);
    if (!spec) {
        optopt = ch;
        if (cur[g_optpos + 1] != '\0') {
            g_optpos++;
        } else {
            optind++;
            g_optpos = 1;
        }
        if (opterr && !silent) {
            (void)fprintf(stderr, "%s: illegal option -- %c\n",
                          argc > 0 && argv[0] ? argv[0] : "getopt", ch);
        }
        return '?';
    }

    if (spec[1] == ':') {
        if (cur[g_optpos + 1] != '\0') {
            optarg = (char *)(cur + g_optpos + 1);
            optind++;
            g_optpos = 1;
            return ch;
        }
        if (optind + 1 < argc) {
            optarg = argv[optind + 1];
            optind += 2;
            g_optpos = 1;
            return ch;
        }
        optopt = ch;
        optind++;
        g_optpos = 1;
        if (silent) {
            return ':';
        }
        if (opterr) {
            (void)fprintf(stderr, "%s: option requires an argument -- %c\n",
                          argc > 0 && argv[0] ? argv[0] : "getopt", ch);
        }
        return '?';
    }

    if (cur[g_optpos + 1] != '\0') {
        g_optpos++;
    } else {
        optind++;
        g_optpos = 1;
    }
    return ch;
}
