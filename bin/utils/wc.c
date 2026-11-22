/**
 * @file     wc.c
 * @brief    Ring 3 wc utility targeting POSIX.1-2024 XCU (-l, -w, -c)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

static void count_stream(FILE *fp, unsigned long *lines, unsigned long *words,
                         unsigned long *bytes) {
    unsigned long l = 0;
    unsigned long w = 0;
    unsigned long c = 0;
    int in_word = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        c++;
        if (ch == '\n') {
            l++;
        }
        if (isspace(ch)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            w++;
        }
    }
    *lines = l;
    *words = w;
    *bytes = c;
}

static void print_counts(int show_l, int show_w, int show_c, unsigned long l,
                         unsigned long w, unsigned long c, const char *name) {
    int first = 1;
    if (show_l) {
        (void)printf("%lu", l);
        first = 0;
    }
    if (show_w) {
        (void)printf("%s%lu", first ? "" : " ", w);
        first = 0;
    }
    if (show_c) {
        (void)printf("%s%lu", first ? "" : " ", c);
    }
    if (name) {
        (void)printf(" %s", name);
    }
    (void)putchar('\n');
}

int main(int argc, char **argv) {
    int show_l = 0;
    int show_w = 0;
    int show_c = 0;
    int opt;
    while ((opt = getopt(argc, argv, "lwc")) != -1) {
        if (opt == 'l') {
            show_l = 1;
        } else if (opt == 'w') {
            show_w = 1;
        } else if (opt == 'c') {
            show_c = 1;
        } else {
            return 1;
        }
    }
    if (!show_l && !show_w && !show_c) {
        show_l = 1;
        show_w = 1;
        show_c = 1;
    }
    if (optind >= argc) {
        unsigned long l = 0, w = 0, c = 0;
        count_stream(stdin, &l, &w, &c);
        print_counts(show_l, show_w, show_c, l, w, c, NULL);
        return 0;
    }
    int rc = 0;
    unsigned long tl = 0, tw = 0, tc = 0;
    int nfiles = argc - optind;
    for (int i = optind; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        unsigned long l = 0, w = 0, c = 0;
        count_stream(fp, &l, &w, &c);
        (void)fclose(fp);
        print_counts(show_l, show_w, show_c, l, w, c, argv[i]);
        tl += l;
        tw += w;
        tc += c;
    }
    if (nfiles > 1) {
        print_counts(show_l, show_w, show_c, tl, tw, tc, "total");
    }
    return rc;
}
