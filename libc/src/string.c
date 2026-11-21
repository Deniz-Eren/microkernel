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

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 */

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

char *strerror(int errnum) {
    switch (errnum) {
    case 0:
        return (char *)"No error";
    case EPERM:
        return (char *)"Operation not permitted";
    case ENOENT:
        return (char *)"No such file or directory";
    case ESRCH:
        return (char *)"No such process";
    case EINTR:
        return (char *)"Interrupted system call";
    case EIO:
        return (char *)"I/O error";
    case ENXIO:
        return (char *)"No such device or address";
    case E2BIG:
        return (char *)"Argument list too long";
    case ENOEXEC:
        return (char *)"Exec format error";
    case EBADF:
        return (char *)"Bad file descriptor";
    case ECHILD:
        return (char *)"No child processes";
    case EAGAIN:
        return (char *)"Resource temporarily unavailable";
    case ENOMEM:
        return (char *)"Cannot allocate memory";
    case EACCES:
        return (char *)"Permission denied";
    case EFAULT:
        return (char *)"Bad address";
    case EBUSY:
        return (char *)"Device or resource busy";
    case EEXIST:
        return (char *)"File exists";
    case EXDEV:
        return (char *)"Invalid cross-device link";
    case ENODEV:
        return (char *)"No such device";
    case ENOTDIR:
        return (char *)"Not a directory";
    case EISDIR:
        return (char *)"Is a directory";
    case EINVAL:
        return (char *)"Invalid argument";
    case ENFILE:
        return (char *)"Too many open files in system";
    case EMFILE:
        return (char *)"Too many open files";
    case ENOTTY:
        return (char *)"Inappropriate ioctl for device";
    case ETXTBSY:
        return (char *)"Text file busy";
    case EFBIG:
        return (char *)"File too large";
    case ENOSPC:
        return (char *)"No space left on device";
    case ESPIPE:
        return (char *)"Illegal seek";
    case EROFS:
        return (char *)"Read-only file system";
    case EMLINK:
        return (char *)"Too many links";
    case EPIPE:
        return (char *)"Broken pipe";
    case EDOM:
        return (char *)"Numerical argument out of domain";
    case ERANGE:
        return (char *)"Result too large";
    case EDEADLK:
        return (char *)"Resource deadlock avoided";
    case ENAMETOOLONG:
        return (char *)"File name too long";
    case ENOLCK:
        return (char *)"No locks available";
    case ENOSYS:
        return (char *)"Function not implemented";
    case ENOTEMPTY:
        return (char *)"Directory not empty";
    case ELOOP:
        return (char *)"Too many levels of symbolic links";
    case EOVERFLOW:
        return (char *)"Value too large for defined data type";
    case EILSEQ:
        return (char *)"Illegal byte sequence";
    case ENOTSOCK:
        return (char *)"Socket operation on non-socket";
    case EOPNOTSUPP:
        return (char *)"Operation not supported";
    case ETIMEDOUT:
        return (char *)"Connection timed out";
    case ECANCELED:
        return (char *)"Operation canceled";
    default:
        errno = EINVAL;
        return (char *)"Unknown error";
    }
}
