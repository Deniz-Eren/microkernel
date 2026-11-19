#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static void emit_unsigned(struct sbuf *b, uint64_t v, unsigned base, int upper) {
    char tmp[24];
    int i = 0;
    if (!v) {
        tmp[i++] = '0';
    }
    while (v) {
        unsigned d = v % base;
        tmp[i++] = upper ? (char)(d < 10 ? '0' + d : 'A' + d - 10) : g_digits[d];
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
        case '%': emit(b, '%'); break;
        case 'c': emit(b, (char)va_arg(ap, int)); break;
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
            uint64_t v = (longs || is_z) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            emit_unsigned(b, v, 10, 0);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t v = (longs || is_z) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
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

int snprintf(char *buf, size_t n, const char *fmt, ...) {
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

int printf(const char *fmt, ...) {
    /* Per-call stack buffer: multiple threads may print concurrently. */
    char buf[1024];
    struct sbuf b = {buf, buf + sizeof(buf) - 1, 0};
    va_list ap;
    va_start(ap, fmt);
    kvformat(&b, fmt, ap);
    va_end(ap);
    *b.p = '\0';
    size_t n = (size_t)(b.p - buf);
    ssize_t r = write(1, buf, n);
    return r < 0 ? -1 : (int)r;
}

int puts(const char *s) {
    ssize_t r = write(1, s, strlen(s));
    if (r >= 0) {
        r = write(1, "\n", 1);
    }
    return r < 0 ? -1 : 0;
}

int putchar(int c) {
    char ch = (char)c;
    ssize_t r = write(1, &ch, 1);
    return r < 0 ? -1 : (unsigned char)c;
}
