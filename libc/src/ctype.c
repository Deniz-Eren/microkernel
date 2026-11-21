/**
 * @file     ctype.c
 * @brief    POSIX.1-2024 ctype, strings, assert, and inttypes implementations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

/* need:: COMP_LIBC_0004 */

int isupper(int c) {
    return (c >= 'A' && c <= 'Z') ? 1 : 0;
}

int islower(int c) {
    return (c >= 'a' && c <= 'z') ? 1 : 0;
}

int isalpha(int c) {
    return (isupper(c) || islower(c)) ? 1 : 0;
}

int isdigit(int c) {
    return (c >= '0' && c <= '9') ? 1 : 0;
}

int isalnum(int c) {
    return (isalpha(c) || isdigit(c)) ? 1 : 0;
}

int isxdigit(int c) {
    return (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) ? 1
                                                                            : 0;
}

int isblank(int c) {
    return (c == ' ' || c == '\t') ? 1 : 0;
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
            c == '\f')
               ? 1
               : 0;
}

int iscntrl(int c) {
    return ((c >= 0 && c <= 0x1F) || c == 0x7F) ? 1 : 0;
}

int isprint(int c) {
    return (c >= 0x20 && c <= 0x7E) ? 1 : 0;
}

int isgraph(int c) {
    return (c >= 0x21 && c <= 0x7E) ? 1 : 0;
}

int ispunct(int c) {
    return (isgraph(c) && !isalnum(c)) ? 1 : 0;
}

int tolower(int c) {
    return isupper(c) ? (c + ('a' - 'A')) : c;
}

int toupper(int c) {
    return islower(c) ? (c - ('a' - 'A')) : c;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int c1 = tolower((unsigned char)s1[i]);
        int c2 = tolower((unsigned char)s2[i]);
        if (c1 != c2 || c1 == 0) {
            return c1 - c2;
        }
    }
    return 0;
}

_Noreturn void __assert_fail(const char *expr, const char *file,
                             unsigned int line, const char *func) {
    fprintf(stderr, "assertion failed: %s (%s:%u in %s)\n", expr ? expr : "?",
            file ? file : "?", line, func ? func : "?");
    (void)fflush(stderr);
    abort();
}

intmax_t imaxabs(intmax_t j) {
    return j < 0 ? -j : j;
}

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
    imaxdiv_t r;
    r.quot = numer / denom;
    r.rem = numer % denom;
    return r;
}

intmax_t strtoimax(const char *restrict nptr, char **restrict endptr,
                   int base) {
    return (intmax_t)strtoll(nptr, endptr, base);
}

uintmax_t strtoumax(const char *restrict nptr, char **restrict endptr,
                    int base) {
    return (uintmax_t)strtoull(nptr, endptr, base);
}
