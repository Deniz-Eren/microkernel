/**
 * @file     tty.c
 * @brief    POSIX.1-2024 XCU tty utility (return user's terminal name)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <unistd.h>

/* need:: COMP_PROC_0009 */

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "")) != -1) {
        (void)fprintf(stderr, "tty: invalid option -%c\n", optopt);
        return 2;
    }
    if (optind < argc) {
        (void)fprintf(stderr, "tty: unexpected operand: %s\n", argv[optind]);
        return 2;
    }

    char *name = ttyname(STDIN_FILENO);
    if (!name) {
        (void)puts("not a tty");
        return 1;
    }
    (void)puts(name);
    return 0;
}
