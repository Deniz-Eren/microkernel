/**
 * @file     uuencode.c
 * @brief    Ring 3 POSIX.1-2024 XCU uuencode utility (historical and -m Base64)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

static char uu_char(unsigned int v) {
    v &= 0x3fu;
    return (v == 0u) ? '`' : (char)(v + 32u);
}

static int encode_uu(FILE *fp, unsigned int mode, const char *name) {
    (void)printf("begin %o %s\n", mode, name);
    unsigned char buf[45];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        (void)putchar(uu_char((unsigned int)n));
        for (size_t i = 0; i < n; i += 3) {
            unsigned int b0 = buf[i];
            unsigned int b1 = (i + 1 < n) ? buf[i + 1] : 0u;
            unsigned int b2 = (i + 2 < n) ? buf[i + 2] : 0u;
            (void)putchar(uu_char(b0 >> 2));
            (void)putchar(uu_char(((b0 & 0x03u) << 4) | (b1 >> 4)));
            (void)putchar(uu_char(((b1 & 0x0fu) << 2) | (b2 >> 6)));
            (void)putchar(uu_char(b2 & 0x3fu));
        }
        (void)putchar('\n');
    }
    (void)puts("`");
    (void)puts("end");
    return ferror(fp) ? 1 : 0;
}

static int encode_base64(FILE *fp, unsigned int mode, const char *name) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    (void)printf("begin-base64 %o %s\n", mode, name);
    unsigned char buf[57];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        for (size_t i = 0; i < n; i += 3) {
            unsigned int b0 = buf[i];
            int has1 = (i + 1 < n);
            int has2 = (i + 2 < n);
            unsigned int b1 = has1 ? buf[i + 1] : 0u;
            unsigned int b2 = has2 ? buf[i + 2] : 0u;
            (void)putchar(b64[b0 >> 2]);
            (void)putchar(b64[((b0 & 0x03u) << 4) | (b1 >> 4)]);
            (void)putchar(has1 ? b64[((b1 & 0x0fu) << 2) | (b2 >> 6)] : '=');
            (void)putchar(has2 ? b64[b2 & 0x3fu] : '=');
        }
        (void)putchar('\n');
    }
    (void)puts("====");
    return ferror(fp) ? 1 : 0;
}

int main(int argc, char **argv) {
    int base64 = 0;
    int opt = 0;
    while ((opt = getopt(argc, argv, "m")) != -1) {
        if (opt == 'm') {
            base64 = 1;
        } else {
            (void)fprintf(stderr,
                          "usage: uuencode [-m] [file] decode_pathname\n");
            return 1;
        }
    }

    int rem = argc - optind;
    if (rem < 1 || rem > 2) {
        (void)fprintf(stderr, "usage: uuencode [-m] [file] decode_pathname\n");
        return 1;
    }

    FILE *fp = stdin;
    unsigned int mode = 0644u;
    const char *decode_name = NULL;

    if (rem == 2) {
        const char *inpath = argv[optind];
        decode_name = argv[optind + 1];
        if (strcmp(inpath, "-") != 0) {
            fp = fopen(inpath, "rb");
            if (!fp) {
                perror(inpath);
                return 1;
            }
            struct stat st;
            if (stat(inpath, &st) == 0) {
                mode = (unsigned int)(st.st_mode & 0777u);
            }
        }
    } else {
        decode_name = argv[optind];
    }

    int rc = base64 ? encode_base64(fp, mode, decode_name)
                    : encode_uu(fp, mode, decode_name);
    if (fp != stdin) {
        (void)fclose(fp);
    }
    return rc;
}
