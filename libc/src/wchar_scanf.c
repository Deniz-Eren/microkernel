/**
 * @file     wchar_scanf.c
 * @brief    POSIX.1-2024 fwscanf, wscanf, swscanf, and v*wscanf family
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>
#include "stdio_internal.h"

/* need:: COMP_LIBC_0013 */

struct wscan_src {
    FILE *fp;
    const wchar_t *wp;
    size_t consumed;
};

static wint_t wsrc_getc(struct wscan_src *src) {
    wint_t wc = WEOF;
    if (src->fp) {
        wc = __stdio_fgetwc_unlocked(src->fp);
    } else if (*src->wp != L'\0') {
        wc = (wint_t)(*src->wp++);
    }
    if (wc != WEOF) {
        src->consumed++;
    }
    return wc;
}

static void wsrc_ungetc(struct wscan_src *src, wint_t wc) {
    if (wc == WEOF) {
        return;
    }
    if (src->fp) {
        (void)__stdio_ungetwc_unlocked(wc, src->fp);
    } else {
        src->wp--;
    }
    src->consumed--;
}

static void wskip_ws(struct wscan_src *src) {
    wint_t wc;
    while ((wc = wsrc_getc(src)) != WEOF) {
        if (!iswspace(wc)) {
            wsrc_ungetc(src, wc);
            return;
        }
    }
}

enum wlen_mod {
    WLEN_NONE = 0,
    WLEN_HH,
    WLEN_H,
    WLEN_L,
    WLEN_LL,
    WLEN_J,
    WLEN_Z,
    WLEN_T,
    WLEN_CAP_L,
};

static void wstore_signed(va_list *ap, enum wlen_mod lm, long long val) {
    switch (lm) {
    case WLEN_HH:
        *va_arg(*ap, signed char *) = (signed char)val;
        break;
    case WLEN_H:
        *va_arg(*ap, short *) = (short)val;
        break;
    case WLEN_L:
        *va_arg(*ap, long *) = (long)val;
        break;
    case WLEN_LL:
        *va_arg(*ap, long long *) = val;
        break;
    case WLEN_J:
        *va_arg(*ap, intmax_t *) = (intmax_t)val;
        break;
    case WLEN_Z:
        *va_arg(*ap, ssize_t *) = (ssize_t)val;
        break;
    case WLEN_T:
        *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)val;
        break;
    default:
        *va_arg(*ap, int *) = (int)val;
        break;
    }
}

static void wstore_unsigned(va_list *ap, enum wlen_mod lm,
                            unsigned long long val) {
    switch (lm) {
    case WLEN_HH:
        *va_arg(*ap, unsigned char *) = (unsigned char)val;
        break;
    case WLEN_H:
        *va_arg(*ap, unsigned short *) = (unsigned short)val;
        break;
    case WLEN_L:
        *va_arg(*ap, unsigned long *) = (unsigned long)val;
        break;
    case WLEN_LL:
        *va_arg(*ap, unsigned long long *) = val;
        break;
    case WLEN_J:
        *va_arg(*ap, uintmax_t *) = (uintmax_t)val;
        break;
    case WLEN_Z:
        *va_arg(*ap, size_t *) = (size_t)val;
        break;
    case WLEN_T:
        *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)val;
        break;
    default:
        *va_arg(*ap, unsigned int *) = (unsigned int)val;
        break;
    }
}

