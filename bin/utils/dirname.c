/**
 * @file     dirname.c
 * @brief    POSIX.1-2024 XCU dirname utility (return directory portion)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* need:: COMP_IPC_0017 */

int main(int argc, char **argv) {
    int idx = 1;
    if (idx < argc && strcmp(argv[idx], "--") == 0) {
        idx++;
    }
    if (argc - idx != 1) {
        (void)fprintf(stderr, "usage: dirname string\n");
        return 1;
    }

    const char *src = argv[idx];
    if (src[0] == '\0') {
        (void)puts(".");
        return 0;
    }

    char buf[PATH_MAX];
    (void)strlcpy(buf, src, sizeof(buf));
    size_t len = strlen(buf);
    while (len > 1u && buf[len - 1u] == '/') {
        buf[--len] = '\0';
    }
    char *slash = strrchr(buf, '/');
    if (!slash) {
        (void)puts(".");
        return 0;
    }
    while (slash > buf && *slash == '/') {
        *slash-- = '\0';
    }
    if (slash == buf && buf[0] == '/') {
        buf[1] = '\0';
    }
    (void)puts(buf);
    return 0;
}
