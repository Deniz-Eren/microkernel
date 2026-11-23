/**
 * @file     tput.c
 * @brief    Ring 3 POSIX.1-2024 XCU tput terminal capability utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

static void get_term_size(int *rows, int *cols) {
    *rows = 24;
    *cols = 80;
    struct winsize ws = {0};
    if (tcgetwinsize(STDOUT_FILENO, &ws) == 0 ||
        tcgetwinsize(STDIN_FILENO, &ws) == 0 ||
        tcgetwinsize(STDERR_FILENO, &ws) == 0) {
        if (ws.ws_row > 0) {
            *rows = (int)ws.ws_row;
        }
        if (ws.ws_col > 0) {
            *cols = (int)ws.ws_col;
        }
    }
}

int main(int argc, char **argv) {
    const char *term = getenv("TERM");
    int opt = 0;
    while ((opt = getopt(argc, argv, "T:")) != -1) {
        if (opt == 'T') {
            term = optarg;
        } else {
            (void)fprintf(stderr, "usage: tput [-T type] operand...\n");
            return 2;
        }
    }
    if (!term || term[0] == '\0') {
        term = "vt100";
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: tput [-T type] operand...\n");
        return 2;
    }

    int rows = 24;
    int cols = 80;
    get_term_size(&rows, &cols);
    const char *cap = argv[optind];

    if (strcmp(cap, "clear") == 0) {
        (void)fputs("\033[H\033[2J", stdout);
        return 0;
    }
    if (strcmp(cap, "init") == 0 || strcmp(cap, "reset") == 0) {
        (void)fputs("\033c", stdout);
        return 0;
    }
    if (strcmp(cap, "cols") == 0) {
        (void)printf("%d\n", cols);
        return 0;
    }
    if (strcmp(cap, "lines") == 0) {
        (void)printf("%d\n", rows);
        return 0;
    }
    if (strcmp(cap, "colors") == 0) {
        (void)puts((strcmp(term, "dumb") == 0) ? "-1" : "8");
        return 0;
    }
    if (strcmp(cap, "cup") == 0) {
        int r = (optind + 1 < argc) ? atoi(argv[optind + 1]) : 0;
        int c = (optind + 2 < argc) ? atoi(argv[optind + 2]) : 0;
        (void)printf("\033[%d;%dH", r + 1, c + 1);
        return 0;
    }
    if (strcmp(cap, "home") == 0) {
        (void)fputs("\033[H", stdout);
        return 0;
    }
    if (strcmp(cap, "bold") == 0) {
        (void)fputs("\033[1m", stdout);
        return 0;
    }
    if (strcmp(cap, "dim") == 0) {
        (void)fputs("\033[2m", stdout);
        return 0;
    }
    if (strcmp(cap, "smul") == 0) {
        (void)fputs("\033[4m", stdout);
        return 0;
    }
    if (strcmp(cap, "rmul") == 0) {
        (void)fputs("\033[24m", stdout);
        return 0;
    }
    if (strcmp(cap, "rev") == 0) {
        (void)fputs("\033[7m", stdout);
        return 0;
    }
    if (strcmp(cap, "sgr0") == 0) {
        (void)fputs("\033[0m", stdout);
        return 0;
    }
    if (strcmp(cap, "setaf") == 0) {
        int n = (optind + 1 < argc) ? atoi(argv[optind + 1]) : 0;
        (void)printf("\033[3%dm", n & 7);
        return 0;
    }
    if (strcmp(cap, "setab") == 0) {
        int n = (optind + 1 < argc) ? atoi(argv[optind + 1]) : 0;
        (void)printf("\033[4%dm", n & 7);
        return 0;
    }
    if (strcmp(cap, "el") == 0) {
        (void)fputs("\033[K", stdout);
        return 0;
    }
    if (strcmp(cap, "ed") == 0) {
        (void)fputs("\033[J", stdout);
        return 0;
    }
    if (strcmp(cap, "civis") == 0) {
        (void)fputs("\033[?25l", stdout);
        return 0;
    }
    if (strcmp(cap, "cnorm") == 0) {
        (void)fputs("\033[?25h", stdout);
        return 0;
    }
    if (strcmp(cap, "bel") == 0) {
        (void)putchar('\a');
        return 0;
    }
    if (strcmp(cap, "cr") == 0) {
        (void)putchar('\r');
        return 0;
    }
    if (strcmp(cap, "ht") == 0) {
        (void)putchar('\t');
        return 0;
    }

    (void)fprintf(stderr, "tput: unknown terminfo capability '%s'\n", cap);
    return 4;
}
