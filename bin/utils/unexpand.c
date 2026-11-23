/**
 * @file     unexpand.c
 * @brief    Ring 3 unexpand utility targeting POSIX.1-2024 XCU (-a, -t)
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

#define MAX_TABS 64

static size_t g_tabs[MAX_TABS];
static size_t g_ntabs;

static int parse_tabs(const char *spec) {
    g_ntabs = 0u;
    const char *p = spec;
    while (*p != '\0') {
        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (g_ntabs >= MAX_TABS) {
            return -1;
        }
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        if (end == p || v == 0ul) {
            return -1;
        }
        if (g_ntabs > 0u && v <= g_tabs[g_ntabs - 1u]) {
            return -1;
        }
        g_tabs[g_ntabs++] = (size_t)v;
        p = end;
        if (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t') {
            return -1;
        }
    }
    return g_ntabs > 0u ? 0 : -1;
}

static size_t next_tab_stop(size_t col) {
    if (g_ntabs == 0u) {
        return col + (8u - (col % 8u));
    }
    if (g_ntabs == 1u) {
        return col + (g_tabs[0] - (col % g_tabs[0]));
    }
    for (size_t i = 0u; i < g_ntabs; i++) {
        if (g_tabs[i] > col) {
            return g_tabs[i];
        }
    }
    return 0u; /* past last explicit tab stop */
}

static void unexpand_line(const char *s, size_t len, int convert_all) {
    size_t col = 0u;
    size_t i = 0u;
    int leading = 1;
    while (i < len) {
        if ((leading || convert_all) && (s[i] == ' ' || s[i] == '\t')) {
            size_t start_col = col;
            size_t blank_chars = 0u;
            while (i < len && (s[i] == ' ' || s[i] == '\t')) {
                if (s[i] == ' ') {
                    col++;
                } else {
                    size_t ns = next_tab_stop(col);
                    col = (ns > col) ? ns : (col + 1u);
                }
                blank_chars++;
                i++;
            }
            if (blank_chars == 1u && s[i - 1u] == ' ') {
                (void)putchar(' ');
                continue;
            }
            size_t cur = start_col;
            while (cur < col) {
                size_t ns = next_tab_stop(cur);
                if (ns > cur && ns <= col) {
                    (void)putchar('\t');
                    cur = ns;
                } else {
                    (void)putchar(' ');
                    cur++;
                }
            }
            continue;
        }
        leading = 0;
        (void)putchar((unsigned char)s[i]);
        if (s[i] == '\b') {
            if (col > 0u) {
                col--;
            }
        } else {
            col++;
        }
        i++;
    }
}

static void unexpand_stream(FILE *fp, int convert_all) {
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
        unexpand_line(line, len, convert_all);
        if (had_nl) {
            (void)putchar('\n');
        }
    }
    free(line);
}

int main(int argc, char **argv) {
    int convert_all = 0;
    int opt;
    while ((opt = getopt(argc, argv, "at:")) != -1) {
        if (opt == 'a') {
            convert_all = 1;
        } else if (opt == 't') {
            if (parse_tabs(optarg) != 0) {
                (void)fprintf(stderr, "unexpand: invalid tab list\n");
                return 1;
            }
            convert_all = 1;
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        unexpand_stream(stdin, convert_all);
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
        unexpand_stream(fp, convert_all);
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
