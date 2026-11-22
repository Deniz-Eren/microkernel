/**
 * @file     nohup.c
 * @brief    POSIX.1-2024 XCU nohup utility (invoke a command immune to SIGHUP)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0008 */

int main(int argc, char **argv) {
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    if (argi >= argc) {
        (void)fprintf(stderr, "usage: nohup utility [argument...]\n");
        return 127;
    }

    struct sigaction sa = {0};
    (void)sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    (void)sigaction(SIGHUP, &sa, NULL);

    if (isatty(STDOUT_FILENO)) {
        const char *chosen = "nohup.out";
        char path[512];
        int fd = open(chosen, O_WRONLY | O_CREAT | O_APPEND, 0600u);
        if (fd < 0) {
            const char *home = getenv("HOME");
            if (home && *home &&
                strlen(home) + sizeof("/nohup.out") < sizeof(path)) {
                (void)strcpy(path, home);
                (void)strcat(path, "/nohup.out");
                chosen = path;
                fd = open(chosen, O_WRONLY | O_CREAT | O_APPEND, 0600u);
            }
            if (fd < 0) {
                perror("nohup: cannot open output file");
                return 127;
            }
        }
        (void)fprintf(stderr, "nohup: appending output to '%s'\n", chosen);
        if (fd != STDOUT_FILENO) {
            (void)dup2(fd, STDOUT_FILENO);
            (void)close(fd);
        }
    }
    if (isatty(STDERR_FILENO)) {
        (void)dup2(STDOUT_FILENO, STDERR_FILENO);
    }

    (void)execvp(argv[argi], &argv[argi]);
    int exec_err = errno;
    if (exec_err == ENOENT) {
        (void)fprintf(stderr, "nohup: %s: not found\n", argv[argi]);
        return 127;
    }
    (void)fprintf(stderr, "nohup: %s: cannot execute\n", argv[argi]);
    return 126;
}
