/**
 * @file     wchar_conv.c
 * @brief    POSIX.1-2024 wide numeric parsing (wcsto*) and wcsftime
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

/* need:: COMP_LIBC_0012 */

static int wdigit_val(wchar_t wc) {
    if (wc >= L'0' && wc <= L'9') {
        return (int)(wc - L'0');
    }
    if (wc >= L'a' && wc <= L'z') {
        return (int)(wc - L'a') + 10;
    }
    if (wc >= L'A' && wc <= L'Z') {
        return (int)(wc - L'A') + 10;
    }
    return -1;
}

static unsigned long long parse_wuint(const wchar_t *nptr, wchar_t **endptr,
                                      int base, int *neg_out, int *ovf_out,
                                      int *any_out) {
    const wchar_t *p = nptr;
    *neg_out = 0;
    *ovf_out = 0;
    *any_out = 0;

    if (base != 0 && (base < 2 || base > 36)) {
        if (endptr) {
            *endptr = (wchar_t *)nptr;
        }
        errno = EINVAL;
        return 0ULL;
    }
    while (iswspace((wint_t)*p)) {
        p++;
    }
    if (*p == L'+') {
        p++;
    } else if (*p == L'-') {
        *neg_out = 1;
        p++;
    }
    if (base == 0) {
        if (*p == L'0') {
            if ((p[1] == L'x' || p[1] == L'X') && wdigit_val(p[2]) >= 0 &&
                wdigit_val(p[2]) < 16) {
                base = 16;
                p += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && *p == L'0' && (p[1] == L'x' || p[1] == L'X') &&
               wdigit_val(p[2]) >= 0 && wdigit_val(p[2]) < 16) {
        p += 2;
    }

    unsigned long long acc = 0ULL;
    unsigned long long ubase = (unsigned long long)base;
    unsigned long long cutoff = ULLONG_MAX / ubase;
    unsigned long long cutlim = ULLONG_MAX % ubase;

    for (;;) {
        int d = wdigit_val(*p);
        if (d < 0 || d >= base) {
            break;
        }
        *any_out = 1;
        if (*ovf_out || acc > cutoff ||
            (acc == cutoff && (unsigned long long)d > cutlim)) {
            *ovf_out = 1;
        } else {
            acc = acc * ubase + (unsigned long long)d;
        }
        p++;
    }
    if (endptr) {
        *endptr = (wchar_t *)(*any_out ? p : nptr);
    }
    return acc;
}

long long wcstoll(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                  int base) {
    int neg = 0;
    int ovf = 0;
    int any = 0;
    unsigned long long v = parse_wuint(nptr, endptr, base, &neg, &ovf, &any);
    if (!any) {
        return 0LL;
    }
    unsigned long long limit = neg ? ((unsigned long long)LLONG_MAX + 1ULL)
                                   : (unsigned long long)LLONG_MAX;
    if (ovf || v > limit) {
        errno = ERANGE;
        return neg ? LLONG_MIN : LLONG_MAX;
    }
    if (neg) {
        return (v == (unsigned long long)LLONG_MAX + 1ULL) ? LLONG_MIN
                                                           : -(long long)v;
    }
    return (long long)v;
}

long wcstol(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base) {
    long long v = wcstoll(nptr, endptr, base);
    if (v > LONG_MAX) {
        errno = ERANGE;
        return LONG_MAX;
    }
    if (v < LONG_MIN) {
        errno = ERANGE;
        return LONG_MIN;
    }
    return (long)v;
}

unsigned long long wcstoull(const wchar_t *restrict nptr,
                            wchar_t **restrict endptr, int base) {
    int neg = 0;
    int ovf = 0;
    int any = 0;
    unsigned long long v = parse_wuint(nptr, endptr, base, &neg, &ovf, &any);
    if (!any) {
        return 0ULL;
    }
    if (ovf) {
        errno = ERANGE;
        return ULLONG_MAX;
    }
    return neg ? (0ULL - v) : v;
}

unsigned long wcstoul(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                      int base) {
    unsigned long long v = wcstoull(nptr, endptr, base);
    if (v > ULONG_MAX) {
        errno = ERANGE;
        return ULONG_MAX;
    }
    return (unsigned long)v;
}

intmax_t wcstoimax(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                   int base) {
    return (intmax_t)wcstoll(nptr, endptr, base);
}

uintmax_t wcstoumax(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                    int base) {
    return (uintmax_t)wcstoull(nptr, endptr, base);
}

/* Copy leading ASCII floating-point prefix so strto{d,f,ld} handles exact
 * IEEE-754 rounding without duplicating arbitrary-precision math. */
static size_t widen_float_prefix(const wchar_t *nptr, char *buf, size_t cap,
                                 const wchar_t **start_out) {
    const wchar_t *p = nptr;
    while (iswspace((wint_t)*p)) {
        p++;
    }
    *start_out = p;
    size_t n = 0u;
    while (n + 1u < cap && p[n] >= 0x20 && p[n] <= 0x7E) {
        buf[n] = (char)p[n];
        n++;
    }
    buf[n] = '\0';
    return n;
}

double wcstod(const wchar_t *restrict nptr, wchar_t **restrict endptr) {
    char buf[512];
    const wchar_t *start = nptr;
    (void)widen_float_prefix(nptr, buf, sizeof(buf), &start);
    char *ep = buf;
    double res = strtod(buf, &ep);
    if (endptr) {
        *endptr = (wchar_t *)((ep == buf) ? nptr : (start + (ep - buf)));
    }
    return res;
}

float wcstof(const wchar_t *restrict nptr, wchar_t **restrict endptr) {
    char buf[512];
    const wchar_t *start = nptr;
    (void)widen_float_prefix(nptr, buf, sizeof(buf), &start);
    char *ep = buf;
    float res = strtof(buf, &ep);
    if (endptr) {
        *endptr = (wchar_t *)((ep == buf) ? nptr : (start + (ep - buf)));
    }
    return res;
}

long double wcstold(const wchar_t *restrict nptr, wchar_t **restrict endptr) {
    char buf[512];
    const wchar_t *start = nptr;
    (void)widen_float_prefix(nptr, buf, sizeof(buf), &start);
    char *ep = buf;
    long double res = strtold(buf, &ep);
    if (endptr) {
        *endptr = (wchar_t *)((ep == buf) ? nptr : (start + (ep - buf)));
    }
    return res;
}

size_t wcsftime(wchar_t *restrict wcs, size_t maxsize,
                const wchar_t *restrict format,
                const struct tm *restrict timeptr) {
    if (!wcs || maxsize == 0u || !format || !timeptr) {
        return 0u;
    }
    size_t pos = 0u;
    for (const wchar_t *f = format; *f != L'\0'; f++) {
        if (*f != L'%') {
            if (pos + 1u >= maxsize) {
                return 0u;
            }
            wcs[pos++] = *f;
            continue;
        }
        char spec[8];
        size_t slen = 0u;
        spec[slen++] = '%';
        f++;
        if (*f == L'E' || *f == L'O') {
            spec[slen++] = (char)*f++;
        }
        if (*f == L'\0') {
            break;
        }
        if (*f < 0x20 || *f > 0x7E) {
            if (pos + 2u >= maxsize) {
                return 0u;
            }
            wcs[pos++] = L'%';
            wcs[pos++] = *f;
            continue;
        }
        spec[slen++] = (char)*f;
        spec[slen] = '\0';

        char tmp[128];
        size_t n = strftime(tmp, sizeof(tmp), spec, timeptr);
        for (size_t i = 0u; i < n; i++) {
            if (pos + 1u >= maxsize) {
                return 0u;
            }
            wcs[pos++] = (wchar_t)(unsigned char)tmp[i];
        }
    }
    wcs[pos] = L'\0';
    return pos;
}
