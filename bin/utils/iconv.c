/**
 * @file     iconv.c
 * @brief    Ring 3 iconv utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

static int convert_stream(iconv_t cd, FILE *fp, int silent) {
    char inbuf[4096];
    char outbuf[8192];
    size_t carry = 0u;

    for (;;) {
        size_t n = fread(inbuf + carry, 1u, sizeof(inbuf) - carry, fp);
        size_t total = carry + n;
        if (total == 0u) {
            break;
        }
        char *ip = inbuf;
        size_t inleft = total;
        while (inleft > 0u) {
            char *op = outbuf;
            size_t outleft = sizeof(outbuf);
            size_t rc = iconv(cd, &ip, &inleft, &op, &outleft);
            size_t produced = sizeof(outbuf) - outleft;
            if (produced > 0u &&
                fwrite(outbuf, 1u, produced, stdout) != produced) {
                if (!silent) {
                    perror("iconv");
                }
                return 1;
            }
            if (rc != (size_t)-1) {
                break;
            }
            if (errno == E2BIG) {
                continue;
            }
            if (errno == EINVAL) {
                if (n == 0u) {
                    if (!silent) {
                        fprintf(stderr, "iconv: incomplete sequence at EOF\n");
                    }
                    return 1;
                }
                break;
            }
            if (!silent) {
                perror("iconv");
            }
            return 1;
        }
        carry = inleft;
        if (carry > 0u) {
            memmove(inbuf, ip, carry);
        }
        if (n == 0u) {
            break;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)setlocale(LC_ALL, "");
    const char *fromcode = NULL;
    const char *tocode = NULL;
    int list_enc = 0;
    int omit_invalid = 0;
    int silent = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-f") == 0 && idx + 1 < argc) {
            fromcode = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-t") == 0 && idx + 1 < argc) {
            tocode = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-l") == 0) {
            list_enc = 1;
            idx++;
        } else if (strcmp(argv[idx], "-c") == 0) {
            omit_invalid = 1;
            idx++;
        } else if (strcmp(argv[idx], "-s") == 0) {
            silent = 1;
            idx++;
        } else {
            fprintf(stderr, "iconv: unknown option '%s'\n", argv[idx]);
            return 1;
        }
    }

    if (list_enc) {
        puts("US-ASCII ASCII");
        puts("ISO-8859-1 LATIN1");
        puts("UTF-8");
        puts("UTF-16 UTF-16LE UTF-16BE");
        puts("UTF-32 UTF-32LE UTF-32BE UCS-4 WCHAR_T");
        return 0;
    }

    const char *def_cs = nl_langinfo(CODESET);
    if (!fromcode) {
        fromcode = def_cs;
    }
    if (!tocode) {
        tocode = def_cs;
    }
    char to_buf[96];
    if (omit_invalid) {
        (void)snprintf(to_buf, sizeof(to_buf), "%s//IGNORE", tocode);
        tocode = to_buf;
    }

    iconv_t cd = iconv_open(tocode, fromcode);
    if (cd == (iconv_t)-1) {
        if (!silent) {
            fprintf(stderr, "iconv: conversion from '%s' to '%s' unsupported\n",
                    fromcode, tocode);
        }
        return 1;
    }

    int rc = 0;
    if (idx >= argc) {
        rc = convert_stream(cd, stdin, silent);
    } else {
        for (; idx < argc; idx++) {
            FILE *fp = stdin;
            if (strcmp(argv[idx], "-") != 0) {
                fp = fopen(argv[idx], "rb");
                if (!fp) {
                    if (!silent) {
                        perror(argv[idx]);
                    }
                    rc = 1;
                    continue;
                }
            }
            if (convert_stream(cd, fp, silent) != 0) {
                rc = 1;
            }
            if (fp != stdin) {
                (void)fclose(fp);
            }
        }
    }

    (void)iconv_close(cd);
    return rc;
}
