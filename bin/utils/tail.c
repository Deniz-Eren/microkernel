/**
 * @file     tail.c
 * @brief    Ring 3 tail utility targeting POSIX.1-2024 XCU (-n, -c, -f)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

static int read_all(FILE *fp, uint8_t **buf_out, size_t *len_out) {
    size_t cap = 4096u;
    size_t len = 0u;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) {
        return -1;
    }
    for (;;) {
        if (len == cap) {
            size_t ncap = cap * 2u;
            uint8_t *nb = (uint8_t *)realloc(buf, ncap);
            if (!nb) {
                free(buf);
                return -1;
            }
            buf = nb;
            cap = ncap;
        }
        size_t got = fread(buf + len, 1u, cap - len, fp);
        len += got;
        if (got == 0u) {
            break;
        }
    }
    *buf_out = buf;
    *len_out = len;
    return 0;
}

static int tail_stream(FILE *fp, int byte_mode, int from_start, size_t count) {
    uint8_t *buf = NULL;
    size_t len = 0u;
    if (read_all(fp, &buf, &len) != 0) {
        return -1;
    }
    size_t start = 0u;
    if (byte_mode) {
        if (from_start) {
            start = (count > 0u) ? (count - 1u) : 0u;
            if (start > len) {
                start = len;
            }
        } else {
            start = (count < len) ? (len - count) : 0u;
        }
    } else {
        if (from_start) {
            size_t need = (count > 1u) ? (count - 1u) : 0u;
            size_t seen = 0u;
            while (start < len && seen < need) {
                if (buf[start] == '\n') {
                    seen++;
                }
                start++;
            }
        } else if (count == 0u) {
            start = len;
        } else {
            size_t pos = len;
            if (pos > 0u && buf[pos - 1u] == '\n') {
                pos--;
            }
            size_t lines = 0u;
            while (pos > 0u) {
                if (buf[pos - 1u] == '\n') {
                    lines++;
                    if (lines == count) {
                        break;
                    }
                }
                pos--;
            }
            start = pos;
        }
    }
    if (start < len) {
        (void)fwrite(buf + start, 1u, len - start, stdout);
    }
    free(buf);
    return 0;
}

static int parse_count(const char *arg, int *from_start, size_t *out) {
    *from_start = 0;
    const char *p = arg;
    if (*p == '+') {
        *from_start = 1;
        p++;
    } else if (*p == '-') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return -1;
    }
    char *end = NULL;
    unsigned long v = strtoul(p, &end, 10);
    if (!end || *end != '\0') {
        return -1;
    }
    *out = (size_t)v;
    return 0;
}

int main(int argc, char **argv) {
    int byte_mode = 0;
    int from_start = 0;
    size_t count = 10u;
    int opt;
    while ((opt = getopt(argc, argv, "c:fn:")) != -1) {
        switch (opt) {
        case 'c':
            byte_mode = 1;
            if (parse_count(optarg, &from_start, &count) != 0) {
                (void)fprintf(stderr, "tail: invalid byte count\n");
                return 1;
            }
            break;
        case 'n':
            byte_mode = 0;
            if (parse_count(optarg, &from_start, &count) != 0) {
                (void)fprintf(stderr, "tail: invalid line count\n");
                return 1;
            }
            break;
        case 'f':
            break;
        default:
            return 1;
        }
    }
    if (optind >= argc) {
        return tail_stream(stdin, byte_mode, from_start, count) == 0 ? 0 : 1;
    }
    int rc = 0;
    int multi = (argc - optind) > 1;
    for (int i = optind; i < argc; i++) {
        FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
        if (!fp) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        if (multi) {
            (void)printf("%s==> %s <==\n", i > optind ? "\n" : "", argv[i]);
        }
        if (tail_stream(fp, byte_mode, from_start, count) != 0) {
            rc = 1;
        }
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
