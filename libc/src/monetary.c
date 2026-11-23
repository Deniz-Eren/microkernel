/**
 * @file     monetary.c
 * @brief    POSIX.1-2024 strfmon and strfmon_l monetary value formatting
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <monetary.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

static int out_put(char *s, size_t maxsize, size_t *pos, char c) {
    if (*pos + 1u >= maxsize) {
        errno = E2BIG;
        return -1;
    }
    s[(*pos)++] = c;
    return 0;
}

static int out_puts(char *s, size_t maxsize, size_t *pos, const char *str) {
    for (size_t i = 0u; str[i] != '\0'; i++) {
        if (out_put(s, maxsize, pos, str[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int parse_uint(const char **pp) {
    int v = 0;
    while (isdigit((unsigned char)**pp)) {
        if (v < 10000) {
            v = v * 10 + (**pp - '0');
        }
        (*pp)++;
    }
    return v;
}

static ssize_t vstrfmon_impl(char *restrict s, size_t maxsize, locale_t locale,
                             const char *restrict format, va_list ap) {
    (void)locale;
    if (!s || maxsize == 0u || !format) {
        errno = EINVAL;
        return -1;
    }
    const struct lconv *lc = localeconv();
    size_t pos = 0u;

    for (const char *p = format; *p != '\0'; p++) {
        if (*p != '%') {
            if (out_put(s, maxsize, &pos, *p) != 0) {
                return -1;
            }
            continue;
        }
        p++;
        if (*p == '%') {
            if (out_put(s, maxsize, &pos, '%') != 0) {
                return -1;
            }
            continue;
        }

        char fill = ' ';
        int no_group = 0;
        int paren_neg = 0;
        int suppress_curr = 0;
        int left_align = 0;

        for (;;) {
            if (*p == '=' && p[1] != '\0') {
                fill = p[1];
                p += 2;
            } else if (*p == '^') {
                no_group = 1;
                p++;
            } else if (*p == '+') {
                paren_neg = 0;
                p++;
            } else if (*p == '(') {
                paren_neg = 1;
                p++;
            } else if (*p == '!') {
                suppress_curr = 1;
                p++;
            } else if (*p == '-') {
                left_align = 1;
                p++;
            } else {
                break;
            }
        }

        int width = 0;
        if (isdigit((unsigned char)*p)) {
            width = parse_uint(&p);
        }
        int left_prec = -1;
        if (*p == '#') {
            p++;
            left_prec = parse_uint(&p);
        }
        int right_prec = -1;
        if (*p == '.') {
            p++;
            right_prec = parse_uint(&p);
        }

        char conv = *p;
        if (conv != 'i' && conv != 'n') {
            errno = EINVAL;
            return -1;
        }

        double val = va_arg(ap, double);
        int neg = signbit(val) ? 1 : 0;
        double mag = fabs(val);

        char field[256];
        size_t flen = 0u;

        if (isnan(val) || isinf(val)) {
            const char *txt = isnan(val) ? "nan" : (neg ? "-inf" : "inf");
            flen = strlcpy(field, txt, sizeof(field));
        } else {
            int frac = right_prec;
            if (frac < 0) {
                char lcf =
                    (conv == 'i') ? lc->int_frac_digits : lc->frac_digits;
                frac = (lcf == CHAR_MAX || lcf < 0) ? 2 : (int)lcf;
            }
            if (frac > 18) {
                frac = 18;
            }

            char numbuf[128];
            (void)snprintf(numbuf, sizeof(numbuf), "%.*f", frac, mag);
            char *dot = strchr(numbuf, '.');
            size_t int_len = dot ? (size_t)(dot - numbuf) : strlen(numbuf);
            const char *frac_str = dot ? (dot + 1) : "";
            const char *dec_pt =
                (lc->mon_decimal_point && lc->mon_decimal_point[0] != '\0')
                    ? lc->mon_decimal_point
                    : ".";
            const char *tsep = (!no_group && lc->mon_thousands_sep &&
                                lc->mon_thousands_sep[0] != '\0' &&
                                lc->mon_grouping && lc->mon_grouping[0] > 0)
                                   ? lc->mon_thousands_sep
                                   : "";
            int grp =
                (!no_group && tsep[0] != '\0') ? (int)lc->mon_grouping[0] : 0;

            char body[192];
            size_t blen = 0u;
            int pad_digits =
                (left_prec > (int)int_len) ? (left_prec - (int)int_len) : 0;
            for (int k = 0; k < pad_digits && blen + 1u < sizeof(body); k++) {
                body[blen++] = fill;
            }
            for (size_t i = 0u; i < int_len && blen + 1u < sizeof(body); i++) {
                if (grp > 0 && i > 0u && ((int_len - i) % (size_t)grp) == 0u) {
                    size_t sl = strlen(tsep);
                    if (blen + sl < sizeof(body)) {
                        memcpy(body + blen, tsep, sl);
                        blen += sl;
                    }
                }
                body[blen++] = numbuf[i];
            }
            if (frac > 0) {
                size_t dl = strlen(dec_pt);
                if (blen + dl < sizeof(body)) {
                    memcpy(body + blen, dec_pt, dl);
                    blen += dl;
                }
                size_t frl = strlen(frac_str);
                if (blen + frl < sizeof(body)) {
                    memcpy(body + blen, frac_str, frl);
                    blen += frl;
                }
            }
            body[blen] = '\0';

            const char *csym = "";
            if (!suppress_curr) {
                csym =
                    (conv == 'i') ? lc->int_curr_symbol : lc->currency_symbol;
                if (!csym) {
                    csym = "";
                }
            }
            const char *nsign =
                (lc->negative_sign && lc->negative_sign[0] != '\0')
                    ? lc->negative_sign
                    : "-";
            const char *psign = lc->positive_sign ? lc->positive_sign : "";

            char pre[32] = {0};
            char post[32] = {0};
            if (neg) {
                if (paren_neg) {
                    (void)strlcpy(pre, "(", sizeof(pre));
                    (void)strlcpy(post, ")", sizeof(post));
                } else {
                    (void)strlcpy(pre, nsign, sizeof(pre));
                }
            } else if (psign[0] != '\0') {
                (void)strlcpy(pre, psign, sizeof(pre));
            }

            int n = snprintf(field, sizeof(field), "%s%s%s%s", pre, csym, body,
                             post);
            flen = (n > 0) ? (size_t)n : 0u;
            if (flen >= sizeof(field)) {
                flen = sizeof(field) - 1u;
            }
        }

        int pad = (width > (int)flen) ? (width - (int)flen) : 0;
        if (!left_align) {
            for (int k = 0; k < pad; k++) {
                if (out_put(s, maxsize, &pos, ' ') != 0) {
                    return -1;
                }
            }
        }
        if (out_puts(s, maxsize, &pos, field) != 0) {
            return -1;
        }
        if (left_align) {
            for (int k = 0; k < pad; k++) {
                if (out_put(s, maxsize, &pos, ' ') != 0) {
                    return -1;
                }
            }
        }
    }

    s[pos] = '\0';
    return (ssize_t)pos;
}

ssize_t strfmon_l(char *restrict s, size_t maxsize, locale_t locale,
                  const char *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    ssize_t res = vstrfmon_impl(s, maxsize, locale, format, ap);
    va_end(ap);
    return res;
}

ssize_t strfmon(char *restrict s, size_t maxsize, const char *restrict format,
                ...) {
    va_list ap;
    va_start(ap, format);
    ssize_t res = vstrfmon_impl(s, maxsize, uselocale((locale_t)0), format, ap);
    va_end(ap);
    return res;
}
