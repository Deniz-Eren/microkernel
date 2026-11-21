/**
 * @file     test_exechelper.c
 * @brief    Execve target binary verifying argv, envp, O_CLOEXEC, and pipe I/O
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: VER_PROC_0005 VER_IPC_0003 */

int main(int argc, char **argv, char **envp) {
    if (argc < 4) {
        printf("[FAIL] exechelper expected >=4 args, got %d\n", argc);
        return 1;
    }

    int write_fd = atoi(argv[1]);
    int cloexec_fd = atoi(argv[2]);
    const char *tag = argv[3];

    /* Verify envp propagation via environ / envp. */
    int env_ok = 0;
    for (int i = 0; envp && envp[i]; i++) {
        if (strcmp(envp[i], "TEST_ENV=posix2024") == 0) {
            env_ok = 1;
            break;
        }
    }
    if (!env_ok) {
        printf("[FAIL] exechelper missing TEST_ENV=posix2024 in envp\n");
        return 2;
    }

    /* Verify O_CLOEXEC descriptor was closed atomically by execve(). */
    errno = 0;
    if (fcntl(cloexec_fd, F_GETFD) != -1 || errno != EBADF) {
        printf("[FAIL] exechelper cloexec_fd=%d remained open (errno=%d)\n",
               cloexec_fd, errno);
        return 3;
    }

    /* Write verification message over the inherited pipe write end. */
    const char *msg = tag;
    size_t len = strlen(msg);
    ssize_t w = write(write_fd, msg, len);
    if (w != (ssize_t)len) {
        printf("[FAIL] exechelper write(%d) -> %ld (errno=%d)\n", write_fd,
               (long)w, errno);
        return 4;
    }
    return 77;
}
