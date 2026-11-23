/**
 * @file     cut.c
 * @brief    Ring 3 cut utility targeting POSIX.1-2024 XCU (-b, -c, -f, -d, -s)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

#define MAX_RANGES 128

typedef struct {
    size_t lo;
    size_t hi; /* SIZE_MAX = open-ended */
} cut_range_t;

static cut_range_t g_ranges[MAX_RANGES];
static size_t g_nranges;

static int parse_ranges(const char *spec) {
    g_nranges = 0u;
    const char *p = spec;
    while (*p != '\0') {
        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (g_nranges >= MAX_RANGES) {
            return -1;
        }
        size_t lo = 1u;
        size_t hi = (size_t)-1;
        if (*p == '-') {
            p++;
            char *end = NULL;
            unsigned long v = strtoul(p, &end, 10);
            if (end == p || v == 0ul) {
                return -1;
            }
            hi = (size_t)v;
            p = end;
        } else {
            char *end = NULL;
            unsigned long v = strtoul(p, &end, 10);
            if (end == p || v == 0ul) {
                return -1;
            }
            lo = (size_t)v;
            p = end;
            if (*p == '-') {
                p++;
                if (*p >= '0' && *p <= '9') {
                    unsigned long v2 = strtoul(p, &end, 10);
                    if (end == p || v2 < lo) {
                        return -1;
                    }
                    hi = (size_t)v2;
                    p = end;
                }
            } else {
                hi = lo;
            }
        }
        if (*p != '\0' && *p != ',' && *p != ' ' && *p != '\t') {
            return -1;
        }
        g_ranges[g_nranges].lo = lo;
        g_ranges[g_nranges].hi = hi;
        g_nranges++;
    }
    return g_nranges > 0u ? 0 : -1;
}

static int is_selected(size_t pos) {
    for (size_t i = 0u; i < g_nranges; i++) {
        if (pos >= g_ranges[i].lo && pos <= g_ranges[i].hi) {
            return 1;
        }
    }
    return 0;
}

static void cut_stream(FILE *fp, int mode, int delim, int suppress) {
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
        if (mode == 'b' || mode == 'c') {
            for (size_t i = 0u; i < len; i++) {
                if (is_selected(i + 1u)) {
                    (void)putchar((unsigned char)line[i]);
                }
            }
            if (had_nl) {
                (void)putchar('\n');
            }
            continue;
        }
        /* Field mode (-f). */
        int has_delim = (memchr(line, delim, len) != NULL);
        if (!has_delim) {
            if (!suppress) {
                if (len > 0u) {
                    (void)fwrite(line, 1u, len, stdout);
                }
                if (had_nl) {
                    (void)putchar('\n');
                }
            }
            continue;
        }
        size_t field = 1u;
        size_t start = 0u;
        int printed = 0;
        for (size_t i = 0u; i <= len; i++) {
            if (i == len || (unsigned char)line[i] == (unsigned char)delim) {
                if (is_selected(field)) {
                    if (printed) {
                        (void)putchar(delim);
                    }
                    if (i > start) {
                        (void)fwrite(line + start, 1u, i - start, stdout);
                    }
                    printed = 1;
                }
                field++;
                start = i + 1u;
            }
        }
        if (had_nl) {
            (void)putchar('\n');
        }
    }
    free(line);
}

int main(int argc, char **argv) {
    int mode = 0;
    const char *list = NULL;
    int delim = '\t';
    int suppress = 0;
    int opt;
    while ((opt = getopt(argc, argv, "b:c:d:f:ns")) != -1) {
        switch (opt) {
        case 'b':
        case 'c':
        case 'f':
            mode = opt;
            list = optarg;
            break;
        case 'd':
            if (optarg[0] == '\0' || optarg[1] != '\0') {
                (void)fprintf(stderr, "cut: delimiter must be one byte\n");
                return 1;
            }
            delim = (unsigned char)optarg[0];
            break;
        case 'n':
            break;
        case 's':
            suppress = 1;
            break;
        default:
            return 1;
        }
    }
    if (mode == 0 || !list || parse_ranges(list) != 0) {
        (void)fprintf(stderr, "cut: invalid list specification\n");
        return 1;
    }
    if (optind >= argc) {
        cut_stream(stdin, mode, delim, suppress);
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
        cut_stream(fp, mode, delim, suppress);
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
