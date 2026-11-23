/**
 * @file     string.c
 * @brief    POSIX.1-2024 memory, string, tokenization, and strerror functions
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

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 COMP_LIBC_0009 COMP_LIBC_0012 */

typedef uint64_t __attribute__((may_alias, aligned(1))) libc_u64_unaligned_t;

#if defined(__AVX2__)
typedef uint8_t libc_v32u8_t __attribute__((vector_size(32), aligned(1)));
#endif

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t b = (uint8_t)c;
#if defined(__AVX2__)
    if (n >= 32u) {
        libc_v32u8_t v = {b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b,
                          b, b, b, b, b, b, b, b, b, b, b, b, b, b, b, b};
        while (n >= 32u) {
            *(libc_v32u8_t *)d = v;
            d += 32u;
            n -= 32u;
        }
    }
#endif
    if (n >= 8u) {
        uint64_t fill = (uint64_t)b * 0x0101010101010101ULL;
        while (n >= 8u) {
            *(libc_u64_unaligned_t *)d = fill;
            d += 8u;
            n -= 8u;
        }
    }
    while (n > 0u) {
        *d++ = b;
        n--;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
#if defined(__AVX2__)
    while (n >= 32u) {
        *(libc_v32u8_t *)d = *(const libc_v32u8_t *)s;
        d += 32u;
        s += 32u;
        n -= 32u;
    }
#endif
    while (n >= 8u) {
        *(libc_u64_unaligned_t *)d = *(const libc_u64_unaligned_t *)s;
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
        size_t gap = (size_t)(s - d);
#if defined(__AVX2__)
        if (gap >= 32u) {
            while (n >= 32u) {
                *(libc_v32u8_t *)d = *(const libc_v32u8_t *)s;
                d += 32u;
                s += 32u;
                n -= 32u;
            }
        }
#endif
        if (gap >= 8u) {
            while (n >= 8u) {
                *(libc_u64_unaligned_t *)d = *(const libc_u64_unaligned_t *)s;
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
        size_t gap = (size_t)(d - s);
        d += n;
        s += n;
#if defined(__AVX2__)
        if (gap >= 32u) {
            while (n >= 32u) {
                d -= 32u;
                s -= 32u;
                *(libc_v32u8_t *)d = *(const libc_v32u8_t *)s;
                n -= 32u;
            }
        }
#endif
        if (gap >= 8u) {
            while (n >= 8u) {
                d -= 8u;
                s -= 8u;
                *(libc_u64_unaligned_t *)d = *(const libc_u64_unaligned_t *)s;
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
        if (*(const libc_u64_unaligned_t *)x !=
            *(const libc_u64_unaligned_t *)y) {
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

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    unsigned char target = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == target) {
            return (void *)(p + i);
        }
    }
    return 0;
}

void *memmem(const void *haystack, size_t haystacklen, const void *needle,
             size_t needlelen) {
    if (needlelen == 0u) {
        return (void *)haystack;
    }
    if (needlelen > haystacklen) {
        return 0;
    }
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    size_t limit = haystacklen - needlelen;
    for (size_t i = 0; i <= limit; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0) {
            return (void *)(h + i);
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

size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n]) {
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

char *strcpy(char *restrict dst, const char *restrict src) {
    char *d = dst;
    while ((*d++ = *src++)) {
    }
    return dst;
}

char *strncpy(char *restrict dst, const char *restrict src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = 0;
    }
    return dst;
}

char *stpcpy(char *restrict dst, const char *restrict src) {
    while ((*dst = *src) != '\0') {
        dst++;
        src++;
    }
    return dst;
}

char *stpncpy(char *restrict dst, const char *restrict src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dst[i] = src[i];
    }
    char *end = dst + i;
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return end;
}

char *strcat(char *restrict s1, const char *restrict s2) {
    char *d = s1 + strlen(s1);
    while ((*d++ = *s2++)) {
    }
    return s1;
}

char *strncat(char *restrict s1, const char *restrict s2, size_t n) {
    char *d = s1 + strlen(s1);
    size_t i = 0;
    for (; i < n && s2[i]; i++) {
        d[i] = s2[i];
    }
    d[i] = '\0';
    return s1;
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

char *strrchr(const char *s, int c) {
    const char *last = 0;
    for (; *s; s++) {
        if (*s == (char)c) {
            last = s;
        }
    }
    if ((char)c == 0) {
        return (char *)s;
    }
    return (char *)last;
}

char *strstr(const char *s1, const char *s2) {
    if (!*s2) {
        return (char *)s1;
    }
    size_t nlen = strlen(s2);
    for (const char *p = s1; *p; p++) {
        if (*p == *s2 && strncmp(p, s2, nlen) == 0) {
            return (char *)p;
        }
    }
    return 0;
}

size_t strspn(const char *s1, const char *s2) {
    size_t n = 0;
    while (s1[n] && strchr(s2, (unsigned char)s1[n]) != 0) {
        n++;
    }
    return n;
}

size_t strcspn(const char *s1, const char *s2) {
    size_t n = 0;
    while (s1[n] && strchr(s2, (unsigned char)s1[n]) == 0) {
        n++;
    }
    return n;
}

char *strpbrk(const char *s1, const char *s2) {
    for (; *s1; s1++) {
        if (strchr(s2, (unsigned char)*s1) != 0) {
            return (char *)s1;
        }
    }
    return 0;
}

char *strtok_r(char *restrict s, const char *restrict sep,
               char **restrict state) {
    char *p = s ? s : *state;
    if (!p) {
        return 0;
    }
    p += strspn(p, sep);
    if (!*p) {
        *state = 0;
        return 0;
    }
    char *end = p + strcspn(p, sep);
    if (*end) {
        *end = '\0';
        *state = end + 1;
    } else {
        *state = 0;
    }
    return p;
}

char *strtok(char *restrict s, const char *restrict sep) {
    static char *g_strtok_state = 0;
    return strtok_r(s, sep, &g_strtok_state);
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1u);
    if (!copy) {
        errno = ENOMEM;
        return 0;
    }
    memcpy(copy, s, len + 1u);
    return copy;
}

char *strndup(const char *s, size_t size) {
    size_t len = strnlen(s, size);
    char *copy = (char *)malloc(len + 1u);
    if (!copy) {
        errno = ENOMEM;
        return 0;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

static const char *strerror_lookup(int errnum, int *valid) {
    *valid = 1;
    switch (errnum) {
    case 0:
        return "No error";
    case EPERM:
        return "Operation not permitted";
    case ENOENT:
        return "No such file or directory";
    case ESRCH:
        return "No such process";
    case EINTR:
        return "Interrupted system call";
    case EIO:
        return "I/O error";
    case ENXIO:
        return "No such device or address";
    case E2BIG:
        return "Argument list too long";
    case ENOEXEC:
        return "Exec format error";
    case EBADF:
        return "Bad file descriptor";
    case ECHILD:
        return "No child processes";
    case EAGAIN:
        return "Resource temporarily unavailable";
    case ENOMEM:
        return "Cannot allocate memory";
    case EACCES:
        return "Permission denied";
    case EFAULT:
        return "Bad address";
    case EBUSY:
        return "Device or resource busy";
    case EEXIST:
        return "File exists";
    case EXDEV:
        return "Invalid cross-device link";
    case ENODEV:
        return "No such device";
    case ENOTDIR:
        return "Not a directory";
    case EISDIR:
        return "Is a directory";
    case EINVAL:
        return "Invalid argument";
    case ENFILE:
        return "Too many open files in system";
    case EMFILE:
        return "Too many open files";
    case ENOTTY:
        return "Inappropriate ioctl for device";
    case ETXTBSY:
        return "Text file busy";
    case EFBIG:
        return "File too large";
    case ENOSPC:
        return "No space left on device";
    case ESPIPE:
        return "Illegal seek";
    case EROFS:
        return "Read-only file system";
    case EMLINK:
        return "Too many links";
    case EPIPE:
        return "Broken pipe";
    case EDOM:
        return "Numerical argument out of domain";
    case ERANGE:
        return "Result too large";
    case EDEADLK:
        return "Resource deadlock avoided";
    case ENAMETOOLONG:
        return "File name too long";
    case ENOLCK:
        return "No locks available";
    case ENOSYS:
        return "Function not implemented";
    case ENOTEMPTY:
        return "Directory not empty";
    case ELOOP:
        return "Too many levels of symbolic links";
    case EOVERFLOW:
        return "Value too large for defined data type";
    case EILSEQ:
        return "Illegal byte sequence";
    case ENOTSOCK:
        return "Socket operation on non-socket";
    case EOPNOTSUPP:
        return "Operation not supported";
    case ETIMEDOUT:
        return "Connection timed out";
    case ECANCELED:
        return "Operation canceled";
    default:
        *valid = 0;
        return "Unknown error";
    }
}

char *strerror(int errnum) {
    int valid = 1;
    const char *msg = strerror_lookup(errnum, &valid);
    if (!valid) {
        errno = EINVAL;
    }
    return (char *)msg;
}

char *strerror_l(int errnum, locale_t locale) {
    (void)locale;
    return strerror(errnum);
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int strcoll_l(const char *s1, const char *s2, locale_t locale) {
    (void)locale;
    return strcmp(s1, s2);
}

size_t strxfrm(char *restrict s1, const char *restrict s2, size_t n) {
    size_t len = strlen(s2);
    if (n > 0u) {
        size_t copy = len < (n - 1u) ? len : (n - 1u);
        if (copy > 0u) {
            memcpy(s1, s2, copy);
        }
        s1[copy] = '\0';
    }
    return len;
}

size_t strxfrm_l(char *restrict s1, const char *restrict s2, size_t n,
                 locale_t locale) {
    (void)locale;
    return strxfrm(s1, s2, n);
}

int strerror_r(int errnum, char *strerrbuf, size_t buflen) {
    int valid = 1;
    const char *msg = strerror_lookup(errnum, &valid);
    size_t len = strlen(msg);
    if (!strerrbuf || buflen == 0u) {
        return valid ? ERANGE : EINVAL;
    }
    size_t copy = len < (buflen - 1u) ? len : (buflen - 1u);
    if (copy > 0u) {
        memcpy(strerrbuf, msg, copy);
    }
    strerrbuf[copy] = '\0';
    if (!valid) {
        return EINVAL;
    }
    if (len >= buflen) {
        return ERANGE;
    }
    return 0;
}
