/**
 * @file     stdlib.c
 * @brief    Freestanding Ring 3 exit, abort, getenv, and integer conversion
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/bootinfo.h>
#include <unistd.h>

/* need:: COMP_LIBC_0001 */

#define AT_SECURE_KEY 23UL

void exit(int status) {
    extern void __libc_fini_array(void);
    __libc_fini_array();
    _exit(status);
}

void _Exit(int status) {
    _exit(status);
}

void abort(void) {
    _exit(134);
}

char *getenv(const char *name) {
    if (!name || !*name || strchr(name, '=') || !environ) {
        return 0;
    }
    size_t nlen = strlen(name);
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, nlen) == 0 && (*e)[nlen] == '=') {
            return *e + nlen + 1;
        }
    }
    return 0;
}

char *secure_getenv(const char *name) {
    const unsigned long *auxv = (const unsigned long *)__auxv;
    if (auxv) {
        for (size_t i = 0; auxv[i] != 0UL; i += 2) {
            if (auxv[i] == AT_SECURE_KEY && auxv[i + 1] != 0UL) {
                return 0;
            }
        }
    }
    return getenv(name);
}

long strtol(const char *s, char **end, int base) {
    long v = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') {
            d = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            d = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            d = *s - 'A' + 10;
        } else {
            break;
        }
        if (d >= base) {
            break;
        }
        v = v * base + d;
        s++;
    }
    if (end) {
        *end = (char *)s;
    }
    return neg ? -v : v;
}

int atoi(const char *s) {
    return (int)strtol(s, 0, 10);
}
