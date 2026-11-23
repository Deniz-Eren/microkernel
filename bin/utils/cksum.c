/**
 * @file     cksum.c
 * @brief    Ring 3 cksum utility targeting POSIX.1-2024 XCU CRC-32
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* need:: COMP_LIBC_0020 */

static uint32_t g_crc_tab[256];

static void init_crc_table(void) {
    for (uint32_t i = 0u; i < 256u; i++) {
        uint32_t c = i << 24;
        for (int b = 0; b < 8; b++) {
            c = (c & 0x80000000u) ? ((c << 1) ^ 0x04C11DB7u) : (c << 1);
        }
        g_crc_tab[i] = c;
    }
}

static int cksum_stream(FILE *fp, const char *name) {
    uint32_t crc = 0u;
    uint64_t len = 0ULL;
    unsigned char buf[1024];
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), fp);
        for (size_t i = 0u; i < got; i++) {
            crc = (crc << 8) ^ g_crc_tab[((crc >> 24) ^ buf[i]) & 0xFFu];
        }
        len += got;
        if (got == 0u) {
            break;
        }
    }
    if (ferror(fp)) {
        return -1;
    }
    for (uint64_t rem = len; rem > 0ULL; rem >>= 8) {
        uint32_t b = (uint32_t)(rem & 0xFFu);
        crc = (crc << 8) ^ g_crc_tab[((crc >> 24) ^ b) & 0xFFu];
    }
    crc = ~crc;
    if (name) {
        (void)printf("%u %llu %s\n", crc, (unsigned long long)len, name);
    } else {
        (void)printf("%u %llu\n", crc, (unsigned long long)len);
    }
    return 0;
}

int main(int argc, char **argv) {
    init_crc_table();
    int idx = 1;
    if (idx < argc && strcmp(argv[idx], "--") == 0) {
        idx++;
    }
    if (idx >= argc) {
        return cksum_stream(stdin, NULL) == 0 ? 0 : 1;
    }
    int rc = 0;
    for (; idx < argc; idx++) {
        FILE *fp = fopen(argv[idx], "rb");
        if (!fp) {
            perror(argv[idx]);
            rc = 1;
            continue;
        }
        if (cksum_stream(fp, argv[idx]) != 0) {
            perror(argv[idx]);
            rc = 1;
        }
        (void)fclose(fp);
    }
    return rc;
}
