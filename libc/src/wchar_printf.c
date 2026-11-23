/**
 * @file     wchar_printf.c
 * @brief    POSIX.1-2024 fwprintf, wprintf, swprintf, and v*wprintf family
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include "stdio_internal.h"

/* need:: COMP_LIBC_0013 */

struct wbuf {
    FILE *fp;
    wchar_t *p;
    wchar_t *end;
    size_t count;
    int err;
    int overflow;
};

static void wemit(struct wbuf *b, wchar_t wc) {
    if (b->err) {
        return;
    }
    b->count++;
    if (b->fp) {
        if (__stdio_fputwc_unlocked(wc, b->fp) == WEOF) {
            b->err = 1;
        }
    } else if (b->p && b->p < b->end) {
        *b->p++ = wc;
    } else {
        b->overflow = 1;
    }
}

static void wemit_repeat(struct wbuf *b, wchar_t wc, int n) {
    for (int i = 0; i < n; i++) {
        wemit(b, wc);
    }
}

static void wemit_padded(struct wbuf *b, const wchar_t *pfx, size_t plen,
                         const wchar_t *body, size_t blen, int width, int left,
                         int zero_pad) {
    int total = (int)(plen + blen);
    int pad = width > total ? (width - total) : 0;
    if (!left && !zero_pad) {
        wemit_repeat(b, L' ', pad);
    }
    for (size_t i = 0u; i < plen; i++) {
        wemit(b, pfx[i]);
    }
    if (!left && zero_pad) {
        wemit_repeat(b, L'0', pad);
    }
    for (size_t i = 0u; i < blen; i++) {
        wemit(b, body[i]);
    }
    if (left) {
        wemit_repeat(b, L' ', pad);
    }
}

static size_t wformat_uint(wchar_t *tmp, uint64_t v, unsigned base, int upper) {
    static const wchar_t k_lo[16] = L"0123456789abcdef";
    static const wchar_t k_hi[16] = L"0123456789ABCDEF";
    wchar_t rev[32];
    size_t i = 0u;
    if (v == 0ULL) {
        rev[i++] = L'0';
    }
    while (v != 0ULL) {
        unsigned d = (unsigned)(v % base);
        rev[i++] = upper ? k_hi[d] : k_lo[d];
        v /= base;
    }
    for (size_t j = 0u; j < i; j++) {
        tmp[j] = rev[i - 1u - j];
    }
    return i;
}

static void emit_narrow_str(struct wbuf *b, const char *s, int width, int prec,
                            int left) {
    if (!s) {
        s = "(null)";
    }
    /* First pass: decode UTF-8/multibyte characters up to prec. */
    wchar_t stack_buf[128];
    size_t slen = strlen(s);
    size_t cap = (prec >= 0 && (size_t)prec < slen) ? (size_t)prec : slen;
    wchar_t *wtmp = stack_buf;
    wchar_t *heap_buf = NULL;
    if (cap > sizeof(stack_buf) / sizeof(stack_buf[0])) {
        heap_buf = (wchar_t *)reallocarray(NULL, cap, sizeof(wchar_t));
        if (!heap_buf) {
            errno = ENOMEM;
            b->err = 1;
            return;
        }
        wtmp = heap_buf;
    }
    mbstate_t st = {0};
    const char *p = s;
    size_t wlen = 0u;
    while (*p != '\0' && (prec < 0 || wlen < (size_t)prec)) {
        wchar_t wc = 0;
        size_t rc = mbrtowc(&wc, p, 4u, &st);
        if (rc == (size_t)-1 || rc == (size_t)-2) {
            errno = EILSEQ;
            b->err = 1;
            free(heap_buf);
            return;
        }
        if (rc == 0u) {
            break;
        }
        wtmp[wlen++] = wc;
        p += rc;
    }
    wemit_padded(b, NULL, 0u, wtmp, wlen, width, left, 0);
    free(heap_buf);
}

