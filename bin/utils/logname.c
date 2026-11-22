/**
 * @file     logname.c
 * @brief    POSIX.1-2024 XCU logname utility (return user's login name)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <unistd.h>

/* need:: COMP_PROC_0010 */

int main(int argc, char **argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "")) != -1) {
        (void)fprintf(stderr, "logname: invalid option -%c\n", optopt);
        return 1;
    }
    if (optind < argc) {
        (void)fprintf(stderr, "logname: unexpected operand: %s\n",
                      argv[optind]);
        return 1;
    }

    char *name = getlogin();
    if (!name || !name[0]) {
        (void)fprintf(stderr, "logname: no login name\n");
        return 1;
    }
    (void)puts(name);
    return 0;
}
