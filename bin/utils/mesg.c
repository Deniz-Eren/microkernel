/**
 * @file     mesg.c
 * @brief    Ring 3 POSIX.1-2024 XCU mesg terminal message-permission utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

#define MESG_STATE_PATH "/tmp/.mesg"

static int read_state(void) {
    const char *tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = ttyname(STDOUT_FILENO);
    }
    if (!tty) {
        tty = ttyname(STDERR_FILENO);
    }
    if (tty) {
        struct stat st;
        if (stat(tty, &st) == 0) {
            return (st.st_mode & S_IWGRP) ? 1 : 0;
        }
    }
    FILE *fp = fopen(MESG_STATE_PATH, "rb");
    if (!fp) {
        return 1; /* default: messages permitted */
    }
    int c = fgetc(fp);
    (void)fclose(fp);
    return (c == 'n') ? 0 : 1;
}

static int write_state(int allow) {
    const char *tty = ttyname(STDIN_FILENO);
    if (!tty) {
        tty = ttyname(STDOUT_FILENO);
    }
    if (!tty) {
        tty = ttyname(STDERR_FILENO);
    }
    if (tty) {
        struct stat st;
        if (stat(tty, &st) == 0) {
            mode_t m = allow ? (st.st_mode | S_IWGRP)
                             : (st.st_mode & ~(mode_t)S_IWGRP);
            (void)chmod(tty, m & 07777u);
        }
    }
    FILE *fp = fopen(MESG_STATE_PATH, "wb");
    if (!fp) {
        return -1;
    }
    (void)fputc(allow ? 'y' : 'n', fp);
    (void)fputc('\n', fp);
    (void)fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    if (argi >= argc) {
        int allow = read_state();
        (void)puts(allow ? "is y" : "is n");
        return allow ? 0 : 1;
    }
    if (argc - argi != 1) {
        (void)fprintf(stderr, "usage: mesg [y|n]\n");
        return 2;
    }
    if (strcmp(argv[argi], "y") == 0) {
        if (write_state(1) != 0) {
            return 2;
        }
        return 0;
    }
    if (strcmp(argv[argi], "n") == 0) {
        if (write_state(0) != 0) {
            return 2;
        }
        return 1;
    }
    (void)fprintf(stderr, "usage: mesg [y|n]\n");
    return 2;
}
