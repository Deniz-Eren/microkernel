/**
 * @file     wchar_mb.c
 * @brief    POSIX.1-2024 multibyte and restartable wide character conversions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "locale_internal.h"

/* need:: COMP_LIBC_0012 */

static mbstate_t g_mbrlen_state;
static mbstate_t g_mbrtowc_state;
static mbstate_t g_wcrtomb_state;
static mbstate_t g_mbsrtowcs_state;
static mbstate_t g_wcsrtombs_state;

static int is_valid_scalar(uint32_t cp, uint8_t expected) {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return 0;
    }
    if (expected == 2u && cp < 0x80u) {
        return 0;
    }
    if (expected == 3u && cp < 0x800u) {
        return 0;
    }
    if (expected == 4u && cp < 0x10000u) {
        return 0;
    }
    return 1;
}

static size_t utf8_decode_step(uint32_t *out_cp, const char *s, size_t n,
                               mbstate_t *ps) {
    size_t consumed = 0u;
    while (consumed < n) {
        uint8_t b = (uint8_t)s[consumed++];
        if (ps->__expected == 0u) {
            if (b < 0x80u) {
                *out_cp = b;
                return (b == 0u) ? 0u : consumed;
            }
            if ((b & 0xE0u) == 0xC0u) {
                ps->__codepoint = (uint32_t)(b & 0x1Fu);
                ps->__expected = 2u;
                ps->__seen = 1u;
            } else if ((b & 0xF0u) == 0xE0u) {
                ps->__codepoint = (uint32_t)(b & 0x0Fu);
                ps->__expected = 3u;
                ps->__seen = 1u;
            } else if ((b & 0xF8u) == 0xF0u) {
                ps->__codepoint = (uint32_t)(b & 0x07u);
                ps->__expected = 4u;
                ps->__seen = 1u;
            } else {
                memset(ps, 0, sizeof(*ps));
                errno = EILSEQ;
                return (size_t)-1;
            }
        } else {
            if ((b & 0xC0u) != 0x80u) {
                memset(ps, 0, sizeof(*ps));
                errno = EILSEQ;
                return (size_t)-1;
            }
            ps->__codepoint = (ps->__codepoint << 6) | (uint32_t)(b & 0x3Fu);
            ps->__seen++;
            if (ps->__seen == ps->__expected) {
                uint32_t cp = ps->__codepoint;
                uint8_t exp = ps->__expected;
                ps->__codepoint = 0u;
                ps->__expected = 0u;
                ps->__seen = 0u;
                if (!is_valid_scalar(cp, exp)) {
                    errno = EILSEQ;
                    return (size_t)-1;
                }
                *out_cp = cp;
                return consumed;
            }
        }
    }
    return (size_t)-2;
}

