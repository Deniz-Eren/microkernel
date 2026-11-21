/**
 * @file     string.c
 * @brief    Freestanding Ring 3 memory and string helper functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <string.h>

/* need:: COMP_LIBC_0001 */

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = dst;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = a, *y = b;
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

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = 0;
    }
    return dst;
}

size_t strlcpy(char *restrict dst, const char *restrict src, size_t dstsize) {
    size_t slen = strlen(src);
    if (dstsize > 0u) {
        size_t copy = (slen < dstsize - 1u) ? slen : (dstsize - 1u);
        if (copy > 0u) {
            memcpy(dst, src, copy);
        }
        dst[copy] = '\0';
    }
    return slen;
}

size_t strlcat(char *restrict dst, const char *restrict src, size_t dstsize) {
    size_t dlen = 0u;
    while (dlen < dstsize && dst[dlen] != '\0') {
        dlen++;
    }
    size_t slen = strlen(src);
    if (dlen == dstsize) {
        return dstsize + slen;
    }
    size_t avail = dstsize - dlen - 1u;
    size_t copy = (slen < avail) ? slen : avail;
    if (copy > 0u) {
        memcpy(dst + dlen, src, copy);
    }
    dst[dlen + copy] = '\0';
    return dlen + slen;
}

char *strchr(const char *s, int c) {
    for (; *s; s++) {
        if (*s == (char)c) {
            return (char *)s;
        }
    }
    return (char)c == 0 ? (char *)s : 0;
}
