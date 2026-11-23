/**
 * @file     tee.c
 * @brief    Ring 3 tee utility targeting POSIX.1-2024 XCU (-a, -i)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

int main(int argc, char **argv) {
    int append = 0;
    int ignore_int = 0;
    int opt;
    while ((opt = getopt(argc, argv, "ai")) != -1) {
        if (opt == 'a') {
            append = 1;
        } else if (opt == 'i') {
            ignore_int = 1;
        } else {
            return 1;
        }
    }
    if (ignore_int) {
        (void)signal(SIGINT, SIG_IGN);
    }

    int nfiles = argc - optind;
    FILE **fps = NULL;
    if (nfiles > 0) {
        fps = (FILE **)calloc((size_t)nfiles, sizeof(FILE *));
        if (!fps) {
            return 1;
        }
    }

    int rc = 0;
    const char *mode = append ? "ab" : "wb";
    for (int i = 0; i < nfiles; i++) {
        fps[i] = fopen(argv[optind + i], mode);
        if (!fps[i]) {
            perror(argv[optind + i]);
            rc = 1;
        }
    }

    unsigned char buf[1024];
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), stdin);
        if (got == 0u) {
            break;
        }
        if (fwrite(buf, 1u, got, stdout) != got) {
            rc = 1;
        }
        (void)fflush(stdout);
        for (int i = 0; i < nfiles; i++) {
            if (fps[i]) {
                if (fwrite(buf, 1u, got, fps[i]) != got) {
                    rc = 1;
                }
                (void)fflush(fps[i]);
            }
        }
    }

    for (int i = 0; i < nfiles; i++) {
        if (fps[i] && fclose(fps[i]) != 0) {
            rc = 1;
        }
    }
    free(fps);
    return rc;
}
