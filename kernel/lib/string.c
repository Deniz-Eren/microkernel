/**
 * @file     string.c
 * @brief    Kernel freestanding memory and string utilities
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* Freestanding scalar string/memory helpers (Ring 0 never uses SIMD). */
#include <stddef.h>
#include <stdint.h>

typedef uint64_t __attribute__((may_alias, aligned(1))) k_u64_unaligned_t;

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t byte = (uint8_t)c;
    if (n >= 8u) {
        uint64_t fill = (uint64_t)byte * 0x0101010101010101ULL;
        while (n >= 8u) {
            *(k_u64_unaligned_t *)d = fill;
            d += 8u;
            n -= 8u;
        }
    }
    while (n > 0u) {
        *d++ = byte;
        n--;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n >= 8u) {
        *(k_u64_unaligned_t *)d = *(const k_u64_unaligned_t *)s;
        d += 8u;
        s += 8u;
        n -= 8u;
    }
    while (n > 0u) {
        *d++ = *s++;
        n--;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        if ((size_t)(s - d) >= 8u) {
            while (n >= 8u) {
                *(k_u64_unaligned_t *)d = *(const k_u64_unaligned_t *)s;
                d += 8u;
                s += 8u;
                n -= 8u;
            }
        }
        while (n > 0u) {
            *d++ = *s++;
            n--;
        }
    } else if (d > s) {
        d += n;
        s += n;
        if ((size_t)(d - s) >= 8u) {
            while (n >= 8u) {
                d -= 8u;
                s -= 8u;
                *(k_u64_unaligned_t *)d = *(const k_u64_unaligned_t *)s;
                n -= 8u;
            }
        }
        while (n > 0u) {
            *--d = *--s;
            n--;
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    while (n >= 8u) {
        if (*(const k_u64_unaligned_t *)x != *(const k_u64_unaligned_t *)y) {
            break;
        }
        x += 8u;
        y += 8u;
        n -= 8u;
    }
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrstr(const char *haystack, const char *needle) {
    if (!*needle) {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) {
            return (char *)haystack;
        }
    }
    return 0;
}
