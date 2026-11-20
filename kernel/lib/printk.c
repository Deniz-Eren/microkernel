// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/printk.h>
#include "../arch/x86_64/serial.h"

static int g_quiet;

void serial_set_quiet(int quiet) {
    g_quiet = quiet;
}

static const char g_digits_lc[] = "0123456789abcdef";
static const char g_digits_uc[] = "0123456789ABCDEF";

static void emit(char **p, const char *end, char c) {
    if (*p < end) {
        **p = c;
        (*p)++;
    }
}

static void format_unsigned(char **p, const char *end, uint64_t v, unsigned base,
                            int upper) {
    char tmp[24];
    const char *digits = upper ? g_digits_uc : g_digits_lc;
    int i = 0;
    if (v == 0) {
        tmp[i++] = '0';
    }
    while (v) {
        tmp[i++] = digits[v % base];
        v /= base;
    }
    while (i--) {
        emit(p, end, tmp[i]);
    }
}

static void format_signed(char **p, const char *end, int64_t v, unsigned base) {
    if (v < 0) {
        emit(p, end, '-');
        format_unsigned(p, end, (uint64_t)(-v), base, 0);
    } else {
        format_unsigned(p, end, (uint64_t)v, base, 0);
    }
}

/* Minimal freestanding vsnprintf: %s %c %d %i %u %x %X %p %%, lengths l/ll/z. */
static int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    char *p = buf;
    const char *end = buf + n - 1;

    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            emit(&p, end, *f);
            continue;
        }
        f++;
        int longs = 0;
        int is_z = 0;
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
            emit(&p, end, '%');
            break;
        case 'c':
            emit(&p, end, (char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) {
                s = "(null)";
            }
            while (*s) {
                emit(&p, end, *s++);
            }
            break;
        }
        case 'd':
        case 'i': {
            int64_t v;
            if (longs > 0 || is_z) {
                v = (int64_t)va_arg(ap, long);
            } else {
                v = va_arg(ap, int);
            }
            format_signed(&p, end, v, 10);
            break;
        }
        case 'u': {
            uint64_t v;
            if (longs > 0 || is_z) {
                v = va_arg(ap, unsigned long);
            } else {
                v = va_arg(ap, unsigned int);
            }
            format_unsigned(&p, end, v, 10, 0);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v;
            if (longs > 0 || is_z) {
                v = va_arg(ap, unsigned long);
            } else {
                v = va_arg(ap, unsigned int);
            }
            format_unsigned(&p, end, v, 16, *f == 'X');
            break;
        }
        case 'p': {
            uint64_t v = (uint64_t)va_arg(ap, void *);
            emit(&p, end, '0');
            emit(&p, end, 'x');
            format_unsigned(&p, end, v, 16, 0);
            break;
        }
        default:
            emit(&p, end, '%');
            emit(&p, end, *f);
            break;
        }
    }
    *p = '\0';
    return (int)(p - buf);
}

void printk_v(const char *fmt, va_list ap) {
    static char buf[2048];
    kvsnprintf(buf, sizeof(buf), fmt, ap);
    if (!g_quiet) {
        const char *s = buf;
        while (*s) {
            serial_putc(*s++);
        }
    }
}

void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printk_v(fmt, ap);
    va_end(ap);
}
