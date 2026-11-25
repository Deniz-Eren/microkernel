/**
 * @file     test_zlib.c
 * @brief    Ring 3 QEMU verification of sysroot zlib (libz.a, libz.so.1)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0030 VER_LIBC_0034 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

static int check_checksums(void) {
    static const unsigned char k_vec[] = "123456789";
    uLong crc = crc32(0UL, Z_NULL, 0u);
    crc = crc32(crc, k_vec, 9u);
    if (crc != 0xCBF43926UL) {
        printf("[FAIL] zlibtest crc32=0x%lx expected 0xcbf43926\n", crc);
        return -1;
    }
    uLong adler = adler32(0UL, Z_NULL, 0u);
    adler = adler32(adler, k_vec, 9u);
    if (adler != 0x091E01DEUL) {
        printf("[FAIL] zlibtest adler32=0x%lx expected 0x091e01de\n", adler);
        return -1;
    }
    return 0;
}

static int check_oneshot_compress(void) {
    unsigned char src[4096];
    for (size_t i = 0u; i < sizeof(src); i++) {
        src[i] = (unsigned char)('A' + (i % 26u));
    }
    uLong bound = compressBound((uLong)sizeof(src));
    unsigned char *comp = (unsigned char *)malloc(bound);
    unsigned char *out = (unsigned char *)malloc(sizeof(src));
    if (!comp || !out) {
        free(comp);
        free(out);
        printf("[FAIL] zlibtest malloc failed\n");
        return -1;
    }

    static const int k_levels[] = {Z_NO_COMPRESSION, Z_BEST_SPEED,
                                   Z_BEST_COMPRESSION};
    for (size_t li = 0u; li < sizeof(k_levels) / sizeof(k_levels[0]); li++) {
        uLong clen = bound;
        int rc = compress2(comp, &clen, (const Bytef *)src, (uLong)sizeof(src),
                           k_levels[li]);
        if (rc != Z_OK) {
            printf("[FAIL] zlibtest compress2(level=%d) -> %d\n", k_levels[li],
                   rc);
            free(comp);
            free(out);
            return -1;
        }
        if (k_levels[li] == Z_BEST_COMPRESSION && clen >= sizeof(src) / 10u) {
            printf("[FAIL] zlibtest poor ratio clen=%lu\n", clen);
            free(comp);
            free(out);
            return -1;
        }
        memset(out, 0, sizeof(src));
        uLong ulen = (uLong)sizeof(src);
        rc = uncompress(out, &ulen, comp, clen);
        if (rc != Z_OK || ulen != sizeof(src) ||
            memcmp(out, src, sizeof(src)) != 0) {
            printf("[FAIL] zlibtest uncompress(level=%d) -> %d len=%lu\n",
                   k_levels[li], rc, ulen);
            free(comp);
            free(out);
            return -1;
        }
    }

    /* Verify corruption detection returns Z_DATA_ERROR. */
    uLong clen = bound;
    if (compress2(comp, &clen, (const Bytef *)src, (uLong)sizeof(src),
                  Z_DEFAULT_COMPRESSION) != Z_OK ||
        clen < 8u) {
        free(comp);
        free(out);
        return -1;
    }
    comp[clen / 2u] ^= 0xFFu;
    uLong ulen = (uLong)sizeof(src);
    int bad_rc = uncompress(out, &ulen, comp, clen);
    free(comp);
    free(out);
    if (bad_rc != Z_DATA_ERROR) {
        printf("[FAIL] zlibtest expected Z_DATA_ERROR got %d\n", bad_rc);
        return -1;
    }
    return 0;
}

