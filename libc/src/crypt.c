/**
 * @file     crypt.c
 * @brief    POSIX.1-2024 XSI salted password hashing (crypt) via SHA-256
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0028 */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    uint8_t buf[64];
    size_t len;
} sha256_ctx_t;

static const uint32_t k_sha256_k[64] = {
    0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu,
    0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u, 0xD807AA98u, 0x12835B01u,
    0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u,
    0xC19BF174u, 0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu,
    0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu, 0x983E5152u,
    0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u,
    0x06CA6351u, 0x14292967u, 0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu,
    0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
    0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u,
    0xD6990624u, 0xF40E3585u, 0x106AA070u, 0x19A4C116u, 0x1E376C08u,
    0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu,
    0x682E6FF3u, 0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u,
    0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u,
};

static uint32_t rotr32(uint32_t x, unsigned int n) {
    return (x >> n) | (x << (32u - n));
}

static void sha256_block(sha256_ctx_t *ctx, const uint8_t b[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)b[i * 4] << 24) | ((uint32_t)b[i * 4 + 1] << 16) |
               ((uint32_t)b[i * 4 + 2] << 8) | (uint32_t)b[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 =
            rotr32(w[i - 15], 7u) ^ rotr32(w[i - 15], 18u) ^ (w[i - 15] >> 3);
        uint32_t s1 =
            rotr32(w[i - 2], 17u) ^ rotr32(w[i - 2], 19u) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = ctx->h[0];
    uint32_t b0 = ctx->h[1];
    uint32_t c = ctx->h[2];
    uint32_t d = ctx->h[3];
    uint32_t e = ctx->h[4];
    uint32_t f = ctx->h[5];
    uint32_t g = ctx->h[6];
    uint32_t h = ctx->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + s1 + ch + k_sha256_k[i] + w[i];
        uint32_t s0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        uint32_t maj = (a & b0) ^ (a & c) ^ (b0 & c);
        uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b0;
        b0 = a;
        a = t1 + t2;
    }
    ctx->h[0] += a;
    ctx->h[1] += b0;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
    ctx->h[5] += f;
    ctx->h[6] += g;
    ctx->h[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->h[0] = 0x6A09E667u;
    ctx->h[1] = 0xBB67AE85u;
    ctx->h[2] = 0x3C6EF372u;
    ctx->h[3] = 0xA54FF53Au;
    ctx->h[4] = 0x510E527Fu;
    ctx->h[5] = 0x9B05688Cu;
    ctx->h[6] = 0x1F83D9ABu;
    ctx->h[7] = 0x5BE0CD19u;
    ctx->bits = 0ULL;
    ctx->len = 0u;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ctx->buf[ctx->len++] = data[i];
        ctx->bits += 8ULL;
        if (ctx->len == 64u) {
            sha256_block(ctx, ctx->buf);
            ctx->len = 0u;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t out[32]) {
    uint64_t total_bits = ctx->bits;
    ctx->buf[ctx->len++] = 0x80u;
    if (ctx->len > 56u) {
        while (ctx->len < 64u) {
            ctx->buf[ctx->len++] = 0u;
        }
        sha256_block(ctx, ctx->buf);
        ctx->len = 0u;
    }
    while (ctx->len < 56u) {
        ctx->buf[ctx->len++] = 0u;
    }
    for (int i = 0; i < 8; i++) {
        ctx->buf[56 + i] = (uint8_t)(total_bits >> (56 - i * 8));
    }
    sha256_block(ctx, ctx->buf);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(ctx->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)ctx->h[i];
    }
}

static const char k_b64[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void b64_encode_bytes(const uint8_t *in, size_t in_len, char *out,
                             size_t out_chars) {
    size_t pos = 0u;
    uint32_t acc = 0u;
    unsigned int bits = 0u;
    size_t idx = 0u;
    while (pos < out_chars) {
        while (bits < 6u) {
            uint8_t b = (idx < in_len) ? in[idx++] : 0u;
            acc |= (uint32_t)b << bits;
            bits += 8u;
        }
        out[pos++] = k_b64[acc & 0x3Fu];
        acc >>= 6u;
        bits -= 6u;
    }
    out[pos] = '\0';
}

char *crypt(const char *key, const char *salt) {
    static char g_crypt_out[96];
    if (!key || !salt || !salt[0]) {
        errno = EINVAL;
        return NULL;
    }

    int is_sha256 = (salt[0] == '$' && salt[1] == '5' && salt[2] == '$');
    char sbuf[17];
    size_t slen = 0u;
    if (is_sha256) {
        const char *p = salt + 3;
        while (*p && *p != '$' && slen < 16u) {
            sbuf[slen++] = *p++;
        }
    } else {
        while (salt[slen] && slen < 2u) {
            sbuf[slen] = salt[slen];
            slen++;
        }
    }
    sbuf[slen] = '\0';
    if (slen == 0u) {
        errno = EINVAL;
        return NULL;
    }

    uint8_t digest[32];
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)sbuf, slen);
    sha256_update(&ctx, (const uint8_t *)key, strlen(key));
    sha256_final(&ctx, digest);

    /* Iterate 1000 rounds for deterministic key stretching. */
    for (int r = 0; r < 1000; r++) {
        sha256_init(&ctx);
        if (r & 1) {
            sha256_update(&ctx, (const uint8_t *)key, strlen(key));
        } else {
            sha256_update(&ctx, digest, sizeof(digest));
        }
        if ((r % 3) != 0) {
            sha256_update(&ctx, (const uint8_t *)sbuf, slen);
        }
        if (r & 1) {
            sha256_update(&ctx, digest, sizeof(digest));
        } else {
            sha256_update(&ctx, (const uint8_t *)key, strlen(key));
        }
        sha256_final(&ctx, digest);
    }

    if (is_sha256) {
        char *dst = g_crypt_out;
        *dst++ = '$';
        *dst++ = '5';
        *dst++ = '$';
        memcpy(dst, sbuf, slen);
        dst += slen;
        *dst++ = '$';
        b64_encode_bytes(digest, sizeof(digest), dst, 43u);
    } else {
        memcpy(g_crypt_out, sbuf, slen);
        b64_encode_bytes(digest, sizeof(digest), g_crypt_out + slen, 11u);
    }
    return g_crypt_out;
}
