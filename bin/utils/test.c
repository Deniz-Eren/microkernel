/**
 * @file     test.c
 * @brief    POSIX.1-2024 XCU test / [ conditional expression evaluator
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

/* need:: COMP_LIBC_0005 */

static int eval_unary(const char *op, const char *arg) {
    if (strcmp(op, "-n") == 0) {
        return arg[0] != '\0' ? 0 : 1;
    }
    if (strcmp(op, "-z") == 0) {
        return arg[0] == '\0' ? 0 : 1;
    }
    struct stat st = {0};
    if (strcmp(op, "-e") == 0) {
        return stat(arg, &st) == 0 ? 0 : 1;
    }
    if (strcmp(op, "-f") == 0) {
        return (stat(arg, &st) == 0 && S_ISREG(st.st_mode)) ? 0 : 1;
    }
    if (strcmp(op, "-d") == 0) {
        return (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) ? 0 : 1;
    }
    if (strcmp(op, "-s") == 0) {
        return (stat(arg, &st) == 0 && st.st_size > 0) ? 0 : 1;
    }
    if (strcmp(op, "-r") == 0) {
        return access(arg, R_OK) == 0 ? 0 : 1;
    }
    if (strcmp(op, "-w") == 0) {
        return access(arg, W_OK) == 0 ? 0 : 1;
    }
    if (strcmp(op, "-x") == 0) {
        return access(arg, X_OK) == 0 ? 0 : 1;
    }
    return 2;
}

static int eval_binary(const char *a, const char *op, const char *b) {
    if (strcmp(op, "=") == 0) {
        return strcmp(a, b) == 0 ? 0 : 1;
    }
    if (strcmp(op, "!=") == 0) {
        return strcmp(a, b) != 0 ? 0 : 1;
    }
    char *ea = NULL;
    char *eb = NULL;
    long va = strtol(a, &ea, 10);
    long vb = strtol(b, &eb, 10);
    if (!ea || *ea != '\0' || !eb || *eb != '\0') {
        return 2;
    }
    if (strcmp(op, "-eq") == 0) {
        return va == vb ? 0 : 1;
    }
    if (strcmp(op, "-ne") == 0) {
        return va != vb ? 0 : 1;
    }
    if (strcmp(op, "-lt") == 0) {
        return va < vb ? 0 : 1;
    }
    if (strcmp(op, "-le") == 0) {
        return va <= vb ? 0 : 1;
    }
    if (strcmp(op, "-gt") == 0) {
        return va > vb ? 0 : 1;
    }
    if (strcmp(op, "-ge") == 0) {
        return va >= vb ? 0 : 1;
    }
    return 2;
}

static int eval_expr(int argc, char **argv) {
    if (argc == 0) {
        return 1;
    }
    if (strcmp(argv[0], "!") == 0) {
        if (argc == 1) {
            return 0;
        }
        int r = eval_expr(argc - 1, argv + 1);
        if (r == 2) {
            return 2;
        }
        return r == 0 ? 1 : 0;
    }
    if (argc == 1) {
        return argv[0][0] != '\0' ? 0 : 1;
    }
    if (argc == 2) {
        return eval_unary(argv[0], argv[1]);
    }
    if (argc == 3) {
        return eval_binary(argv[0], argv[1], argv[2]);
    }
    return 2;
}

int main(int argc, char **argv) {
    int n = argc - 1;
    char **args = argv + 1;
    const char *base = strrchr(argv[0], '/');
    base = base ? base + 1 : argv[0];
    if (strcmp(base, "[") == 0) {
        if (n < 1 || strcmp(args[n - 1], "]") != 0) {
            fprintf(stderr, "[: missing ']'\n");
            return 2;
        }
        n--;
    }
    return eval_expr(n, args);
}