static int check_streaming(int window_bits) {
    unsigned char src[2048];
    for (size_t i = 0u; i < sizeof(src); i++) {
        src[i] = (unsigned char)((i * 7u + (i / 64u)) & 0xFFu);
    }

    unsigned char comp[4096];
    unsigned char out[2048];
    z_stream def;
    memset(&def, 0, sizeof(def));
    if (deflateInit2(&def, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        printf("[FAIL] zlibtest deflateInit2(%d) failed\n", window_bits);
        return -1;
    }

    size_t in_off = 0u;
    size_t comp_len = 0u;
    int rc = Z_OK;
    while (rc != Z_STREAM_END) {
        size_t chunk = sizeof(src) - in_off;
        if (chunk > 97u) {
            chunk = 97u;
        }
        int flush = (in_off + chunk == sizeof(src)) ? Z_FINISH : Z_NO_FLUSH;
        def.next_in = src + in_off;
        def.avail_in = (uInt)chunk;
        in_off += chunk;
        do {
            def.next_out = comp + comp_len;
            def.avail_out = (uInt)(sizeof(comp) - comp_len);
            rc = deflate(&def, flush);
            if (rc != Z_OK && rc != Z_STREAM_END) {
                (void)deflateEnd(&def);
                printf("[FAIL] zlibtest deflate(%d) -> %d\n", window_bits, rc);
                return -1;
            }
            comp_len = sizeof(comp) - (size_t)def.avail_out;
        } while (def.avail_out == 0u && rc != Z_STREAM_END);
    }
    (void)deflateEnd(&def);

    z_stream inf;
    memset(&inf, 0, sizeof(inf));
    if (inflateInit2(&inf, window_bits) != Z_OK) {
        printf("[FAIL] zlibtest inflateInit2(%d) failed\n", window_bits);
        return -1;
    }
    size_t cin_off = 0u;
    size_t out_len = 0u;
    rc = Z_OK;
    while (rc != Z_STREAM_END) {
        size_t chunk = comp_len - cin_off;
        if (chunk > 61u) {
            chunk = 61u;
        }
        inf.next_in = comp + cin_off;
        inf.avail_in = (uInt)chunk;
        cin_off += chunk;
        do {
            inf.next_out = out + out_len;
            inf.avail_out = (uInt)(sizeof(out) - out_len);
            rc = inflate(&inf, Z_NO_FLUSH);
            if (rc != Z_OK && rc != Z_STREAM_END) {
                (void)inflateEnd(&inf);
                printf("[FAIL] zlibtest inflate(%d) -> %d\n", window_bits, rc);
                return -1;
            }
            out_len = sizeof(out) - (size_t)inf.avail_out;
        } while (inf.avail_out == 0u && rc != Z_STREAM_END);
    }
    (void)inflateEnd(&inf);

    if (out_len != sizeof(src) || memcmp(out, src, sizeof(src)) != 0) {
        printf("[FAIL] zlibtest stream mismatch(%d) out_len=%zu\n", window_bits,
               out_len);
        return -1;
    }
    return 0;
}

static int check_gzfile(void) {
    static const char k_path[] = "/tmp/zlibtest.gz";
    static const char k_msg[] =
        "Microkernel Ring 3 zlib gzopen/gzwrite/gzread verification payload\n";

    gzFile gw = gzopen(k_path, "wb6");
    if (!gw) {
        printf("[FAIL] zlibtest gzopen(wb6) failed\n");
        return -1;
    }
    int nw = gzwrite(gw, k_msg, (unsigned int)strlen(k_msg));
    if (nw != (int)strlen(k_msg) || gzclose(gw) != Z_OK) {
        printf("[FAIL] zlibtest gzwrite/gzclose failed nw=%d\n", nw);
        (void)unlink(k_path);
        return -1;
    }

    gzFile gr = gzopen(k_path, "rb");
    if (!gr) {
        printf("[FAIL] zlibtest gzopen(rb) failed\n");
        (void)unlink(k_path);
        return -1;
    }
    char buf[128];
    memset(buf, 0, sizeof(buf));
    int nr = gzread(gr, buf, (unsigned int)(sizeof(buf) - 1u));
    int cr = gzclose(gr);
    (void)unlink(k_path);
    if (nr != (int)strlen(k_msg) || cr != Z_OK || strcmp(buf, k_msg) != 0) {
        printf("[FAIL] zlibtest gzread mismatch nr=%d cr=%d\n", nr, cr);
        return -1;
    }
    return 0;
}

int main(void) {
    const char *ver = zlibVersion();
    if (!ver || strcmp(ver, ZLIB_VERSION) != 0) {
        printf("[FAIL] zlibtest version mismatch: %s\n", ver ? ver : "(null)");
        return 1;
    }
    if (check_checksums() != 0) {
        return 1;
    }
    if (check_oneshot_compress() != 0) {
        return 1;
    }
    if (check_streaming(15) != 0 || check_streaming(15 + 16) != 0) {
        return 1;
    }
    if (check_gzfile() != 0) {
        return 1;
    }
    printf("[OK] zlib %s sysroot libz.so.1 dynamic compress, deflate/inflate, "
           "gzip, and crc32/adler32 verified\n",
           ver);
    return 0;
}
