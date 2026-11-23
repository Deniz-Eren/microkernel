/**
 * @file     tr.c
 * @brief    Ring 3 tr utility targeting POSIX.1-2024 XCU (-c, -C, -d, -s)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} byte_vec_t;

static int bv_push(byte_vec_t *v, uint8_t b) {
    if (v->len >= v->cap) {
        size_t ncap = (v->cap == 0u) ? 64u : (v->cap * 2u);
        uint8_t *nd = (uint8_t *)realloc(v->data, ncap);
        if (!nd) {
            return -1;
        }
        v->data = nd;
        v->cap = ncap;
    }
    v->data[v->len++] = b;
    return 0;
}

static int parse_escape(const char **pp, uint8_t *out) {
    const char *p = *pp;
    if (*p == '\0') {
        *out = '\\';
        return 0;
    }
    if (*p >= '0' && *p <= '7') {
        unsigned val = 0u;
        for (int k = 0; k < 3 && *p >= '0' && *p <= '7'; k++, p++) {
            val = val * 8u + (unsigned)(*p - '0');
        }
        *out = (uint8_t)(val & 0xFFu);
        *pp = p;
        return 0;
    }
    switch (*p) {
    case 'a':
        *out = '\a';
        break;
    case 'b':
        *out = '\b';
        break;
    case 'f':
        *out = '\f';
        break;
    case 'n':
        *out = '\n';
        break;
    case 'r':
        *out = '\r';
        break;
    case 't':
        *out = '\t';
        break;
    case 'v':
        *out = '\v';
        break;
    default:
        *out = (uint8_t)*p;
        break;
    }
    *pp = p + 1;
    return 0;
}

static int append_class(byte_vec_t *v, const char *name, size_t nlen) {
    for (int c = 0; c < 256; c++) {
        int match = 0;
        if (nlen == 5u && memcmp(name, "alnum", 5u) == 0) {
            match = isalnum(c);
        } else if (nlen == 5u && memcmp(name, "alpha", 5u) == 0) {
            match = isalpha(c);
        } else if (nlen == 5u && memcmp(name, "blank", 5u) == 0) {
            match = (c == ' ' || c == '\t');
        } else if (nlen == 5u && memcmp(name, "cntrl", 5u) == 0) {
            match = iscntrl(c);
        } else if (nlen == 5u && memcmp(name, "digit", 5u) == 0) {
            match = isdigit(c);
        } else if (nlen == 5u && memcmp(name, "graph", 5u) == 0) {
            match = isgraph(c);
        } else if (nlen == 5u && memcmp(name, "lower", 5u) == 0) {
            match = islower(c);
        } else if (nlen == 5u && memcmp(name, "print", 5u) == 0) {
            match = isprint(c);
        } else if (nlen == 5u && memcmp(name, "punct", 5u) == 0) {
            match = ispunct(c);
        } else if (nlen == 5u && memcmp(name, "space", 5u) == 0) {
            match = isspace(c);
        } else if (nlen == 5u && memcmp(name, "upper", 5u) == 0) {
            match = isupper(c);
        } else if (nlen == 6u && memcmp(name, "xdigit", 6u) == 0) {
            match = isxdigit(c);
        } else {
            return -1;
        }
        if (match && bv_push(v, (uint8_t)c) != 0) {
            return -1;
        }
    }
    return 0;
}

static int expand_set(const char *spec, byte_vec_t *v, size_t target_len) {
    memset(v, 0, sizeof(*v));
    const char *p = spec;
    while (*p != '\0') {
        if (p[0] == '[' && p[1] == ':') {
            const char *end = strstr(p + 2, ":]");
            if (end) {
                if (append_class(v, p + 2, (size_t)(end - (p + 2))) != 0) {
                    return -1;
                }
                p = end + 2;
                continue;
            }
        }
        if (p[0] == '[' && p[1] == '=' && p[2] != '\0' && p[3] == '=' &&
            p[4] == ']') {
            if (bv_push(v, (uint8_t)p[2]) != 0) {
                return -1;
            }
            p += 5;
            continue;
        }
        if (p[0] == '[' && p[1] != '\0' && p[2] == '*') {
            const char *rb = strchr(p + 3, ']');
            if (rb) {
                uint8_t ch = (uint8_t)p[1];
                size_t rep = 0u;
                if (rb == p + 3) {
                    rep = (target_len > v->len) ? (target_len - v->len) : 0u;
                } else {
                    int base = (p[3] == '0') ? 8 : 10;
                    rep = (size_t)strtoul(p + 3, NULL, base);
                }
                for (size_t k = 0u; k < rep; k++) {
                    if (bv_push(v, ch) != 0) {
                        return -1;
                    }
                }
                p = rb + 1;
                continue;
            }
        }
        uint8_t c1 = 0u;
        if (*p == '\\') {
            p++;
            (void)parse_escape(&p, &c1);
        } else {
            c1 = (uint8_t)*p++;
        }
        if (*p == '-' && p[1] != '\0') {
            p++;
            uint8_t c2 = 0u;
            if (*p == '\\') {
                p++;
                (void)parse_escape(&p, &c2);
            } else {
                c2 = (uint8_t)*p++;
            }
            if (c2 < c1) {
                return -1;
            }
            for (unsigned c = c1; c <= c2; c++) {
                if (bv_push(v, (uint8_t)c) != 0) {
                    return -1;
                }
            }
        } else if (bv_push(v, c1) != 0) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    int opt_c = 0;
    int opt_d = 0;
    int opt_s = 0;
    int opt;
    while ((opt = getopt(argc, argv, "cCds")) != -1) {
        switch (opt) {
        case 'c':
        case 'C':
            opt_c = 1;
            break;
        case 'd':
            opt_d = 1;
            break;
        case 's':
            opt_s = 1;
            break;
        default:
            return 1;
        }
    }

    int rem = argc - optind;
    if (rem < 1 || (opt_d && !opt_s && rem != 1) ||
        (!opt_d && !opt_s && rem != 2) || (opt_d && opt_s && rem != 2) ||
        (!opt_d && opt_s && (rem < 1 || rem > 2))) {
        (void)fprintf(stderr, "tr: invalid operand count\n");
        return 1;
    }

    byte_vec_t s1 = {0};
    byte_vec_t s2 = {0};
    if (expand_set(argv[optind], &s1, 0u) != 0) {
        free(s1.data);
        return 1;
    }
    if (opt_c) {
        uint8_t present[256] = {0};
        for (size_t i = 0u; i < s1.len; i++) {
            present[s1.data[i]] = 1u;
        }
        s1.len = 0u;
        for (int c = 0; c < 256; c++) {
            if (!present[c] && bv_push(&s1, (uint8_t)c) != 0) {
                free(s1.data);
                return 1;
            }
        }
    }
    if (rem == 2 && expand_set(argv[optind + 1], &s2, s1.len) != 0) {
        free(s1.data);
        free(s2.data);
        return 1;
    }

    uint8_t del_tbl[256] = {0};
    uint8_t sqz_tbl[256] = {0};
    uint8_t map_tbl[256];
    for (int c = 0; c < 256; c++) {
        map_tbl[c] = (uint8_t)c;
    }

    if (opt_d) {
        for (size_t i = 0u; i < s1.len; i++) {
            del_tbl[s1.data[i]] = 1u;
        }
        if (opt_s) {
            for (size_t i = 0u; i < s2.len; i++) {
                sqz_tbl[s2.data[i]] = 1u;
            }
        }
    } else if (rem == 2) {
        if (s2.len > 0u) {
            for (size_t i = 0u; i < s1.len; i++) {
                uint8_t dst = (i < s2.len) ? s2.data[i] : s2.data[s2.len - 1u];
                map_tbl[s1.data[i]] = dst;
            }
        }
        if (opt_s) {
            for (size_t i = 0u; i < s2.len; i++) {
                sqz_tbl[s2.data[i]] = 1u;
            }
        }
    } else if (opt_s) {
        for (size_t i = 0u; i < s1.len; i++) {
            sqz_tbl[s1.data[i]] = 1u;
        }
    }

    int prev = -1;
    int ch;
    while ((ch = getchar()) != EOF) {
        uint8_t b = (uint8_t)ch;
        if (opt_d && del_tbl[b]) {
            continue;
        }
        b = map_tbl[b];
        if (opt_s && sqz_tbl[b] && prev == (int)b) {
            continue;
        }
        (void)putchar(b);
        prev = (int)b;
    }

    free(s1.data);
    free(s2.data);
    return 0;
}
