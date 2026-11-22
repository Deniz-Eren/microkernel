/**
 * @file     sleep.c
 * @brief    POSIX.1-2024 XCU sleep utility (suspend for a given duration)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0008 */

int main(int argc, char **argv) {
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    if (argi + 1 != argc) {
        (void)fprintf(stderr, "usage: sleep seconds\n");
        return 1;
    }
    char *end = NULL;
    errno = 0;
    long seconds = strtol(argv[argi], &end, 10);
    if (errno != 0 || end == argv[argi] || *end != '\0' || seconds < 0) {
        (void)fprintf(stderr, "sleep: invalid time: %s\n", argv[argi]);
        return 1;
    }
    struct timespec req = {.tv_sec = (time_t)seconds, .tv_nsec = 0};
    struct timespec rem = {0};
    while (nanosleep(&req, &rem) < 0 && errno == EINTR) {
        req = rem;
    }
    return 0;
}