static int wdigit_val(wint_t wc) {
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

static int wscan_int(struct wscan_src *src, int base, int is_signed, int width,
                     int suppress, enum wlen_mod lm, va_list *ap,
                     int *input_fail) {
    wskip_ws(src);
    size_t rem = width > 0 ? (size_t)width : SIZE_MAX;
    wint_t wc = wsrc_getc(src);
    if (wc == WEOF) {
        *input_fail = 1;
        return 0;
    }

    int neg = 0;
    if (wc == L'+' || wc == L'-') {
        neg = (wc == L'-');
        rem--;
        if (rem == 0u) {
            return 0;
        }
        wc = wsrc_getc(src);
        if (wc == WEOF) {
            return 0;
        }
    }

    int any = 0;
    if (base == 0 || base == 16) {
        if (wc == L'0') {
            any = 1;
            rem--;
            if (rem > 0u) {
                wint_t nx = wsrc_getc(src);
                if (nx == L'x' || nx == L'X') {
                    rem--;
                    if (rem > 0u) {
                        wint_t d = wsrc_getc(src);
                        if (d != WEOF && wdigit_val(d) >= 0 &&
                            wdigit_val(d) < 16) {
                            base = 16;
                            wc = d;
                        } else {
                            wsrc_ungetc(src, d);
                            if (base == 0) {
                                base = 8;
                            }
                            wc = WEOF;
                        }
                    } else {
                        if (base == 0) {
                            base = 8;
                        }
                        wc = WEOF;
                    }
                } else {
                    wsrc_ungetc(src, nx);
                    if (base == 0) {
                        base = 8;
                    }
                    wc = WEOF;
                }
            } else {
                if (base == 0) {
                    base = 8;
                }
                wc = WEOF;
            }
        } else if (base == 0) {
            base = 10;
        }
    }

    unsigned long long acc = 0ULL;
    unsigned long long ubase = (unsigned long long)base;
    while (wc != WEOF) {
        int d = wdigit_val(wc);
        if (d < 0 || d >= base) {
            wsrc_ungetc(src, wc);
            break;
        }
        any = 1;
        acc = acc * ubase + (unsigned long long)d;
        rem--;
        if (rem == 0u) {
            break;
        }
        wc = wsrc_getc(src);
    }

    if (!any) {
        return 0;
    }
    if (!suppress) {
        if (is_signed) {
            long long sv = neg ? -(long long)acc : (long long)acc;
            wstore_signed(ap, lm, sv);
        } else {
            unsigned long long uv = neg ? (0ULL - acc) : acc;
            wstore_unsigned(ap, lm, uv);
        }
    }
    return 1;
}

static int wscan_float(struct wscan_src *src, int width, int suppress,
                       enum wlen_mod lm, va_list *ap, int *input_fail) {
    wskip_ws(src);
    size_t rem = width > 0 ? (size_t)width : 255u;
    if (rem > 255u) {
        rem = 255u;
    }
    char buf[256] = {0};
    size_t len = 0u;
    wint_t wc = wsrc_getc(src);
    if (wc == WEOF) {
        *input_fail = 1;
        return 0;
    }
    if (wc == L'+' || wc == L'-') {
        buf[len++] = (char)wc;
        rem--;
        wc = rem > 0u ? wsrc_getc(src) : WEOF;
    }
    while (wc != WEOF && rem > 0u && len < 255u &&
           ((wc >= L'0' && wc <= L'9') || (wc >= L'a' && wc <= L'z') ||
            (wc >= L'A' && wc <= L'Z') || wc == L'.' || wc == L'+' ||
            wc == L'-' || wc == L'(' || wc == L')' || wc == L'_')) {
        if ((wc == L'+' || wc == L'-') && len > 0u && buf[len - 1u] != 'e' &&
            buf[len - 1u] != 'E' && buf[len - 1u] != 'p' &&
            buf[len - 1u] != 'P') {
            wsrc_ungetc(src, wc);
            break;
        }
        buf[len++] = (char)wc;
        rem--;
        wc = rem > 0u ? wsrc_getc(src) : WEOF;
    }
    if (wc != WEOF && rem > 0u) {
        wsrc_ungetc(src, wc);
    }
    buf[len] = '\0';
    char *ep = buf;
    long double val = strtold(buf, &ep);
    if (ep == buf) {
        return 0;
    }
    if (!suppress) {
        if (lm == WLEN_CAP_L) {
            *va_arg(*ap, long double *) = val;
        } else if (lm == WLEN_L) {
            *va_arg(*ap, double *) = (double)val;
        } else {
            *va_arg(*ap, float *) = (float)val;
        }
    }
    return 1;
}

static int wscanset_match(const wchar_t *pat_start, const wchar_t *pat_end,
                          int neg, wint_t wc) {
    int hit = 0;
    for (const wchar_t *p = pat_start; p < pat_end; p++) {
        if (p + 2 < pat_end && p[1] == L'-') {
            if (wc >= (wint_t)p[0] && wc <= (wint_t)p[2]) {
                hit = 1;
                break;
            }
            p += 2;
        } else if (wc == (wint_t)*p) {
            hit = 1;
            break;
        }
    }
    return neg ? !hit : hit;
}

static int emit_scanned_wcs(const wchar_t *wbuf, size_t wlen, int is_wide,
                            int alloc_m, int add_nul, va_list *ap) {
    if (is_wide) {
        if (alloc_m) {
            wchar_t **outp = va_arg(*ap, wchar_t **);
            wchar_t *dst = (wchar_t *)reallocarray(
                NULL, wlen + (add_nul ? 1u : 0u), sizeof(wchar_t));
            if (!dst) {
                *outp = NULL;
                errno = ENOMEM;
                return -1;
            }
            if (wlen > 0u) {
                (void)wmemcpy(dst, wbuf, wlen);
            }
            if (add_nul) {
                dst[wlen] = L'\0';
            }
            *outp = dst;
        } else {
            wchar_t *dst = va_arg(*ap, wchar_t *);
            if (wlen > 0u) {
                (void)wmemcpy(dst, wbuf, wlen);
            }
            if (add_nul) {
                dst[wlen] = L'\0';
            }
        }
        return 0;
    }
    /* Convert wide chars to multibyte char buffer. */
    size_t mcap = wlen * 4u + 1u;
    char *mbuf = (char *)malloc(mcap);
    if (!mbuf) {
        errno = ENOMEM;
        return -1;
    }
    mbstate_t st = {0};
    size_t mlen = 0u;
    for (size_t i = 0u; i < wlen; i++) {
        size_t nb = wcrtomb(mbuf + mlen, wbuf[i], &st);
        if (nb == (size_t)-1) {
            free(mbuf);
            return -1;
        }
        mlen += nb;
    }
    if (add_nul) {
        mbuf[mlen] = '\0';
    }
    if (alloc_m) {
        char **outp = va_arg(*ap, char **);
        *outp = mbuf;
    } else {
        char *dst = va_arg(*ap, char *);
        memcpy(dst, mbuf, mlen + (add_nul ? 1u : 0u));
        free(mbuf);
    }
    return 0;
}

static int vwscanf_core(struct wscan_src *src, const wchar_t *fmt, va_list ap) {
    va_list args;
    va_copy(args, ap);
    int assigned = 0;
    int input_fail = 0;

    for (const wchar_t *f = fmt; *f != L'\0'; f++) {
        if (iswspace((wint_t)*f)) {
            wskip_ws(src);
            continue;
        }
        if (*f != L'%') {
            wint_t wc = wsrc_getc(src);
            if (wc == WEOF) {
                input_fail = 1;
                break;
            }
            if (wc != (wint_t)*f) {
                wsrc_ungetc(src, wc);
                break;
            }
            continue;
        }
        f++;
        if (*f == L'%') {
            wskip_ws(src);
            wint_t wc = wsrc_getc(src);
            if (wc == WEOF) {
                input_fail = 1;
                break;
            }
            if (wc != L'%') {
                wsrc_ungetc(src, wc);
                break;
            }
            continue;
        }

        int suppress = 0;
        if (*f == L'*') {
            suppress = 1;
            f++;
        }
        int width = 0;
        while (*f >= L'0' && *f <= L'9') {
            width = width * 10 + (int)(*f - L'0');
            f++;
        }
        int alloc_m = 0;
        if (*f == L'm') {
            alloc_m = 1;
            f++;
        }

        enum wlen_mod lm = WLEN_NONE;
        if (*f == L'h') {
            f++;
            if (*f == L'h') {
                lm = WLEN_HH;
                f++;
            } else {
                lm = WLEN_H;
            }
        } else if (*f == L'l') {
            f++;
            if (*f == L'l') {
                lm = WLEN_LL;
                f++;
            } else {
                lm = WLEN_L;
            }
        } else if (*f == L'j') {
            lm = WLEN_J;
            f++;
        } else if (*f == L'z') {
            lm = WLEN_Z;
            f++;
        } else if (*f == L't') {
            lm = WLEN_T;
            f++;
        } else if (*f == L'L') {
            lm = WLEN_CAP_L;
            f++;
        }
        if (*f == L'\0') {
            break;
        }

        if (*f == L'n') {
            if (!suppress) {
                wstore_signed(&args, lm, (long long)src->consumed);
            }
            continue;
        }
        if (*f == L'd' || *f == L'i' || *f == L'u' || *f == L'o' ||
            *f == L'x' || *f == L'X' || *f == L'p') {
            int base =
                (*f == L'i')
                    ? 0
                    : ((*f == L'o') ? 8
                                    : ((*f == L'd' || *f == L'u') ? 10 : 16));
            int is_sig = (*f == L'd' || *f == L'i');
            if (*f == L'p') {
                lm = WLEN_L;
            }
            if (!wscan_int(src, base, is_sig, width, suppress, lm, &args,
                           &input_fail)) {
                break;
            }
            if (!suppress) {
                assigned++;
            }
            continue;
        }
        if (*f == L'f' || *f == L'F' || *f == L'e' || *f == L'E' ||
            *f == L'g' || *f == L'G' || *f == L'a' || *f == L'A') {
            if (!wscan_float(src, width, suppress, lm, &args, &input_fail)) {
                break;
            }
            if (!suppress) {
                assigned++;
            }
            continue;
        }
        if (*f == L's' || *f == L'c' || *f == L'[') {
            wchar_t spec = *f;
            const wchar_t *pat_start = NULL;
            const wchar_t *pat_end = NULL;
            int neg = 0;
            if (spec == L'[') {
                f++;
                if (*f == L'^') {
                    neg = 1;
                    f++;
                }
                pat_start = f;
                if (*f == L']') {
                    f++;
                }
                while (*f != L'\0' && *f != L']') {
                    f++;
                }
                pat_end = f;
                if (*f == L'\0') {
                    break;
                }
            }
            if (spec == L's') {
                wskip_ws(src);
            }
            size_t want = (spec == L'c')
                              ? (width > 0 ? (size_t)width : 1u)
                              : (width > 0 ? (size_t)width : SIZE_MAX);
            size_t cap = 64u;
            wchar_t *wtmp = (wchar_t *)reallocarray(NULL, cap, sizeof(wchar_t));
            if (!wtmp) {
                errno = ENOMEM;
                input_fail = 1;
                break;
            }
            size_t len = 0u;
            while (len < want) {
                wint_t wc = wsrc_getc(src);
                if (wc == WEOF) {
                    if (len == 0u) {
                        input_fail = 1;
                    }
                    break;
                }
                if (spec == L's' && iswspace(wc)) {
                    wsrc_ungetc(src, wc);
                    break;
                }
                if (spec == L'[' &&
                    !wscanset_match(pat_start, pat_end, neg, wc)) {
                    wsrc_ungetc(src, wc);
                    break;
                }
                if (len == cap) {
                    size_t ncap = cap * 2u;
                    wchar_t *nb =
                        (wchar_t *)reallocarray(wtmp, ncap, sizeof(wchar_t));
                    if (!nb) {
                        free(wtmp);
                        errno = ENOMEM;
                        input_fail = 1;
                        goto done;
                    }
                    wtmp = nb;
                    cap = ncap;
                }
                wtmp[len++] = (wchar_t)wc;
            }
            if (len == 0u || (spec == L'c' && len < want)) {
                free(wtmp);
                break;
            }
            if (!suppress) {
                int is_wide = (lm == WLEN_L);
                if (emit_scanned_wcs(wtmp, len, is_wide, alloc_m, spec != L'c',
                                     &args) != 0) {
                    free(wtmp);
                    input_fail = 1;
                    break;
                }
                assigned++;
            }
            free(wtmp);
            continue;
        }
        break;
    }

done:
    va_end(args);
    if (assigned == 0 && input_fail) {
        return EOF;
    }
    return assigned;
}

int vfwscanf(FILE *restrict stream, const wchar_t *restrict format,
             va_list arg) {
    if (!stream || !format) {
        errno = EINVAL;
        return EOF;
    }
    struct wscan_src src = {.fp = stream, .wp = NULL, .consumed = 0u};
    int r = EOF;
    flockfile(stream);
    pthread_cleanup_push(__stdio_unlock_cleanup, stream);
    r = vwscanf_core(&src, format, arg);
    pthread_cleanup_pop(1);
    return r;
}

int vwscanf(const wchar_t *restrict format, va_list arg) {
    return vfwscanf(stdin, format, arg);
}

int vswscanf(const wchar_t *restrict ws, const wchar_t *restrict format,
             va_list arg) {
    if (!ws || !format) {
        errno = EINVAL;
        return EOF;
    }
    struct wscan_src src = {.fp = NULL, .wp = ws, .consumed = 0u};
    return vwscanf_core(&src, format, arg);
}

int fwscanf(FILE *restrict stream, const wchar_t *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfwscanf(stream, format, ap);
    va_end(ap);
    return r;
}

int wscanf(const wchar_t *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfwscanf(stdin, format, ap);
    va_end(ap);
    return r;
}

int swscanf(const wchar_t *restrict ws, const wchar_t *restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vswscanf(ws, format, ap);
    va_end(ap);
    return r;
}
