/**
 * @file     uchar.c
 * @brief    ISO C17 and POSIX.1-2024 UTF-8/UTF-16/UTF-32 conversion routines
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <string.h>
#include <uchar.h>

/* need:: COMP_LIBC_0003 */

static mbstate_t g_c16_in_state;
static mbstate_t g_c16_out_state;
static mbstate_t g_c32_in_state;
static mbstate_t g_c32_out_state;

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

static size_t decode_utf8(uint32_t *out_cp, const char *s, size_t n,
                          mbstate_t *ps) {
    if (!s) {
        memset(ps, 0, sizeof(*ps));
        return 0u;
    }
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

static size_t encode_utf8(char *s, uint32_t cp) {
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

size_t mbrtoc32(char32_t *restrict pc32, const char *restrict s, size_t n,
                mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_c32_in_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 0u;
    }
    uint32_t cp = 0u;
    size_t rc = decode_utf8(&cp, s, n, st);
    if (rc != (size_t)-1 && rc != (size_t)-2 && pc32) {
        *pc32 = (char32_t)cp;
    }
    return rc;
}

size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_c32_out_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 1u;
    }
    return encode_utf8(s, (uint32_t)c32);
}

size_t mbrtoc16(char16_t *restrict pc16, const char *restrict s, size_t n,
                mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_c16_in_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 0u;
    }
    if (st->__pending_low != 0u) {
        if (pc16) {
            *pc16 = (char16_t)st->__pending_low;
        }
        st->__pending_low = 0u;
        return (size_t)-3;
    }
    uint32_t cp = 0u;
    size_t rc = decode_utf8(&cp, s, n, st);
    if (rc == (size_t)-1 || rc == (size_t)-2) {
        return rc;
    }
    if (cp <= 0xFFFFu) {
        if (pc16) {
            *pc16 = (char16_t)cp;
        }
        return rc;
    }
    uint32_t v = cp - 0x10000u;
    if (pc16) {
        *pc16 = (char16_t)(0xD800u | (v >> 10));
    }
    st->__pending_low = (uint16_t)(0xDC00u | (v & 0x3FFu));
    return rc;
}

size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict ps) {
    mbstate_t *st = ps ? ps : &g_c16_out_state;
    if (!s) {
        memset(st, 0, sizeof(*st));
        return 1u;
    }
    uint16_t u = (uint16_t)c16;
    if (st->__codepoint != 0u) {
        if (u < 0xDC00u || u > 0xDFFFu) {
            memset(st, 0, sizeof(*st));
            errno = EILSEQ;
            return (size_t)-1;
        }
        uint32_t hi = st->__codepoint - 0xD800u;
        uint32_t lo = (uint32_t)u - 0xDC00u;
        st->__codepoint = 0u;
        return encode_utf8(s, 0x10000u + (hi << 10) + lo);
    }
    if (u >= 0xD800u && u <= 0xDBFFu) {
        st->__codepoint = u;
        return 0u;
    }
    if (u >= 0xDC00u && u <= 0xDFFFu) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    return encode_utf8(s, (uint32_t)u);
}
