/**
 * @file     cmp.c
 * @brief    Ring 3 cmp utility targeting POSIX.1-2024 XCU (-l, -s)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0021 */

int main(int argc, char **argv) {
    int opt_l = 0;
    int opt_s = 0;
    int opt;
    while ((opt = getopt(argc, argv, "ls")) != -1) {
        if (opt == 'l') {
            opt_l = 1;
        } else if (opt == 's') {
            opt_s = 1;
        } else {
            return 2;
        }
    }
    if (argc - optind != 2) {
        if (!opt_s) {
            (void)fprintf(stderr, "cmp: expected two file operands\n");
        }
        return 2;
    }

    const char *n1 = argv[optind];
    const char *n2 = argv[optind + 1];
    FILE *f1 = (strcmp(n1, "-") == 0) ? stdin : fopen(n1, "rb");
    if (!f1) {
        if (!opt_s) {
            perror(n1);
        }
        return 2;
    }
    FILE *f2 = (strcmp(n2, "-") == 0) ? stdin : fopen(n2, "rb");
    if (!f2) {
        if (!opt_s) {
            perror(n2);
        }
        if (f1 != stdin) {
            (void)fclose(f1);
        }
        return 2;
    }

    unsigned long long byte_pos = 1ULL;
    unsigned long long line_no = 1ULL;
    int rc = 0;
    for (;;) {
        int c1 = fgetc(f1);
        int c2 = fgetc(f2);
        if (c1 == EOF && c2 == EOF) {
            break;
        }
        if (c1 == EOF || c2 == EOF) {
            rc = 1;
            if (!opt_s) {
                (void)fprintf(stderr, "cmp: EOF on %s\n",
                              (c1 == EOF) ? n1 : n2);
            }
            break;
        }
        if (c1 != c2) {
            rc = 1;
            if (opt_s) {
                break;
            }
            if (opt_l) {
                (void)printf("%llu %o %o\n", byte_pos, (unsigned)c1,
                             (unsigned)c2);
            } else {
                (void)printf("%s %s differ: char %llu, line %llu\n", n1, n2,
                             byte_pos, line_no);
                break;
            }
        }
        if (c1 == '\n') {
            line_no++;
        }
        byte_pos++;
    }

    if (f1 != stdin) {
        (void)fclose(f1);
    }
    if (f2 != stdin) {
        (void)fclose(f2);
    }
    return rc;
}
