/**
 * @file     stdlib.c
 * @brief    Freestanding Ring 3 exit, heap, reallocarray, getenv, and strtol
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/bootinfo.h>
#include <sys/microkernel.h>
#include <unistd.h>

/* need:: COMP_LIBC_0001 */

#define AT_SECURE_KEY 23UL
#define ALLOC_MAGIC 0x4D4B414C4C4F4355ULL

struct alloc_hdr {
    size_t size;
    uint64_t magic;
};

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

static char g_arena[1 << 20] __attribute__((aligned(16)));
static size_t g_arena_pos;
static unsigned int g_heap_lock;

static void heap_lock(void) {
    while (__atomic_exchange_n(&g_heap_lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
        __asm__ volatile("pause" : : : "memory");
    }
}

static void heap_unlock(void) {
    __atomic_store_n(&g_heap_lock, 0u, __ATOMIC_RELEASE);
}

void *malloc(size_t n) {
    size_t aligned = (n + 15u) & ~(size_t)15u;
    if (aligned < n) {
        errno = ENOMEM;
        return 0;
    }
    size_t total = aligned + sizeof(struct alloc_hdr);
    if (total < aligned) {
        errno = ENOMEM;
        return 0;
    }
    heap_lock();
    if (total > sizeof(g_arena) - g_arena_pos) {
        heap_unlock();
        errno = ENOMEM;
        return 0;
    }
    struct alloc_hdr *hdr = (struct alloc_hdr *)(void *)&g_arena[g_arena_pos];
    g_arena_pos += total;
    heap_unlock();
    hdr->size = n;
    hdr->magic = ALLOC_MAGIC;
    return (void *)(hdr + 1);
}

void free(void *p) {
    (void)p;
}

void *calloc(size_t a, size_t b) {
    if (a != 0u && b > ((size_t)-1) / a) {
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

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0u) {
        free(ptr);
        return 0;
    }
    struct alloc_hdr *hdr = ((struct alloc_hdr *)ptr) - 1;
    size_t old_size = (hdr->magic == ALLOC_MAGIC) ? hdr->size : 0u;
    if (old_size >= size) {
        hdr->size = size;
        return ptr;
    }
    void *np = malloc(size);
    if (!np) {
        return 0;
    }
    if (old_size > 0u) {
        memcpy(np, ptr, old_size);
    }
    return np;
}

void *reallocarray(void *ptr, size_t nelem, size_t elsize) {
    if (nelem != 0u && elsize > ((size_t)-1) / nelem) {
        errno = ENOMEM;
        return 0;
    }
    return realloc(ptr, nelem * elsize);
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
