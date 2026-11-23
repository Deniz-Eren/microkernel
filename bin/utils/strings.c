/**
 * @file     strings.c
 * @brief    Ring 3 strings utility targeting POSIX.1-2024 XCU (-a, -n, -t)
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
#include <unistd.h>

/* need:: COMP_LIBC_0023 */

static void emit_string(const char *buf, size_t len, unsigned long offset,
                        char fmt) {
    if (fmt == 'd') {
        (void)printf("%lu ", offset);
    } else if (fmt == 'o') {
        (void)printf("%lo ", offset);
    } else if (fmt == 'x') {
        (void)printf("%lx ", offset);
    }
    (void)fwrite(buf, 1, len, stdout);
    (void)putchar('\n');
}

static int scan_stream(FILE *fp, size_t min_len, char fmt) {
    size_t cap = 256;
    char *buf = malloc(cap);
    if (!buf) {
        return -1;
    }
    size_t len = 0;
    unsigned long pos = 0;
    unsigned long start = 0;
    int ch = 0;
    while ((ch = fgetc(fp)) != EOF) {
        if (isprint(ch) || ch == '\t') {
            if (len == 0) {
                start = pos;
            }
            if (len + 1 >= cap) {
                size_t ncap = cap * 2;
                char *nb = realloc(buf, ncap);
                if (!nb) {
                    free(buf);
                    return -1;
                }
                buf = nb;
                cap = ncap;
            }
            buf[len++] = (char)ch;
        } else {
            if (len >= min_len) {
                emit_string(buf, len, start, fmt);
            }
            len = 0;
        }
        pos++;
    }
    if (len >= min_len) {
        emit_string(buf, len, start, fmt);
    }
    free(buf);
    return ferror(fp) ? -1 : 0;
}

int main(int argc, char **argv) {
    size_t min_len = 4;
    char fmt = '\0';
    int opt = 0;

    while ((opt = getopt(argc, argv, "an:t:")) != -1) {
        switch (opt) {
        case 'a':
            break;
        case 'n': {
            char *end = NULL;
            long v = strtol(optarg, &end, 10);
            if (!end || *end != '\0' || v <= 0) {
                (void)fprintf(stderr, "strings: invalid length: %s\n", optarg);
                return 1;
            }
            min_len = (size_t)v;
            break;
        }
        case 't':
            if (strcmp(optarg, "d") == 0 || strcmp(optarg, "o") == 0 ||
                strcmp(optarg, "x") == 0) {
                fmt = optarg[0];
            } else {
                (void)fprintf(stderr, "strings: invalid -t format: %s\n",
                              optarg);
                return 1;
            }
            break;
        default:
            (void)fprintf(stderr, "usage: strings [-a] [-n number] [-t d|o|x] "
                                  "[file...]\n");
            return 1;
        }
    }

    if (optind >= argc) {
        return (scan_stream(stdin, min_len, fmt) == 0) ? 0 : 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        FILE *fp = stdin;
        if (strcmp(argv[i], "-") != 0) {
            fp = fopen(argv[i], "rb");
            if (!fp) {
                perror(argv[i]);
                rc = 1;
                continue;
            }
        }
        if (scan_stream(fp, min_len, fmt) != 0) {
            rc = 1;
        }
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
