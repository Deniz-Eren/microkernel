/**
 * @file     expand.c
 * @brief    Ring 3 expand utility targeting POSIX.1-2024 XCU (-t tablist)
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

static size_t spaces_for_tab(size_t col) {
    if (g_ntabs == 0u) {
        return 8u - (col % 8u);
    }
    if (g_ntabs == 1u) {
        return g_tabs[0] - (col % g_tabs[0]);
    }
    for (size_t i = 0u; i < g_ntabs; i++) {
        if (g_tabs[i] > col) {
            return g_tabs[i] - col;
        }
    }
    return 1u;
}

static void expand_stream(FILE *fp) {
    size_t col = 0u;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\t') {
            size_t n = spaces_for_tab(col);
            for (size_t i = 0u; i < n; i++) {
                (void)putchar(' ');
            }
            col += n;
        } else {
            (void)putchar(ch);
            if (ch == '\n') {
                col = 0u;
            } else if (ch == '\b') {
                if (col > 0u) {
                    col--;
                }
            } else {
                col++;
            }
        }
    }
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        if (opt == 't') {
            if (parse_tabs(optarg) != 0) {
                (void)fprintf(stderr, "expand: invalid tab list\n");
                return 1;
            }
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        expand_stream(stdin);
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
        expand_stream(fp);
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