static void kwformat(struct wbuf *b, const wchar_t *fmt, va_list ap) {
    for (const wchar_t *f = fmt; *f != L'\0' && !b->err; f++) {
        if (*f != L'%') {
            wemit(b, *f);
            continue;
        }
        f++;
        int left = 0;
        int zero_pad = 0;
        int plus_sign = 0;
        int space_sign = 0;
        int alt_form = 0;
        while (*f == L'-' || *f == L'0' || *f == L'+' || *f == L' ' ||
               *f == L'#') {
            if (*f == L'-') {
                left = 1;
            } else if (*f == L'0') {
                zero_pad = 1;
            } else if (*f == L'+') {
                plus_sign = 1;
            } else if (*f == L' ') {
                space_sign = 1;
            } else if (*f == L'#') {
                alt_form = 1;
            }
            f++;
        }

        int width = 0;
        if (*f == L'*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                width = -width;
            }
            f++;
        } else {
            while (*f >= L'0' && *f <= L'9') {
                width = width * 10 + (int)(*f - L'0');
                f++;
            }
        }

        int prec = -1;
        if (*f == L'.') {
            f++;
            prec = 0;
            if (*f == L'*') {
                prec = va_arg(ap, int);
                f++;
            } else {
                while (*f >= L'0' && *f <= L'9') {
                    prec = prec * 10 + (int)(*f - L'0');
                    f++;
                }
            }
        }

        int longs = 0;
        int shorts = 0;
        int is_z = 0;
        int is_j = 0;
        int is_t = 0;
        int is_cap_l = 0;
        while (*f == L'l' || *f == L'h' || *f == L'z' || *f == L'j' ||
               *f == L't' || *f == L'L') {
            if (*f == L'l') {
                longs++;
            } else if (*f == L'h') {
                shorts++;
            } else if (*f == L'z') {
                is_z = 1;
            } else if (*f == L'j') {
                is_j = 1;
            } else if (*f == L't') {
                is_t = 1;
            } else if (*f == L'L') {
                is_cap_l = 1;
            }
            f++;
        }
        if (*f == L'\0') {
            break;
        }
        if (left) {
            zero_pad = 0;
        }

        switch (*f) {
        case L'%':
            wemit(b, L'%');
            break;
        case L'c': {
            wchar_t wc;
            if (longs >= 1) {
                wc = (wchar_t)va_arg(ap, wint_t);
            } else {
                wint_t w = btowc((unsigned char)va_arg(ap, int));
                if (w == WEOF) {
                    errno = EILSEQ;
                    b->err = 1;
                    break;
                }
                wc = (wchar_t)w;
            }
            wemit_padded(b, NULL, 0u, &wc, 1u, width, left, 0);
            break;
        }
        case L's': {
            if (longs >= 1) {
                const wchar_t *ws = va_arg(ap, const wchar_t *);
                if (!ws) {
                    ws = L"(null)";
                }
                size_t len =
                    (prec >= 0) ? wcsnlen(ws, (size_t)prec) : wcslen(ws);
                wemit_padded(b, NULL, 0u, ws, len, width, left, 0);
            } else {
                const char *s = va_arg(ap, const char *);
                emit_narrow_str(b, s, width, prec, left);
            }
            break;
        }
        case L'd':
        case L'i': {
            int64_t v = 0;
            if (longs >= 2) {
                v = va_arg(ap, long long);
            } else if (longs == 1 || is_z || is_j || is_t) {
                v = va_arg(ap, long);
            } else if (shorts >= 2) {
                v = (signed char)va_arg(ap, int);
            } else if (shorts == 1) {
                v = (short)va_arg(ap, int);
            } else {
                v = va_arg(ap, int);
            }
            const wchar_t *pfx = L"";
            size_t plen = 0u;
            uint64_t mag = 0ULL;
            if (v < 0) {
                pfx = L"-";
                plen = 1u;
                mag = (uint64_t)(-(v + 1)) + 1ULL;
            } else {
                if (plus_sign) {
                    pfx = L"+";
                    plen = 1u;
                } else if (space_sign) {
                    pfx = L" ";
                    plen = 1u;
                }
                mag = (uint64_t)v;
            }
            wchar_t num[32];
            size_t nlen = (prec == 0 && mag == 0ULL)
                              ? 0u
                              : wformat_uint(num, mag, 10u, 0);
            if (prec >= 0) {
                zero_pad = 0;
            }
            wchar_t padded[64];
            size_t zpad = (prec > (int)nlen) ? (size_t)(prec - (int)nlen) : 0u;
            if (zpad + nlen > 64u) {
                zpad = 64u - nlen;
            }
            for (size_t i = 0u; i < zpad; i++) {
                padded[i] = L'0';
            }
            (void)wmemcpy(padded + zpad, num, nlen);
            wemit_padded(b, pfx, plen, padded, zpad + nlen, width, left,
                         zero_pad);
            break;
        }
        case L'u':
        case L'o':
        case L'x':
        case L'X': {
            uint64_t v = 0ULL;
            if (longs >= 2) {
                v = va_arg(ap, unsigned long long);
            } else if (longs == 1 || is_z || is_j || is_t) {
                v = va_arg(ap, unsigned long);
            } else if (shorts >= 2) {
                v = (unsigned char)va_arg(ap, unsigned int);
            } else if (shorts == 1) {
                v = (unsigned short)va_arg(ap, unsigned int);
            } else {
                v = va_arg(ap, unsigned int);
            }
            unsigned base = (*f == L'u') ? 10u : (*f == L'o' ? 8u : 16u);
            const wchar_t *pfx = L"";
            size_t plen = 0u;
            if (alt_form && v != 0ULL) {
                if (*f == L'x') {
                    pfx = L"0x";
                    plen = 2u;
                } else if (*f == L'X') {
                    pfx = L"0X";
                    plen = 2u;
                } else if (*f == L'o') {
                    pfx = L"0";
                    plen = 1u;
                }
            }
            wchar_t num[32];
            size_t nlen = (prec == 0 && v == 0ULL)
                              ? 0u
                              : wformat_uint(num, v, base, *f == L'X');
            if (prec >= 0) {
                zero_pad = 0;
            }
            wemit_padded(b, pfx, plen, num, nlen, width, left, zero_pad);
            break;
        }
        case L'p': {
            uint64_t v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            wchar_t num[32];
            size_t nlen = wformat_uint(num, v, 16u, 0);
            wemit_padded(b, L"0x", 2u, num, nlen, width, left, zero_pad);
            break;
        }
        case L'f':
        case L'F':
        case L'e':
        case L'E':
        case L'g':
        case L'G':
        case L'a':
        case L'A': {
            double val =
                is_cap_l ? (double)va_arg(ap, long double) : va_arg(ap, double);
            char nfmt[32];
            size_t pos = 0u;
            nfmt[pos++] = '%';
            if (left) {
                nfmt[pos++] = '-';
            }
            if (zero_pad) {
                nfmt[pos++] = '0';
            }
            if (plus_sign) {
                nfmt[pos++] = '+';
            }
            if (space_sign) {
                nfmt[pos++] = ' ';
            }
            if (alt_form) {
                nfmt[pos++] = '#';
            }
            nfmt[pos++] = '*';
            if (prec >= 0) {
                nfmt[pos++] = '.';
                nfmt[pos++] = '*';
            }
            nfmt[pos++] = (char)*f;
            nfmt[pos] = '\0';

            char ntmp[160];
            int nlen =
                (prec >= 0)
                    ? snprintf(ntmp, sizeof(ntmp), nfmt, width, prec, val)
                    : snprintf(ntmp, sizeof(ntmp), nfmt, width, val);
            if (nlen > 0) {
                size_t take = (size_t)nlen < sizeof(ntmp) ? (size_t)nlen
                                                          : sizeof(ntmp) - 1u;
                for (size_t i = 0u; i < take; i++) {
                    wemit(b, (wchar_t)(unsigned char)ntmp[i]);
                }
            }
            break;
        }
        case L'n': {
            if (longs >= 2) {
                *va_arg(ap, long long *) = (long long)b->count;
            } else if (longs == 1) {
                *va_arg(ap, long *) = (long)b->count;
            } else if (shorts >= 2) {
                *va_arg(ap, signed char *) = (signed char)b->count;
            } else if (shorts == 1) {
                *va_arg(ap, short *) = (short)b->count;
            } else if (is_j) {
                *va_arg(ap, intmax_t *) = (intmax_t)b->count;
            } else if (is_z) {
                *va_arg(ap, ssize_t *) = (ssize_t)b->count;
            } else if (is_t) {
                *va_arg(ap, ptrdiff_t *) = (ptrdiff_t)b->count;
            } else {
                *va_arg(ap, int *) = (int)b->count;
            }
            break;
        }
        default:
            wemit(b, L'%');
            wemit(b, *f);
            break;
        }
    }
}

