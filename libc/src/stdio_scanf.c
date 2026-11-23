/**
 * @file     stdio_scanf.c
 * @brief    POSIX.1-2024 scanf, fscanf, sscanf, vscanf, vfscanf, and vsscanf
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdio_internal.h"

/* need:: COMP_LIBC_0009 */

struct scan_src {
    FILE *fp;
    const unsigned char *sp;
    size_t consumed;
};

static int src_getc(struct scan_src *src) {
    int c = EOF;
    if (src->fp) {
        c = __stdio_fgetc_unlocked(src->fp);
    } else if (*src->sp != '\0') {
        c = (int)(*src->sp++);
    }
    if (c != EOF) {
        src->consumed++;
    }
    return c;
}

static void src_ungetc(struct scan_src *src, int c) {
    if (c == EOF) {
        return;
    }
    if (src->fp) {
        (void)__stdio_ungetc_unlocked(c, src->fp);
    } else {
        src->sp--;
    }
    src->consumed--;
}

static void skip_ws(struct scan_src *src) {
    int c;
    while ((c = src_getc(src)) != EOF) {
        if (!isspace((unsigned char)c)) {
            src_ungetc(src, c);
            return;
        }
    }
}

enum len_mod {
    LEN_NONE = 0,
    LEN_HH,
    LEN_H,
    LEN_L,
    LEN_LL,
    LEN_J,
    LEN_Z,
    LEN_T,
    LEN_CAP_L,
};

static void store_signed(va_list *ap, enum len_mod lm, long long val) {
    switch (lm) {
    case LEN_HH:
        *va_arg(*ap, signed char *) = (signed char)val;
        break;
    case LEN_H:
        *va_arg(*ap, short *) = (short)val;
        break;
    case LEN_L:
        *va_arg(*ap, long *) = (long)val;
        break;
    case LEN_LL:
        *va_arg(*ap, long long *) = val;
        break;
    case LEN_J:
        *va_arg(*ap, intmax_t *) = (intmax_t)val;
        break;
    case LEN_Z:
        *va_arg(*ap, ssize_t *) = (ssize_t)val;
        break;
    case LEN_T:
        *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)val;
        break;
    default:
        *va_arg(*ap, int *) = (int)val;
        break;
    }
}

static void store_unsigned(va_list *ap, enum len_mod lm,
                           unsigned long long val) {
    switch (lm) {
    case LEN_HH:
        *va_arg(*ap, unsigned char *) = (unsigned char)val;
        break;
    case LEN_H:
        *va_arg(*ap, unsigned short *) = (unsigned short)val;
        break;
    case LEN_L:
        *va_arg(*ap, unsigned long *) = (unsigned long)val;
        break;
    case LEN_LL:
        *va_arg(*ap, unsigned long long *) = val;
        break;
    case LEN_J:
        *va_arg(*ap, uintmax_t *) = (uintmax_t)val;
        break;
    case LEN_Z:
        *va_arg(*ap, size_t *) = (size_t)val;
        break;
    case LEN_T:
        *va_arg(*ap, ptrdiff_t *) = (ptrdiff_t)val;
        break;
    default:
        *va_arg(*ap, unsigned int *) = (unsigned int)val;
        break;
    }
}

static int digit_val(int c, int base) {
    int d = -1;
    if (c >= '0' && c <= '9') {
        d = c - '0';
    } else if (c >= 'a' && c <= 'z') {
        d = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
        d = c - 'A' + 10;
    }
    return (d >= 0 && d < base) ? d : -1;
}

