/**
 * @file     head.c
 * @brief    Ring 3 head utility targeting POSIX.1-2024 XCU (-n count)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* need:: COMP_PROC_0007 */

static void head_stream(FILE *fp, long limit) {
    long lines = 0;
    int ch;
    while (lines < limit && (ch = fgetc(fp)) != EOF) {
        (void)putchar(ch);
        if (ch == '\n') {
            lines++;
        }
    }
}

int main(int argc, char **argv) {
    long limit = 10;
    int opt;
    while ((opt = getopt(argc, argv, "n:")) != -1) {
        if (opt == 'n') {
            limit = strtol(optarg, NULL, 10);
            if (limit < 0) {
                (void)fprintf(stderr, "head: invalid line count\n");
                return 1;
            }
        } else {
            return 1;
        }
    }
    if (optind >= argc) {
        head_stream(stdin, limit);
        return 0;
    }
    int rc = 0;
    int multi = (argc - optind) > 1;
    for (int i = optind; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");
        if (!fp) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        if (multi) {
            (void)printf("%s==> %s <==\n", i > optind ? "\n" : "", argv[i]);
        }
        head_stream(fp, limit);
        (void)fclose(fp);
    }
    return rc;
}
