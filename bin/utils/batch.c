/**
 * @file     batch.c
 * @brief    Ring 3 POSIX.1-2024 XCU batch deferred-execution utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0027 */

int main(int argc, char **argv) {
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    if (argi < argc) {
        (void)fprintf(stderr, "usage: batch\n");
        return 1;
    }

    char *at_argv[] = {"at", "-q", "b", "-m", "now", NULL};
    (void)execv("/boot/at", at_argv);
    perror("/boot/at");
    return 126;
}
