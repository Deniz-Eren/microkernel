/**
 * @file     main.c
 * @brief    CLI driver for Ring 3 POSIX.1-2024 XCU bc calculator
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "bc.h"

/* need:: COMP_LIBC_0025 */

static char *read_stream_all(FILE *fp) {
    size_t cap = 512;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    for (;;) {
        size_t nr = fread(buf + len, 1, cap - len - 1, fp);
        len += nr;
        if (nr == 0) {
            break;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    int mathlib = 0;
    int opt = 0;
    while ((opt = getopt(argc, argv, "l")) != -1) {
        if (opt == 'l') {
            mathlib = 1;
        } else {
            (void)fprintf(stderr, "usage: bc [-l] [file...]\n");
            return 1;
        }
    }

    bc_vm_t vm;
    bc_vm_init(&vm, mathlib);
    int rc = 0;

    for (int i = optind; i < argc && vm.flow != BC_FLOW_QUIT; i++) {
        FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "rb");
        if (!fp) {
            perror(argv[i]);
            rc = 1;
            continue;
        }
        char *src = read_stream_all(fp);
        if (fp != stdin) {
            (void)fclose(fp);
        }
        if (src) {
            if (bc_exec_source(&vm, src) != 0) {
                rc = 1;
            }
            free(src);
        }
    }

    if (optind >= argc || (vm.flow != BC_FLOW_QUIT && !feof(stdin))) {
        char *src = read_stream_all(stdin);
        if (src) {
            if (src[0] != '\0' && bc_exec_source(&vm, src) != 0) {
                rc = 1;
            }
            free(src);
        }
    }

    if (vm.err) {
        rc = 1;
    }
    bc_vm_free(&vm);
    return rc;
}
