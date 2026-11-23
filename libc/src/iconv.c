/**
 * @file     iconv.c
 * @brief    POSIX.1-2024 streaming character set conversion (iconv)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* need:: COMP_LIBC_0015 */

enum iconv_enc {
    ENC_ASCII = 1,
    ENC_LATIN1,
    ENC_UTF8,
    ENC_UTF16,
    ENC_UTF16LE,
    ENC_UTF16BE,
    ENC_UTF32,
    ENC_UTF32LE,
    ENC_UTF32BE,
};

struct __microkernel_iconv {
    enum iconv_enc from;
    enum iconv_enc to;
    int ignore;
    int translit;
    int from_le;
    int bom_seen;
    int bom_emitted;
};

static int strcasecmp_ascii(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        int ca = toupper((unsigned char)*a++);
        int cb = toupper((unsigned char)*b++);
        if (ca != cb) {
            return ca - cb;
        }
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

static int parse_encoding(const char *raw, enum iconv_enc *out_enc, int *ignore,
                          int *translit) {
    if (!raw || raw[0] == '\0') {
        return -1;
    }
    char name[64];
    size_t len = 0u;
    const char *p = raw;
    while (*p != '\0' && !(p[0] == '/' && p[1] == '/') &&
           len + 1u < sizeof(name)) {
        name[len++] = *p++;
    }
    name[len] = '\0';

    while (p[0] == '/' && p[1] == '/') {
        p += 2;
        const char *opt = p;
        while (*p != '\0' && !(p[0] == '/' && p[1] == '/')) {
            p++;
        }
        size_t olen = (size_t)(p - opt);
        if (olen == 6u && strncasecmp(opt, "IGNORE", 6u) == 0 && ignore) {
            *ignore = 1;
        } else if (olen == 8u && strncasecmp(opt, "TRANSLIT", 8u) == 0 &&
                   translit) {
            *translit = 1;
        }
    }

    if (strcasecmp_ascii(name, "ASCII") == 0 ||
        strcasecmp_ascii(name, "US-ASCII") == 0 ||
        strcasecmp_ascii(name, "ANSI_X3.4-1968") == 0 ||
        strcasecmp_ascii(name, "C") == 0 ||
        strcasecmp_ascii(name, "POSIX") == 0) {
        *out_enc = ENC_ASCII;
        return 0;
    }
    if (strcasecmp_ascii(name, "ISO-8859-1") == 0 ||
        strcasecmp_ascii(name, "ISO8859-1") == 0 ||
        strcasecmp_ascii(name, "ISO_8859-1") == 0 ||
        strcasecmp_ascii(name, "LATIN1") == 0 ||
        strcasecmp_ascii(name, "LATIN-1") == 0) {
        *out_enc = ENC_LATIN1;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-8") == 0 ||
        strcasecmp_ascii(name, "UTF8") == 0) {
        *out_enc = ENC_UTF8;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-16") == 0 ||
        strcasecmp_ascii(name, "UTF16") == 0) {
        *out_enc = ENC_UTF16;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-16LE") == 0 ||
        strcasecmp_ascii(name, "UTF16LE") == 0) {
        *out_enc = ENC_UTF16LE;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-16BE") == 0 ||
        strcasecmp_ascii(name, "UTF16BE") == 0) {
        *out_enc = ENC_UTF16BE;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-32") == 0 ||
        strcasecmp_ascii(name, "UTF32") == 0 ||
        strcasecmp_ascii(name, "UCS-4") == 0 ||
        strcasecmp_ascii(name, "UCS4") == 0) {
        *out_enc = ENC_UTF32;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-32LE") == 0 ||
        strcasecmp_ascii(name, "UTF32LE") == 0 ||
        strcasecmp_ascii(name, "UCS-4LE") == 0 ||
        strcasecmp_ascii(name, "WCHAR_T") == 0) {
        *out_enc = ENC_UTF32LE;
        return 0;
    }
    if (strcasecmp_ascii(name, "UTF-32BE") == 0 ||
        strcasecmp_ascii(name, "UTF32BE") == 0 ||
        strcasecmp_ascii(name, "UCS-4BE") == 0) {
        *out_enc = ENC_UTF32BE;
        return 0;
    }
    return -1;
}

iconv_t iconv_open(const char *tocode, const char *fromcode) {
    enum iconv_enc from = ENC_UTF8;
    enum iconv_enc to = ENC_UTF8;
    int ignore = 0;
    int translit = 0;
    if (parse_encoding(fromcode, &from, NULL, NULL) != 0 ||
        parse_encoding(tocode, &to, &ignore, &translit) != 0) {
        errno = EINVAL;
        return (iconv_t)-1;
    }
    struct __microkernel_iconv *ctx =
        (struct __microkernel_iconv *)calloc(1u, sizeof(*ctx));
    if (!ctx) {
        errno = ENOMEM;
        return (iconv_t)-1;
    }
    ctx->from = from;
    ctx->to = to;
    ctx->ignore = ignore;
    ctx->translit = translit;
    ctx->from_le = (from == ENC_UTF16LE || from == ENC_UTF32LE) ? 1 : 0;
    return (iconv_t)ctx;
}

int iconv_close(iconv_t cd) {
    if (cd == (iconv_t)-1 || cd == NULL) {
        errno = EBADF;
        return -1;
    }
    free(cd);
    return 0;
}

/* Returns consumed byte count (>0), 0 for EINVAL (incomplete), -1 for EILSEQ.
 */
static int decode_one(struct __microkernel_iconv *ctx, const uint8_t *in,
                      size_t inlen, uint32_t *out_cp) {
    if (inlen == 0u) {
        return 0;
    }
    switch (ctx->from) {
    case ENC_ASCII:
        if (in[0] > 0x7Fu) {
            return -1;
        }
        *out_cp = in[0];
        return 1;
    case ENC_LATIN1:
        *out_cp = in[0];
        return 1;
    case ENC_UTF8: {
        uint8_t b0 = in[0];
        if (b0 < 0x80u) {
            *out_cp = b0;
            return 1;
        }
        if ((b0 & 0xE0u) == 0xC0u) {
            if (inlen < 2u) {
                return 0;
            }
            if ((in[1] & 0xC0u) != 0x80u) {
                return -1;
            }
            uint32_t cp = ((uint32_t)(b0 & 0x1Fu) << 6) | (in[1] & 0x3Fu);
            if (cp < 0x80u) {
                return -1;
            }
            *out_cp = cp;
            return 2;
        }
        if ((b0 & 0xF0u) == 0xE0u) {
            if (inlen < 3u) {
                return 0;
            }
            if ((in[1] & 0xC0u) != 0x80u || (in[2] & 0xC0u) != 0x80u) {
                return -1;
            }
            uint32_t cp = ((uint32_t)(b0 & 0x0Fu) << 12) |
                          ((uint32_t)(in[1] & 0x3Fu) << 6) | (in[2] & 0x3Fu);
            if (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu)) {
                return -1;
            }
            *out_cp = cp;
            return 3;
        }
        if ((b0 & 0xF8u) == 0xF0u) {
            if (inlen < 4u) {
                return 0;
            }
            if ((in[1] & 0xC0u) != 0x80u || (in[2] & 0xC0u) != 0x80u ||
                (in[3] & 0xC0u) != 0x80u) {
                return -1;
            }
            uint32_t cp = ((uint32_t)(b0 & 0x07u) << 18) |
                          ((uint32_t)(in[1] & 0x3Fu) << 12) |
                          ((uint32_t)(in[2] & 0x3Fu) << 6) | (in[3] & 0x3Fu);
            if (cp < 0x10000u || cp > 0x10FFFFu) {
                return -1;
            }
            *out_cp = cp;
            return 4;
        }
        return -1;
    }
    case ENC_UTF16:
    case ENC_UTF16LE:
    case ENC_UTF16BE: {
        if (inlen < 2u) {
            return 0;
        }
        int le = ctx->from_le;
        uint16_t u1 = le ? (uint16_t)(in[0] | ((uint16_t)in[1] << 8))
                         : (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
        if (ctx->from == ENC_UTF16 && !ctx->bom_seen) {
            ctx->bom_seen = 1;
            if (in[0] == 0xFFu && in[1] == 0xFEu) {
                ctx->from_le = 1;
                *out_cp = 0xFEFFu;
                return 2;
            }
            if (in[0] == 0xFEu && in[1] == 0xFFu) {
                ctx->from_le = 0;
                *out_cp = 0xFEFFu;
                return 2;
            }
        }
        if (u1 >= 0xDC00u && u1 <= 0xDFFFu) {
            return -1;
        }
        if (u1 >= 0xD800u && u1 <= 0xDBFFu) {
            if (inlen < 4u) {
                return 0;
            }
            uint16_t u2 = le ? (uint16_t)(in[2] | ((uint16_t)in[3] << 8))
                             : (uint16_t)(((uint16_t)in[2] << 8) | in[3]);
            if (u2 < 0xDC00u || u2 > 0xDFFFu) {
                return -1;
            }
            *out_cp = 0x10000u + (((uint32_t)(u1 - 0xD800u) << 10) |
                                  (uint32_t)(u2 - 0xDC00u));
            return 4;
        }
        *out_cp = u1;
        return 2;
    }
    case ENC_UTF32:
    case ENC_UTF32LE:
    case ENC_UTF32BE: {
        if (inlen < 4u) {
            return 0;
        }
        if (ctx->from == ENC_UTF32 && !ctx->bom_seen) {
            ctx->bom_seen = 1;
            if (in[0] == 0xFFu && in[1] == 0xFEu && in[2] == 0u &&
                in[3] == 0u) {
                ctx->from_le = 1;
                *out_cp = 0xFEFFu;
                return 4;
            }
            if (in[0] == 0u && in[1] == 0u && in[2] == 0xFEu &&
                in[3] == 0xFFu) {
                ctx->from_le = 0;
                *out_cp = 0xFEFFu;
                return 4;
            }
        }
        uint32_t cp = ctx->from_le
                          ? ((uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                             ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24))
                          : (((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
                             ((uint32_t)in[2] << 8) | (uint32_t)in[3]);
        if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return -1;
        }
        *out_cp = cp;
        return 4;
    }
    }
    return -1;
}

/* Returns written byte count (>0), -2 for E2BIG, -1 for EILSEQ (unmappable). */
static int encode_one(struct __microkernel_iconv *ctx, uint32_t cp,
                      uint8_t *out, size_t outlen) {
    switch (ctx->to) {
    case ENC_ASCII:
        if (cp > 0x7Fu) {
            return -1;
        }
        if (outlen < 1u) {
            return -2;
        }
        out[0] = (uint8_t)cp;
        return 1;
    case ENC_LATIN1:
        if (cp > 0xFFu) {
            return -1;
        }
        if (outlen < 1u) {
            return -2;
        }
        out[0] = (uint8_t)cp;
        return 1;
    case ENC_UTF8:
        if (cp < 0x80u) {
            if (outlen < 1u) {
                return -2;
            }
            out[0] = (uint8_t)cp;
            return 1;
        }
        if (cp < 0x800u) {
            if (outlen < 2u) {
                return -2;
            }
            out[0] = (uint8_t)(0xC0u | (cp >> 6));
            out[1] = (uint8_t)(0x80u | (cp & 0x3Fu));
            return 2;
        }
        if (cp < 0x10000u) {
            if (outlen < 3u) {
                return -2;
            }
            out[0] = (uint8_t)(0xE0u | (cp >> 12));
            out[1] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
            out[2] = (uint8_t)(0x80u | (cp & 0x3Fu));
            return 3;
        }
        if (outlen < 4u) {
            return -2;
        }
        out[0] = (uint8_t)(0xF0u | (cp >> 18));
        out[1] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (uint8_t)(0x80u | (cp & 0x3Fu));
        return 4;
    case ENC_UTF16:
    case ENC_UTF16LE:
    case ENC_UTF16BE: {
        int le = (ctx->to == ENC_UTF16LE);
        if (cp < 0x10000u) {
            if (outlen < 2u) {
                return -2;
            }
            if (le) {
                out[0] = (uint8_t)(cp & 0xFFu);
                out[1] = (uint8_t)((cp >> 8) & 0xFFu);
            } else {
                out[0] = (uint8_t)((cp >> 8) & 0xFFu);
                out[1] = (uint8_t)(cp & 0xFFu);
            }
            return 2;
        }
        if (outlen < 4u) {
            return -2;
        }
        uint32_t v = cp - 0x10000u;
        uint16_t hi = (uint16_t)(0xD800u + (v >> 10));
        uint16_t lo = (uint16_t)(0xDC00u + (v & 0x3FFu));
        if (le) {
            out[0] = (uint8_t)(hi & 0xFFu);
            out[1] = (uint8_t)(hi >> 8);
            out[2] = (uint8_t)(lo & 0xFFu);
            out[3] = (uint8_t)(lo >> 8);
        } else {
            out[0] = (uint8_t)(hi >> 8);
            out[1] = (uint8_t)(hi & 0xFFu);
            out[2] = (uint8_t)(lo >> 8);
            out[3] = (uint8_t)(lo & 0xFFu);
        }
        return 4;
    }
    case ENC_UTF32:
    case ENC_UTF32LE:
    case ENC_UTF32BE: {
        if (outlen < 4u) {
            return -2;
        }
        if (ctx->to == ENC_UTF32LE) {
            out[0] = (uint8_t)(cp & 0xFFu);
            out[1] = (uint8_t)((cp >> 8) & 0xFFu);
            out[2] = (uint8_t)((cp >> 16) & 0xFFu);
            out[3] = (uint8_t)((cp >> 24) & 0xFFu);
        } else {
            out[0] = (uint8_t)((cp >> 24) & 0xFFu);
            out[1] = (uint8_t)((cp >> 16) & 0xFFu);
            out[2] = (uint8_t)((cp >> 8) & 0xFFu);
            out[3] = (uint8_t)(cp & 0xFFu);
        }
        return 4;
    }
    }
    return -1;
}

size_t iconv(iconv_t cd, char **restrict inbuf, size_t *restrict inbytesleft,
             char **restrict outbuf, size_t *restrict outbytesleft) {
    if (cd == (iconv_t)-1 || cd == NULL) {
        errno = EBADF;
        return (size_t)-1;
    }
    struct __microkernel_iconv *ctx = (struct __microkernel_iconv *)cd;
    if (!inbuf || !*inbuf) {
        ctx->bom_seen = 0;
        ctx->bom_emitted = 0;
        ctx->from_le =
            (ctx->from == ENC_UTF16LE || ctx->from == ENC_UTF32LE) ? 1 : 0;
        return 0u;
    }
    if (!inbytesleft || !outbuf || !*outbuf || !outbytesleft) {
        errno = EINVAL;
        return (size_t)-1;
    }

    size_t nonrev = 0u;
    while (*inbytesleft > 0u) {
        int was_bom = ctx->bom_seen;
        uint32_t cp = 0u;
        int d = decode_one(ctx, (const uint8_t *)*inbuf, *inbytesleft, &cp);
        if (d == 0) {
            errno = EINVAL;
            return (size_t)-1;
        }
        if (d < 0) {
            if (ctx->ignore) {
                (*inbuf)++;
                (*inbytesleft)--;
                nonrev++;
                continue;
            }
            errno = EILSEQ;
            return (size_t)-1;
        }
        /* Strip leading BOM when decoding generic UTF-16/UTF-32. */
        if (!was_bom && cp == 0xFEFFu &&
            (ctx->from == ENC_UTF16 || ctx->from == ENC_UTF32)) {
            *inbuf += d;
            *inbytesleft -= (size_t)d;
            continue;
        }
        if (!ctx->bom_emitted &&
            (ctx->to == ENC_UTF16 || ctx->to == ENC_UTF32)) {
            int bn =
                encode_one(ctx, 0xFEFFu, (uint8_t *)*outbuf, *outbytesleft);
            if (bn == -2) {
                errno = E2BIG;
                return (size_t)-1;
            }
            *outbuf += bn;
            *outbytesleft -= (size_t)bn;
            ctx->bom_emitted = 1;
        }
        int e = encode_one(ctx, cp, (uint8_t *)*outbuf, *outbytesleft);
        if (e == -2) {
            errno = E2BIG;
            return (size_t)-1;
        }
        if (e < 0) {
            if (ctx->ignore) {
                *inbuf += d;
                *inbytesleft -= (size_t)d;
                nonrev++;
                continue;
            }
            if (ctx->translit) {
                e = encode_one(ctx, '?', (uint8_t *)*outbuf, *outbytesleft);
                if (e == -2) {
                    errno = E2BIG;
                    return (size_t)-1;
                }
                nonrev++;
            } else {
                errno = EILSEQ;
                return (size_t)-1;
            }
        }
        *inbuf += d;
        *inbytesleft -= (size_t)d;
        *outbuf += e;
        *outbytesleft -= (size_t)e;
    }
    return nonrev;
}
