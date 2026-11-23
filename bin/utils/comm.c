/**
 * @file     comm.c
 * @brief    Ring 3 comm utility targeting POSIX.1-2024 XCU (-1, -2, -3)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0021 */

static int read_next_line(FILE *fp, char **buf, size_t *cap, size_t *len_out) {
    ssize_t n = getline(buf, cap, fp);
    if (n < 0) {
        return 0;
    }
    size_t len = (size_t)n;
    if (len > 0u && (*buf)[len - 1u] == '\n') {
        (*buf)[--len] = '\0';
    }
    *len_out = len;
    return 1;
}

static void emit_col(int col, const char *s, int s1, int s2, int s3) {
    if ((col == 1 && s1) || (col == 2 && s2) || (col == 3 && s3)) {
        return;
    }
    int tabs = 0;
    if (col == 2 && !s1) {
        tabs = 1;
    } else if (col == 3) {
        tabs = (!s1 ? 1 : 0) + (!s2 ? 1 : 0);
    }
    for (int i = 0; i < tabs; i++) {
        (void)putchar('\t');
    }
    (void)fputs(s, stdout);
    (void)putchar('\n');
}

int main(int argc, char **argv) {
    int s1 = 0;
    int s2 = 0;
    int s3 = 0;
    int opt;
    while ((opt = getopt(argc, argv, "123")) != -1) {
        if (opt == '1') {
            s1 = 1;
        } else if (opt == '2') {
            s2 = 1;
        } else if (opt == '3') {
            s3 = 1;
        } else {
            return 1;
        }
    }
    if (argc - optind != 2) {
        (void)fprintf(stderr, "comm: expected two file operands\n");
        return 1;
    }

    const char *p1 = argv[optind];
    const char *p2 = argv[optind + 1];
    FILE *f1 = (strcmp(p1, "-") == 0) ? stdin : fopen(p1, "r");
    if (!f1) {
        perror(p1);
        return 1;
    }
    FILE *f2 = (strcmp(p2, "-") == 0) ? stdin : fopen(p2, "r");
    if (!f2) {
        perror(p2);
        if (f1 != stdin) {
            (void)fclose(f1);
        }
        return 1;
    }

    char *l1 = NULL;
    char *l2 = NULL;
    size_t c1 = 0u;
    size_t c2 = 0u;
    size_t n1 = 0u;
    size_t n2 = 0u;
    int h1 = read_next_line(f1, &l1, &c1, &n1);
    int h2 = read_next_line(f2, &l2, &c2, &n2);

    while (h1 || h2) {
        if (!h2) {
            emit_col(1, l1, s1, s2, s3);
            h1 = read_next_line(f1, &l1, &c1, &n1);
        } else if (!h1) {
            emit_col(2, l2, s1, s2, s3);
            h2 = read_next_line(f2, &l2, &c2, &n2);
        } else {
            int cmp = strcmp(l1, l2);
            if (cmp < 0) {
                emit_col(1, l1, s1, s2, s3);
                h1 = read_next_line(f1, &l1, &c1, &n1);
            } else if (cmp > 0) {
                emit_col(2, l2, s1, s2, s3);
                h2 = read_next_line(f2, &l2, &c2, &n2);
            } else {
                emit_col(3, l1, s1, s2, s3);
                h1 = read_next_line(f1, &l1, &c1, &n1);
                h2 = read_next_line(f2, &l2, &c2, &n2);
            }
        }
    }

    free(l1);
    free(l2);
    if (f1 != stdin) {
        (void)fclose(f1);
    }
    if (f2 != stdin) {
        (void)fclose(f2);
    }
    return 0;
}