static int scan_int(struct scan_src *src, int base, int width, int is_signed,
                    int suppress, enum len_mod lm, va_list *ap, int *eof_hit) {
    skip_ws(src);
    int c = src_getc(src);
    if (c == EOF) {
        *eof_hit = 1;
        return 0;
    }
    int rem = width > 0 ? width : INT_MAX;
    int neg = 0;
    if (c == '-' || c == '+') {
        neg = (c == '-');
        rem--;
        if (rem == 0) {
            return 0;
        }
        c = src_getc(src);
        if (c == EOF) {
            return 0;
        }
    }

    int any = 0;
    if (c == '0') {
        any = 1;
        rem--;
        if (rem > 0 && (base == 0 || base == 16)) {
            int c2 = src_getc(src);
            if (c2 == 'x' || c2 == 'X') {
                if (rem > 1) {
                    int c3 = src_getc(src);
                    if (digit_val(c3, 16) >= 0) {
                        base = 16;
                        rem--;
                        c = c3;
                    } else {
                        src_ungetc(src, c3);
                        src_ungetc(src, c2);
                        if (base == 0) {
                            base = 8;
                        }
                        c = EOF;
                    }
                } else {
                    src_ungetc(src, c2);
                    if (base == 0) {
                        base = 8;
                    }
                    c = EOF;
                }
            } else {
                src_ungetc(src, c2);
                if (base == 0) {
                    base = 8;
                }
                c = (rem > 0) ? src_getc(src) : EOF;
            }
        } else {
            if (base == 0) {
                base = 8;
            }
            c = (rem > 0) ? src_getc(src) : EOF;
        }
    } else if (base == 0) {
        base = 10;
    }

    unsigned long long acc = 0ULL;
    while (c != EOF && rem > 0) {
        int d = digit_val(c, base);
        if (d < 0) {
            src_ungetc(src, c);
            break;
        }
        any = 1;
        acc = acc * (unsigned long long)base + (unsigned long long)d;
        rem--;
        if (rem > 0) {
            c = src_getc(src);
        }
    }
    if (!any) {
        return 0;
    }
    if (!suppress) {
        if (is_signed) {
            long long sval = neg ? -(long long)acc : (long long)acc;
            store_signed(ap, lm, sval);
        } else {
            unsigned long long uval = neg ? (0ULL - acc) : acc;
            store_unsigned(ap, lm, uval);
        }
    }
    return 1;
}

static int scan_float(struct scan_src *src, int width, int suppress,
                      enum len_mod lm, va_list *ap, int *eof_hit) {
    skip_ws(src);
    int c = src_getc(src);
    if (c == EOF) {
        *eof_hit = 1;
        return 0;
    }

    char buf[256] = {0};
    size_t len = 0;
    int rem = width > 0 ? width : (int)(sizeof(buf) - 1u);
    if (rem > (int)(sizeof(buf) - 1u)) {
        rem = (int)(sizeof(buf) - 1u);
    }

    if (c == '+' || c == '-') {
        buf[len++] = (char)c;
        rem--;
        c = (rem > 0) ? src_getc(src) : EOF;
    }

    if (c != EOF && (tolower((unsigned char)c) == 'i' ||
                     tolower((unsigned char)c) == 'n')) {
        while (
            c != EOF && rem > 0 &&
            (isalnum((unsigned char)c) || c == '_' || c == '(' || c == ')')) {
            buf[len++] = (char)c;
            rem--;
            if (c == ')') {
                c = EOF;
                break;
            }
            c = (rem > 0) ? src_getc(src) : EOF;
        }
        if (c != EOF) {
            src_ungetc(src, c);
        }
    } else {
        int hex = 0;
        int seen_dot = 0;
        if (c == '0' && rem > 1) {
            buf[len++] = (char)c;
            rem--;
            int c2 = src_getc(src);
            if (c2 == 'x' || c2 == 'X') {
                hex = 1;
                buf[len++] = (char)c2;
                rem--;
                c = (rem > 0) ? src_getc(src) : EOF;
            } else {
                c = c2;
            }
        }
        while (c != EOF && rem > 0) {
            if (hex ? isxdigit((unsigned char)c) : isdigit((unsigned char)c)) {
                buf[len++] = (char)c;
                rem--;
                c = (rem > 0) ? src_getc(src) : EOF;
            } else if (c == '.' && !seen_dot) {
                seen_dot = 1;
                buf[len++] = (char)c;
                rem--;
                c = (rem > 0) ? src_getc(src) : EOF;
            } else {
                break;
            }
        }
        if (c != EOF && rem > 0 &&
            (hex ? (c == 'p' || c == 'P') : (c == 'e' || c == 'E'))) {
            buf[len++] = (char)c;
            rem--;
            c = (rem > 0) ? src_getc(src) : EOF;
            if (c != EOF && rem > 0 && (c == '+' || c == '-')) {
                buf[len++] = (char)c;
                rem--;
                c = (rem > 0) ? src_getc(src) : EOF;
            }
            while (c != EOF && rem > 0 && isdigit((unsigned char)c)) {
                buf[len++] = (char)c;
                rem--;
                c = (rem > 0) ? src_getc(src) : EOF;
            }
        }
        if (c != EOF) {
            src_ungetc(src, c);
        }
    }

    buf[len] = '\0';
    char *end = 0;
    long double val = strtold(buf, &end);
    if (end == buf) {
        return 0;
    }
    /* Push back any trailing unconsumed characters (e.g., incomplete "1e+"). */
    for (size_t k = len; k > (size_t)(end - buf); k--) {
        src_ungetc(src, (unsigned char)buf[k - 1u]);
    }

    if (!suppress) {
        if (lm == LEN_CAP_L) {
            *va_arg(*ap, long double *) = val;
        } else if (lm == LEN_L) {
            *va_arg(*ap, double *) = (double)val;
        } else {
            *va_arg(*ap, float *) = (float)val;
        }
    }
    return 1;
}

