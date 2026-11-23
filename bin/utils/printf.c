/**
 * @file     printf.c
 * @brief    Ring 3 printf utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0018 */

static int parse_escape(const char **pp, int b_mode, int *stop_out) {
    const char *p = *pp;
    char c = *p++;
    switch (c) {
    case 'a':
        *pp = p;
        return '\a';
    case 'b':
        *pp = p;
        return '\b';
    case 'f':
        *pp = p;
        return '\f';
    case 'n':
        *pp = p;
        return '\n';
    case 'r':
        *pp = p;
        return '\r';
    case 't':
        *pp = p;
        return '\t';
    case 'v':
        *pp = p;
        return '\v';
    case '\\':
    case '\'':
    case '"':
        *pp = p;
        return (unsigned char)c;
    case 'c':
        if (b_mode) {
            *stop_out = 1;
            *pp = p;
            return -1;
        }
        *pp = p;
        return 'c';
    default:
        if (c >= '0' && c <= '7') {
            unsigned v = 0u;
            int max_d = 3;
            if (b_mode && c == '0') {
                max_d = 3;
            } else {
                v = (unsigned)(c - '0');
                max_d = 2;
            }
            for (int k = 0; k < max_d && *p >= '0' && *p <= '7'; k++) {
                v = v * 8u + (unsigned)(*p++ - '0');
            }
            *pp = p;
            return (int)(v & 0xFFu);
        }
        *pp = p;
        return (unsigned char)c;
    }
}

static long long arg_to_ll(const char *s, int *had_err) {
    if (!s || s[0] == '\0') {
        return 0;
    }
    if ((s[0] == '\'' || s[0] == '"') && s[1] != '\0') {
        return (unsigned char)s[1];
    }
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 0);
    if (errno != 0 || !end || *end != '\0') {
        fprintf(stderr, "printf: %s: invalid number\n", s);
        *had_err = 1;
    }
    return v;
}

static unsigned long long arg_to_ull(const char *s, int *had_err) {
    if (!s || s[0] == '\0') {
        return 0ull;
    }
    if ((s[0] == '\'' || s[0] == '"') && s[1] != '\0') {
        return (unsigned char)s[1];
    }
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno != 0 || !end || *end != '\0') {
        fprintf(stderr, "printf: %s: invalid number\n", s);
        *had_err = 1;
    }
    return v;
}

static double arg_to_double(const char *s, int *had_err) {
    if (!s || s[0] == '\0') {
        return 0.0;
    }
    if ((s[0] == '\'' || s[0] == '"') && s[1] != '\0') {
        return (double)(unsigned char)s[1];
    }
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || !end || *end != '\0') {
        fprintf(stderr, "printf: %s: invalid number\n", s);
        *had_err = 1;
    }
    return v;
}

static const char *next_arg(int argc, char **argv, int *ai) {
    if (*ai < argc) {
        return argv[(*ai)++];
    }
    return "";
}

