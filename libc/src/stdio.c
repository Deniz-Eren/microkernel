/**
 * @file     stdio.c
 * @brief    Thread-safe freestanding Ring 3 stdio formatting and stream locks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0001 */

struct __microkernel_file {
    int fd;
    pthread_mutex_t lock;
};

static struct __microkernel_file g_stdin_file = {
    STDIN_FILENO, {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u}};
static struct __microkernel_file g_stdout_file = {
    STDOUT_FILENO, {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u}};
static struct __microkernel_file g_stderr_file = {
    STDERR_FILENO, {0u, PTHREAD_MUTEX_RECURSIVE, 0u, 0u}};

FILE *stdin = &g_stdin_file;
FILE *stdout = &g_stdout_file;
FILE *stderr = &g_stderr_file;

void flockfile(FILE *file) {
    if (file) {
        (void)pthread_mutex_lock(&file->lock);
    }
}

int ftrylockfile(FILE *file) {
    if (!file) {
        return -1;
    }
    return pthread_mutex_trylock(&file->lock) == 0 ? 0 : -1;
}

void funlockfile(FILE *file) {
    if (file) {
        (void)pthread_mutex_unlock(&file->lock);
    }
}

static void stdio_unlock_cleanup(void *arg) {
    funlockfile((FILE *)arg);
}

static const char g_digits[] = "0123456789abcdef";

struct sbuf {
    char *p;
    char *end;
    size_t written;
};

static void emit(struct sbuf *b, char c) {
    b->written++;
    if (b->p < b->end) {
        *b->p++ = c;
    }
}

static void emit_str(struct sbuf *b, const char *s) {
    while (*s) {
        emit(b, *s++);
    }
}

static void emit_unsigned(struct sbuf *b, uint64_t v, unsigned base,
                          int upper) {
    char tmp[24];
    int i = 0;
    if (!v) {
        tmp[i++] = '0';
    }
    while (v) {
        unsigned d = v % base;
        tmp[i++] =
            upper ? (char)(d < 10 ? '0' + d : 'A' + d - 10) : g_digits[d];
        v /= base;
    }
    while (i--) {
        emit(b, tmp[i]);
    }
}

static void emit_signed(struct sbuf *b, int64_t v, unsigned base) {
    if (v < 0) {
        emit(b, '-');
        emit_unsigned(b, (uint64_t)(-v), base, 0);
    } else {
        emit_unsigned(b, (uint64_t)v, base, 0);
    }
}

static void kvformat(struct sbuf *b, const char *fmt, va_list ap) {
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            emit(b, *f);
            continue;
        }
        f++;
        int longs = 0, is_z = 0;
        while (*f == 'l' || *f == 'z') {
            if (*f == 'z') {
                is_z = 1;
            } else {
                longs++;
            }
            f++;
        }
        switch (*f) {
        case '%':
            emit(b, '%');
            break;
        case 'c':
            emit(b, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            emit_str(b, s ? s : "(null)");
            break;
        }
        case 'd':
        case 'i': {
            int64_t v = (longs || is_z) ? va_arg(ap, long) : va_arg(ap, int);
            emit_signed(b, v, 10);
            break;
        }
        case 'u': {
            uint64_t v = (longs || is_z) ? va_arg(ap, unsigned long)
                                         : va_arg(ap, unsigned int);
            emit_unsigned(b, v, 10, 0);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = (longs || is_z) ? va_arg(ap, unsigned long)
                                         : va_arg(ap, unsigned int);
            emit_unsigned(b, v, 16, *f == 'X');
            break;
        }
        case 'p': {
            emit_str(b, "0x");
            emit_unsigned(b, (uint64_t)va_arg(ap, void *), 16, 0);
            break;
        }
        default:
            emit(b, '%');
            emit(b, *f);
            break;
        }
    }
}

int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...) {
    struct sbuf b = {buf, buf + n - 1, 0};
    va_list ap;
    va_start(ap, fmt);
    kvformat(&b, fmt, ap);
    va_end(ap);
    if (n) {
        *b.p = '\0';
    }
    return (int)b.written;
}

static int vfprintf_locked(FILE *stream, const char *fmt, va_list ap) {
    char buf[1024];
    struct sbuf b = {buf, buf + sizeof(buf) - 1, 0};
    kvformat(&b, fmt, ap);
    *b.p = '\0';
    size_t n = (size_t)(b.p - buf);

    ssize_t r = -1;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    r = write(stream->fd, buf, n);
    pthread_cleanup_pop(1);
    return r < 0 ? -1 : (int)r;
}

int fprintf(FILE *restrict stream, const char *restrict fmt, ...) {
    if (!stream) {
        return -1;
    }
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf_locked(stream, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf_locked(stdout, fmt, ap);
    va_end(ap);
    return r;
}

int fputs(const char *restrict s, FILE *restrict stream) {
    if (!s || !stream) {
        return EOF;
    }
    ssize_t r = -1;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    r = write(stream->fd, s, strlen(s));
    pthread_cleanup_pop(1);
    return r < 0 ? EOF : 0;
}

int puts(const char *s) {
    if (!s) {
        return EOF;
    }
    ssize_t r = -1;
    flockfile(stdout);
    pthread_cleanup_push(stdio_unlock_cleanup, stdout);
    r = write(stdout->fd, s, strlen(s));
    if (r >= 0) {
        r = write(stdout->fd, "\n", 1);
    }
    pthread_cleanup_pop(1);
    return r < 0 ? EOF : 0;
}

int putchar_unlocked(int c) {
    char ch = (char)c;
    ssize_t r = write(stdout->fd, &ch, 1);
    return r < 0 ? EOF : (unsigned char)c;
}

int fputc(int c, FILE *stream) {
    if (!stream) {
        return EOF;
    }
    char ch = (char)c;
    ssize_t r = -1;
    flockfile(stream);
    pthread_cleanup_push(stdio_unlock_cleanup, stream);
    r = write(stream->fd, &ch, 1);
    pthread_cleanup_pop(1);
    return r < 0 ? EOF : (unsigned char)c;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}
