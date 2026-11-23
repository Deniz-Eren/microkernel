/**
 * @file     uniq.c
 * @brief    Ring 3 uniq utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

static const char *skip_key(const char *s, long skip_fields, long skip_chars) {
    const char *p = s;
    for (long f = 0; f < skip_fields && *p != '\0'; f++) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
    }
    for (long c = 0; c < skip_chars && *p != '\0'; c++) {
        p++;
    }
    return p;
}

static int lines_equal(const char *a, const char *b, long skip_fields,
                       long skip_chars, int ignore_case) {
    const char *ka = skip_key(a, skip_fields, skip_chars);
    const char *kb = skip_key(b, skip_fields, skip_chars);
    return ignore_case ? (strcasecmp(ka, kb) == 0) : (strcmp(ka, kb) == 0);
}

static void emit_line(FILE *out, const char *line, unsigned long count,
                      int opt_c, int opt_d, int opt_u) {
    if (opt_d && count < 2ul) {
        return;
    }
    if (opt_u && count != 1ul) {
        return;
    }
    if (opt_c) {
        (void)fprintf(out, "%lu %s\n", count, line);
    } else {
        (void)fprintf(out, "%s\n", line);
    }
}

int main(int argc, char **argv) {
    int opt_c = 0;
    int opt_d = 0;
    int opt_u = 0;
    int opt_i = 0;
    long skip_fields = 0;
    long skip_chars = 0;
    int opt;
    while ((opt = getopt(argc, argv, "cduif:s:")) != -1) {
        switch (opt) {
        case 'c':
            opt_c = 1;
            break;
        case 'd':
            opt_d = 1;
            break;
        case 'u':
            opt_u = 1;
            break;
        case 'i':
            opt_i = 1;
            break;
        case 'f':
            skip_fields = strtol(optarg, NULL, 10);
            if (skip_fields < 0) {
                skip_fields = 0;
            }
            break;
        case 's':
            skip_chars = strtol(optarg, NULL, 10);
            if (skip_chars < 0) {
                skip_chars = 0;
            }
            break;
        default:
            return 1;
        }
    }

    FILE *in = stdin;
    FILE *out = stdout;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        in = fopen(argv[optind], "r");
        if (!in) {
            perror(argv[optind]);
            return 1;
        }
    }
    if (optind + 1 < argc && strcmp(argv[optind + 1], "-") != 0) {
        out = fopen(argv[optind + 1], "w");
        if (!out) {
            perror(argv[optind + 1]);
            if (in != stdin) {
                (void)fclose(in);
            }
            return 1;
        }
    }

    char *prev = NULL;
    unsigned long count = 0ul;
    char *line = NULL;
    size_t lcap = 0u;
    ssize_t n;
    while ((n = getline(&line, &lcap, in)) >= 0) {
        size_t len = (size_t)n;
        if (len > 0u && line[len - 1u] == '\n') {
            line[--len] = '\0';
        }
        if (prev && lines_equal(prev, line, skip_fields, skip_chars, opt_i)) {
            count++;
            continue;
        }
        if (prev) {
            emit_line(out, prev, count, opt_c, opt_d, opt_u);
            free(prev);
        }
        prev = (char *)malloc(len + 1u);
        if (!prev) {
            free(line);
            if (in != stdin) {
                (void)fclose(in);
            }
            if (out != stdout) {
                (void)fclose(out);
            }
            return 1;
        }
        memcpy(prev, line, len + 1u);
        count = 1ul;
    }
    if (prev) {
        emit_line(out, prev, count, opt_c, opt_d, opt_u);
        free(prev);
    }
    free(line);
    if (in != stdin) {
        (void)fclose(in);
    }
    if (out != stdout) {
        (void)fclose(out);
    }
    return 0;
}
