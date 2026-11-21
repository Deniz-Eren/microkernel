/**
 * @file     cat.c
 * @brief    POSIX.1-2024 XCU cat utility concatenating files to stdout
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0005 */

static int copy_fd(int fd) {
    char buf[512];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) {
            return 0;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("cat");
            return 1;
        }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(STDOUT_FILENO, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                perror("cat");
                return 1;
            }
            off += w;
        }
    }
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        return copy_fd(STDIN_FILENO);
    }
    int status = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            if (copy_fd(STDIN_FILENO) != 0) {
                status = 1;
            }
            continue;
        }
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            perror(argv[i]);
            status = 1;
            continue;
        }
        if (copy_fd(fd) != 0) {
            status = 1;
        }
        (void)close(fd);
    }
    return status;
}
