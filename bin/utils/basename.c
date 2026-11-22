/**
 * @file     basename.c
 * @brief    POSIX.1-2024 XCU basename utility (return non-directory portion)
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
    int rem = argc - idx;
    if (rem < 1 || rem > 2) {
        (void)fprintf(stderr, "usage: basename string [suffix]\n");
        return 1;
    }

    const char *src = argv[idx];
    const char *suffix = (rem == 2) ? argv[idx + 1] : NULL;
    if (src[0] == '\0') {
        (void)puts("");
        return 0;
    }

    char buf[PATH_MAX];
    (void)strlcpy(buf, src, sizeof(buf));
    size_t len = strlen(buf);
    while (len > 1u && buf[len - 1u] == '/') {
        buf[--len] = '\0';
    }
    if (len == 1u && buf[0] == '/') {
        (void)puts("/");
        return 0;
    }

    char *slash = strrchr(buf, '/');
    char *base = slash ? (slash + 1) : buf;
    if (suffix && suffix[0] != '\0') {
        size_t blen = strlen(base);
        size_t slen = strlen(suffix);
        if (blen > slen && strcmp(base + blen - slen, suffix) == 0) {
            base[blen - slen] = '\0';
        }
    }
    (void)puts(base);
    return 0;
}