int vfwprintf(FILE *restrict stream, const wchar_t *restrict format,
              va_list arg) {
    if (!stream || !format) {
        errno = EINVAL;
        return -1;
    }
    struct wbuf b = {
        .fp = stream,
        .p = NULL,
        .end = NULL,
        .count = 0u,
        .err = 0,
        .overflow = 0,
    };
    flockfile(stream);
    pthread_cleanup_push(__stdio_unlock_cleanup, stream);
    kwformat(&b, format, arg);
    pthread_cleanup_pop(1);
    return b.err ? -1 : (int)b.count;
}

int vwprintf(const wchar_t *restrict format, va_list arg) {
    return vfwprintf(stdout, format, arg);
}

int vswprintf(wchar_t *restrict ws, size_t n, const wchar_t *restrict format,
              va_list arg) {
    if (!format || (n > 0u && !ws)) {
        errno = EINVAL;
        return -1;
    }
    if (n == 0u) {
        errno = EOVERFLOW;
        return -1;
    }
    struct wbuf b = {
        .fp = NULL,
        .p = ws,
        .end = ws + (n - 1u),
        .count = 0u,
        .err = 0,
        .overflow = 0,
    };
    kwformat(&b, format, arg);
    *b.p = L'\0';
    if (b.err) {
        return -1;
    }
    if (b.overflow) {
        errno = EOVERFLOW;
        return -1;
    }
    return (int)b.count;
}

int fwprintf(FILE *restrict stream, const wchar_t *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfwprintf(stream, format, ap);
    va_end(ap);
    return r;
}

int wprintf(const wchar_t *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfwprintf(stdout, format, ap);
    va_end(ap);
    return r;
}

int swprintf(wchar_t *restrict ws, size_t n, const wchar_t *restrict format,
             ...) {
    va_list ap;
    va_start(ap, format);
    int r = vswprintf(ws, n, format, ap);
    va_end(ap);
    return r;
}
