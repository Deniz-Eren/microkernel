/**
 * @file     echo.c
 * @brief    POSIX.1-2024 XCU echo utility writing arguments to stdout
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>

/* need:: COMP_LIBC_0005 */

int main(int argc, char **argv) {
    int i = 1;
    int newline = 1;
    if (i < argc && strcmp(argv[i], "-n") == 0) {
        newline = 0;
        i++;
    }
    for (int first = 1; i < argc; i++, first = 0) {
        if (!first) {
            if (putchar(' ') == EOF) {
                return 1;
            }
        }
        if (fputs(argv[i], stdout) == EOF) {
            return 1;
        }
    }
    if (newline && putchar('\n') == EOF) {
        return 1;
    }
    return fflush(stdout) == 0 ? 0 : 1;
}