int main(int argc, char **argv) {
    int idx = 1;
    if (idx < argc && strcmp(argv[idx], "--") == 0) {
        idx++;
    }
    if (idx >= argc) {
        fprintf(stderr, "usage: printf format [argument...]\n");
        return 1;
    }
    const char *fmt = argv[idx++];
    int ai = idx;
    int had_err = 0;
    int stop = 0;

    do {
        int start_ai = ai;
        for (const char *p = fmt; *p != '\0' && !stop;) {
            if (*p == '\\' && p[1] != '\0') {
                p++;
                int ch = parse_escape(&p, 0, &stop);
                if (ch >= 0) {
                    putchar(ch);
                }
                continue;
            }
            if (*p != '%') {
                putchar(*p++);
                continue;
            }
            p++;
            if (*p == '%') {
                putchar('%');
                p++;
                continue;
            }

            char spec[64];
            size_t slen = 0u;
            spec[slen++] = '%';
            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' ||
                   *p == '0') {
                if (slen + 4u < sizeof(spec)) {
                    spec[slen++] = *p;
                }
                p++;
            }
            int has_w_star = 0;
            int w_val = 0;
            if (*p == '*') {
                has_w_star = 1;
                w_val = (int)arg_to_ll(next_arg(argc, argv, &ai), &had_err);
                spec[slen++] = '*';
                p++;
            } else {
                while (isdigit((unsigned char)*p) && slen + 4u < sizeof(spec)) {
                    spec[slen++] = *p++;
                }
            }
            int has_p_star = 0;
            int p_val = 0;
            if (*p == '.') {
                if (slen + 4u < sizeof(spec)) {
                    spec[slen++] = *p;
                }
                p++;
                if (*p == '*') {
                    has_p_star = 1;
                    p_val = (int)arg_to_ll(next_arg(argc, argv, &ai), &had_err);
                    spec[slen++] = '*';
                    p++;
                } else {
                    while (isdigit((unsigned char)*p) &&
                           slen + 4u < sizeof(spec)) {
                        spec[slen++] = *p++;
                    }
                }
            }
            char conv = *p ? *p++ : '\0';
            if (conv == 'b') {
                const char *s = next_arg(argc, argv, &ai);
                for (const char *q = s; *q != '\0' && !stop;) {
                    if (*q == '\\' && q[1] != '\0') {
                        q++;
                        int ch = parse_escape(&q, 1, &stop);
                        if (ch >= 0) {
                            putchar(ch);
                        }
                    } else {
                        putchar(*q++);
                    }
                }
            } else if (conv == 's') {
                const char *s = next_arg(argc, argv, &ai);
                spec[slen++] = 's';
                spec[slen] = '\0';
                if (has_w_star && has_p_star) {
                    printf(spec, w_val, p_val, s);
                } else if (has_w_star) {
                    printf(spec, w_val, s);
                } else if (has_p_star) {
                    printf(spec, p_val, s);
                } else {
                    printf(spec, s);
                }
            } else if (conv == 'c') {
                const char *s = next_arg(argc, argv, &ai);
                int ch = s[0] ? (unsigned char)s[0] : 0;
                spec[slen++] = 'c';
                spec[slen] = '\0';
                if (has_w_star) {
                    printf(spec, w_val, ch);
                } else {
                    printf(spec, ch);
                }
            } else if (conv == 'd' || conv == 'i') {
                long long v = arg_to_ll(next_arg(argc, argv, &ai), &had_err);
                spec[slen++] = 'l';
                spec[slen++] = 'l';
                spec[slen++] = conv;
                spec[slen] = '\0';
                if (has_w_star && has_p_star) {
                    printf(spec, w_val, p_val, v);
                } else if (has_w_star) {
                    printf(spec, w_val, v);
                } else if (has_p_star) {
                    printf(spec, p_val, v);
                } else {
                    printf(spec, v);
                }
            } else if (conv == 'u' || conv == 'o' || conv == 'x' ||
                       conv == 'X') {
                unsigned long long v =
                    arg_to_ull(next_arg(argc, argv, &ai), &had_err);
                spec[slen++] = 'l';
                spec[slen++] = 'l';
                spec[slen++] = conv;
                spec[slen] = '\0';
                if (has_w_star && has_p_star) {
                    printf(spec, w_val, p_val, v);
                } else if (has_w_star) {
                    printf(spec, w_val, v);
                } else if (has_p_star) {
                    printf(spec, p_val, v);
                } else {
                    printf(spec, v);
                }
            } else if (conv == 'f' || conv == 'F' || conv == 'e' ||
                       conv == 'E' || conv == 'g' || conv == 'G') {
                double v = arg_to_double(next_arg(argc, argv, &ai), &had_err);
                spec[slen++] = conv;
                spec[slen] = '\0';
                if (has_w_star && has_p_star) {
                    printf(spec, w_val, p_val, v);
                } else if (has_w_star) {
                    printf(spec, w_val, v);
                } else if (has_p_star) {
                    printf(spec, p_val, v);
                } else {
                    printf(spec, v);
                }
            }
        }
        if (ai == start_ai) {
            break;
        }
    } while (!stop && ai < argc);

    return had_err ? 1 : 0;
}