static size_t utf8_encode_cp(char *s, uint32_t cp) {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    if (cp < 0x80u) {
        s[0] = (char)cp;
        return 1u;
    }
    if (cp < 0x800u) {
        s[0] = (char)(0xC0u | (cp >> 6));
        s[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2u;
    }
    if (cp < 0x10000u) {
        s[0] = (char)(0xE0u | (cp >> 12));
        s[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        s[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3u;
    }
    s[0] = (char)(0xF0u | (cp >> 18));
    s[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    s[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    s[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4u;
}

wint_t btowc(int c) {
    if (c == EOF) {
        return WEOF;
    }
    unsigned char uc = (unsigned char)c;
    if (uc < 0x80u) {
        return (wint_t)uc;
    }
    return WEOF;
}

int wctob(wint_t c) {
    if (c == WEOF) {
        return EOF;
    }
    if (c >= 0 && c < 0x80) {
        return (int)c;
    }
    return EOF;
}

int mbsinit(const mbstate_t *ps) {
    if (!ps) {
        return 1;
    }
    return ps->__expected == 0u && ps->__seen == 0u && ps->__pending_low == 0u;
}

size_t mbrtowc(wchar_t *restrict pwc, const char *restrict s, size_t n,
               mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_mbrtowc_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 0u;
    }
    if (n == 0u) {
        return (size_t)-2;
    }
    if (!__libc_locale_is_utf8((locale_t)0)) {
        uint8_t b = (uint8_t)s[0];
        if (b >= 0x80u) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (pwc) {
            *pwc = (wchar_t)b;
        }
        return (b == 0u) ? 0u : 1u;
    }
    uint32_t cp = 0u;
    size_t rc = utf8_decode_step(&cp, s, n, st);
    if (rc != (size_t)-1 && rc != (size_t)-2 && pwc) {
        *pwc = (wchar_t)cp;
    }
    return rc;
}

size_t mbrlen(const char *restrict s, size_t n, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_mbrlen_state;
    return mbrtowc(NULL, s, n, st);
}

size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_wcrtomb_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 1u;
    }
    if (wc < 0) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    uint32_t cp = (uint32_t)wc;
    if (!__libc_locale_is_utf8((locale_t)0)) {
        if (cp >= 0x80u) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        s[0] = (char)cp;
        return 1u;
    }
    return utf8_encode_cp(s, cp);
}

size_t mbsnrtowcs(wchar_t *restrict dst, const char **restrict src, size_t nms,
                  size_t len, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_mbsrtowcs_state;
    const char *p = *src;
    size_t count = 0u;
    size_t rem = nms;

    while (rem > 0u && (!dst || count < len)) {
        wchar_t wc = 0;
        size_t rc = mbrtowc(&wc, p, rem, st);
        if (rc == (size_t)-1) {
            if (dst) {
                *src = p;
            }
            return (size_t)-1;
        }
        if (rc == (size_t)-2) {
            if (dst) {
                *src = p + rem;
            }
            return count;
        }
        if (rc == 0u) {
            if (dst) {
                dst[count] = L'\0';
                *src = NULL;
            }
            return count;
        }
        if (dst) {
            dst[count] = wc;
        }
        count++;
        p += rc;
        rem -= rc;
    }
    if (dst) {
        *src = p;
    }
    return count;
}

size_t mbsrtowcs(wchar_t *restrict dst, const char **restrict src, size_t len,
                 mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_mbsrtowcs_state;
    const char *p = *src;
    size_t count = 0u;

    while (!dst || count < len) {
        wchar_t wc = 0;
        size_t rc = mbrtowc(&wc, p, 4u, st);
        if (rc == (size_t)-1) {
            if (dst) {
                *src = p;
            }
            return (size_t)-1;
        }
        if (rc == (size_t)-2) {
            errno = EILSEQ;
            if (dst) {
                *src = p;
            }
            return (size_t)-1;
        }
        if (rc == 0u) {
            if (dst) {
                dst[count] = L'\0';
                *src = NULL;
            }
            return count;
        }
        if (dst) {
            dst[count] = wc;
        }
        count++;
        p += rc;
    }
    if (dst) {
        *src = p;
    }
    return count;
}

size_t wcsnrtombs(char *restrict dst, const wchar_t **restrict src, size_t nwc,
                  size_t len, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_wcsrtombs_state;
    const wchar_t *p = *src;
    size_t count = 0u;
    size_t seen = 0u;

    while (seen < nwc) {
        char tmp[4];
        wchar_t wc = *p;
        size_t nb = wcrtomb(tmp, wc, st);
        if (nb == (size_t)-1) {
            if (dst) {
                *src = p;
            }
            return (size_t)-1;
        }
        if (wc == L'\0') {
            if (dst) {
                if (count < len) {
                    dst[count] = '\0';
                    *src = NULL;
                } else {
                    *src = p;
                }
            }
            return count;
        }
        if (dst && count + nb > len) {
            *src = p;
            return count;
        }
        if (dst) {
            memcpy(dst + count, tmp, nb);
        }
        count += nb;
        p++;
        seen++;
    }
    if (dst) {
        *src = p;
    }
    return count;
}

size_t wcsrtombs(char *restrict dst, const wchar_t **restrict src, size_t len,
                 mbstate_t *restrict ps) {
    return wcsnrtombs(dst, src, (size_t)-1, len, ps);
}

int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n) {
    if (!s) {
        return 0;
    }
    if (n == 0u) {
        errno = EILSEQ;
        return -1;
    }
    mbstate_t st = {0};
    size_t rc = mbrtowc(pwc, s, n, &st);
    if (rc == (size_t)-1 || rc == (size_t)-2) {
        errno = EILSEQ;
        return -1;
    }
    return (int)rc;
}

int mblen(const char *s, size_t n) {
    if (!s) {
        return 0;
    }
    return mbtowc(NULL, s, n);
}

int wctomb(char *s, wchar_t wchar) {
    if (!s) {
        return 0;
    }
    mbstate_t st = {0};
    size_t rc = wcrtomb(s, wchar, &st);
    if (rc == (size_t)-1) {
        return -1;
    }
    return (int)rc;
}

size_t mbstowcs(wchar_t *restrict pwcs, const char *restrict s, size_t n) {
    mbstate_t st = {0};
    const char *p = s;
    return mbsrtowcs(pwcs, &p, n, &st);
}

size_t wcstombs(char *restrict s, const wchar_t *restrict pwcs, size_t n) {
    mbstate_t st = {0};
    const wchar_t *p = pwcs;
    return wcsrtombs(s, &p, n, &st);
}
