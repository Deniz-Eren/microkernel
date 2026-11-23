/**
 * @file     fold.c
 * @brief    Ring 3 fold utility targeting POSIX.1-2024 XCU (-b, -s, -w)
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

/* need:: COMP_LIBC_0023 */

static size_t next_col(size_t col, unsigned char c, int byte_mode) {
    if (byte_mode) {
        return col + 1u;
    }
    if (c == '\t') {
        return col + (8u - (col % 8u));
    }
    if (c == '\b') {
        return col > 0u ? (col - 1u) : 0u;
    }
    if (c == '\r') {
        return 0u;
    }
    return col + 1u;
}

static void fold_line(const char *s, size_t len, size_t width, int byte_mode,
                      int spaces, int had_nl) {
    size_t pos = 0u;
    while (pos < len) {
        size_t col = 0u;
        size_t i = pos;
        size_t last_blank = (size_t)-1;
        while (i < len) {
            unsigned char c = (unsigned char)s[i];
            size_t nc = next_col(col, c, byte_mode);
            if (nc > width && i > pos) {
                break;
            }
            col = nc;
            if (spaces && (c == ' ' || c == '\t')) {
                last_blank = i + 1u;
            }
            i++;
        }
        size_t cut = i;
        if (i < len && spaces && last_blank != (size_t)-1 && last_blank > pos) {
            cut = last_blank;
        }
        if (cut == pos) {
            cut = pos + 1u;
        }
        (void)fwrite(s + pos, 1u, cut - pos, stdout);
        if (cut < len) {
            (void)putchar('\n');
        }
        pos = cut;
    }
    if (had_nl) {
        (void)putchar('\n');
    }
}

static void fold_stream(FILE *fp, size_t width, int byte_mode, int spaces) {
    char *line = NULL;
    size_t cap = 0u;
    ssize_t n;
    while ((n = getline(&line, &cap, fp)) >= 0) {
        size_t len = (size_t)n;
        int had_nl = 0;
        if (len > 0u && line[len - 1u] == '\n') {
            len--;
            had_nl = 1;
        }
        fold_line(line, len, width, byte_mode, spaces, had_nl);
    }
    free(line);
}

int main(int argc, char **argv) {
    size_t width = 80u;
    int byte_mode = 0;
    int spaces = 0;
    int opt;
    while ((opt = getopt(argc, argv, "bsw:")) != -1) {
        if (opt == 'b') {
            byte_mode = 1;
        } else if (opt == 's') {
            spaces = 1;
        } else if (opt == 'w') {
            unsigned long w = strtoul(optarg, NULL, 10);
            if (w == 0ul) {
                (void)fprintf(stderr, "fold: invalid width\n");
                return 1;
            }
            width = (size_t)w;
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        fold_stream(stdin, width, byte_mode, spaces);
        return 0;
    }
    int rc = 0;
    for (int i = optind; i < argc; i++) {
        FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
        if (!fp) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        fold_stream(fp, width, byte_mode, spaces);
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