static int scan_core(struct scan_src *src, const char *fmt, va_list ap) {
    va_list args;
    va_copy(args, ap);
    int assigned = 0;
    int any_conv = 0;

    for (const char *f = fmt; *f; f++) {
        if (isspace((unsigned char)*f)) {
            skip_ws(src);
            continue;
        }
        if (*f != '%') {
            int c = src_getc(src);
            if (c == EOF) {
                if (!any_conv) {
                    va_end(args);
                    return EOF;
                }
                break;
            }
            if (c != (unsigned char)*f) {
                src_ungetc(src, c);
                break;
            }
            continue;
        }

        f++;
        if (*f == '%') {
            skip_ws(src);
            int c = src_getc(src);
            if (c == EOF) {
                if (!any_conv) {
                    va_end(args);
                    return EOF;
                }
                break;
            }
            if (c != '%') {
                src_ungetc(src, c);
                break;
            }
            any_conv = 1;
            continue;
        }

        int suppress = 0;
        if (*f == '*') {
            suppress = 1;
            f++;
        }

        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }

        int alloc_m = 0;
        if (*f == 'm') {
            alloc_m = 1;
            f++;
        }

        enum len_mod lm = LEN_NONE;
        if (*f == 'h') {
            f++;
            if (*f == 'h') {
                lm = LEN_HH;
                f++;
            } else {
                lm = LEN_H;
            }
        } else if (*f == 'l') {
            f++;
            if (*f == 'l') {
                lm = LEN_LL;
                f++;
            } else {
                lm = LEN_L;
            }
        } else if (*f == 'j') {
            lm = LEN_J;
            f++;
        } else if (*f == 'z') {
            lm = LEN_Z;
            f++;
        } else if (*f == 't') {
            lm = LEN_T;
            f++;
        } else if (*f == 'L') {
            lm = LEN_CAP_L;
            f++;
        }

        if (!*f) {
            break;
        }

        int eof_hit = 0;
        int ok = 0;
        switch (*f) {
        case 'd':
            ok = scan_int(src, 10, width, 1, suppress, lm, &args, &eof_hit);
            break;
        case 'i':
            ok = scan_int(src, 0, width, 1, suppress, lm, &args, &eof_hit);
            break;
        case 'u':
            ok = scan_int(src, 10, width, 0, suppress, lm, &args, &eof_hit);
            break;
        case 'o':
            ok = scan_int(src, 8, width, 0, suppress, lm, &args, &eof_hit);
            break;
        case 'x':
        case 'X':
            ok = scan_int(src, 16, width, 0, suppress, lm, &args, &eof_hit);
            break;
        case 'p': {
            skip_ws(src);
            unsigned long long ptr_val = 0ULL;
            int rem = width > 0 ? width : INT_MAX;
            int c = src_getc(src);
            if (c == EOF) {
                eof_hit = 1;
                break;
            }
            if (c == '(' && rem >= 5) {
                char tmp[6];
                tmp[0] = (char)c;
                int n = 1;
                while (n < 5) {
                    int nc = src_getc(src);
                    if (nc == EOF) {
                        break;
                    }
                    tmp[n++] = (char)nc;
                }
                tmp[n] = '\0';
                if (strcmp(tmp, "(nil)") == 0) {
                    if (!suppress) {
                        *va_arg(args, void **) = 0;
                    }
                    ok = 1;
                    break;
                }
                while (n > 0) {
                    src_ungetc(src, (unsigned char)tmp[--n]);
                }
                break;
            }
            src_ungetc(src, c);
            int any = 0;
            int c1 = src_getc(src);
            if (c1 == '0') {
                any = 1;
                rem--;
                if (rem > 0) {
                    int c2 = src_getc(src);
                    if (c2 == 'x' || c2 == 'X') {
                        rem--;
                    } else {
                        src_ungetc(src, c2);
                    }
                }
            } else if (c1 != EOF) {
                src_ungetc(src, c1);
            }
            while (rem > 0) {
                int ch = src_getc(src);
                int d = digit_val(ch, 16);
                if (d < 0) {
                    src_ungetc(src, ch);
                    break;
                }
                any = 1;
                ptr_val = (ptr_val << 4) | (unsigned long long)d;
                rem--;
            }
            if (any) {
                if (!suppress) {
                    *va_arg(args, void **) = (void *)(uintptr_t)ptr_val;
                }
                ok = 1;
            }
            break;
        }
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            ok = scan_float(src, width, suppress, lm, &args, &eof_hit);
            break;
        case 's': {
            skip_ws(src);
            int c = src_getc(src);
            if (c == EOF) {
                eof_hit = 1;
                break;
            }
            int rem = width > 0 ? width : INT_MAX;
            char *dst = 0;
            char **dstp = 0;
            size_t cap = 0;
            size_t n = 0;
            if (!suppress) {
                if (alloc_m) {
                    dstp = va_arg(args, char **);
                    cap = 32u;
                    dst = (char *)malloc(cap);
                    if (!dst) {
                        src_ungetc(src, c);
                        errno = ENOMEM;
                        eof_hit = 1;
                        break;
                    }
                } else {
                    dst = va_arg(args, char *);
                }
            }
            while (c != EOF && rem > 0 && !isspace((unsigned char)c)) {
                if (dst) {
                    if (alloc_m && n + 1u >= cap) {
                        char *nb = (char *)realloc(dst, cap * 2u);
                        if (!nb) {
                            free(dst);
                            errno = ENOMEM;
                            va_end(args);
                            return assigned > 0 ? assigned : EOF;
                        }
                        dst = nb;
                        cap *= 2u;
                    }
                    dst[n] = (char)c;
                }
                n++;
                rem--;
                if (rem > 0) {
                    c = src_getc(src);
                }
            }
            if (c != EOF && rem > 0 && isspace((unsigned char)c)) {
                src_ungetc(src, c);
            }
            if (n > 0u) {
                if (dst) {
                    dst[n] = '\0';
                    if (alloc_m && dstp) {
                        *dstp = dst;
                    }
                }
                ok = 1;
            } else if (alloc_m && dst) {
                free(dst);
            }
            break;
        }
        case 'c': {
            int want = width > 0 ? width : 1;
            char *dst = 0;
            char **dstp = 0;
            if (!suppress) {
                if (alloc_m) {
                    dstp = va_arg(args, char **);
                    dst = (char *)malloc((size_t)want);
                    if (!dst) {
                        errno = ENOMEM;
                        eof_hit = 1;
                        break;
                    }
                } else {
                    dst = va_arg(args, char *);
                }
            }
            int got = 0;
            while (got < want) {
                int c = src_getc(src);
                if (c == EOF) {
                    if (got == 0) {
                        eof_hit = 1;
                    }
                    break;
                }
                if (dst) {
                    dst[got] = (char)c;
                }
                got++;
            }
            if (got == want) {
                if (alloc_m && dstp) {
                    *dstp = dst;
                }
                ok = 1;
            } else if (alloc_m && dst) {
                free(dst);
            }
            break;
        }
        case '[': {
            unsigned char set[256];
            memset(set, 0, sizeof(set));
            int negate = 0;
            f++;
            if (*f == '^') {
                negate = 1;
                f++;
            }
            if (*f == ']') {
                set[(unsigned char)']'] = 1u;
                f++;
            }
            while (*f && *f != ']') {
                unsigned char ch = (unsigned char)*f;
                if (f[1] == '-' && f[2] && f[2] != ']') {
                    unsigned char end_ch = (unsigned char)f[2];
                    if (ch <= end_ch) {
                        for (unsigned int k = ch; k <= end_ch; k++) {
                            set[k] = 1u;
                        }
                    }
                    f += 3;
                } else {
                    set[ch] = 1u;
                    f++;
                }
            }
            if (*f != ']') {
                va_end(args);
                return assigned;
            }

            int c = src_getc(src);
            if (c == EOF) {
                eof_hit = 1;
                break;
            }
            int rem = width > 0 ? width : INT_MAX;
            char *dst = 0;
            char **dstp = 0;
            size_t cap = 0;
            size_t n = 0;
            if (!suppress) {
                if (alloc_m) {
                    dstp = va_arg(args, char **);
                    cap = 32u;
                    dst = (char *)malloc(cap);
                    if (!dst) {
                        src_ungetc(src, c);
                        errno = ENOMEM;
                        eof_hit = 1;
                        break;
                    }
                } else {
                    dst = va_arg(args, char *);
                }
            }
            while (c != EOF && rem > 0) {
                int in = set[(unsigned char)c] != 0u;
                if (negate) {
                    in = !in;
                }
                if (!in) {
                    src_ungetc(src, c);
                    break;
                }
                if (dst) {
                    if (alloc_m && n + 1u >= cap) {
                        char *nb = (char *)realloc(dst, cap * 2u);
                        if (!nb) {
                            free(dst);
                            errno = ENOMEM;
                            va_end(args);
                            return assigned > 0 ? assigned : EOF;
                        }
                        dst = nb;
                        cap *= 2u;
                    }
                    dst[n] = (char)c;
                }
                n++;
                rem--;
                if (rem > 0) {
                    c = src_getc(src);
                }
            }
            if (n > 0u) {
                if (dst) {
                    dst[n] = '\0';
                    if (alloc_m && dstp) {
                        *dstp = dst;
                    }
                }
                ok = 1;
            } else if (alloc_m && dst) {
                free(dst);
            }
            break;
        }
        case 'n':
            if (!suppress) {
                store_signed(&args, lm, (long long)src->consumed);
            }
            continue;
        default:
            va_end(args);
            return assigned;
        }

        if (!ok) {
            if (eof_hit && !any_conv) {
                va_end(args);
                return EOF;
            }
            break;
        }
        any_conv = 1;
        if (!suppress) {
            assigned++;
        }
    }

    va_end(args);
    return assigned;
}

int vfscanf(FILE *restrict stream, const char *restrict fmt, va_list ap) {
    if (!stream || !fmt) {
        errno = EINVAL;
        return EOF;
    }
    int res = EOF;
    flockfile(stream);
    pthread_cleanup_push(__stdio_unlock_cleanup, stream);
    struct scan_src src = {.fp = stream, .sp = 0, .consumed = 0u};
    res = scan_core(&src, fmt, ap);
    pthread_cleanup_pop(1);
    return res;
}

int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap) {
    if (!s || !fmt) {
        errno = EINVAL;
        return EOF;
    }
    struct scan_src src = {
        .fp = 0, .sp = (const unsigned char *)s, .consumed = 0u};
    return scan_core(&src, fmt, ap);
}

int vscanf(const char *restrict fmt, va_list ap) {
    return vfscanf(stdin, fmt, ap);
}

int fscanf(FILE *restrict stream, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfscanf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int sscanf(const char *restrict s, const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

int scanf(const char *restrict fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfscanf(stdin, fmt, ap);
    va_end(ap);
    return r;
}
