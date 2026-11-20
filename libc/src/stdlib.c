// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/microkernel.h>

/* need:: COMP_LIBC_0001 */

void exit(int status) {
    extern void __libc_fini_array(void);
    __libc_fini_array();
    _exit(status);
}

void abort(void) {
    _exit(134);
}

/* Tiny bump allocator over a static arena (sufficient for Ring 3 servers
 * and tests; grows on demand via SYS_VM_MAP in a later milestone). */
extern char __heap_base__[]; /* provided by linker if defined; else static */

static char g_arena[1 << 20];
static size_t g_arena_pos;

void *malloc(size_t n) {
    size_t aligned = (n + 15) & ~(size_t)15;
    if (aligned < n || aligned > sizeof(g_arena) - g_arena_pos) {
        errno = ENOMEM;
        return 0;
    }
    void *p = &g_arena[g_arena_pos];
    g_arena_pos += aligned;
    return p;
}

void free(void *p) {
    (void)p;
}

void *calloc(size_t a, size_t b) {
    if (a != 0 && b > ((size_t)-1) / a) {
        errno = ENOMEM;
        return 0;
    }
    size_t total = a * b;
    void *p = malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
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
