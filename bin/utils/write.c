/**
 * @file     write.c
 * @brief    Ring 3 POSIX.1-2024 XCU write inter-user terminal messaging utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

#define MESG_STATE_PATH "/tmp/.mesg"

static int mesg_allowed(void) {
    FILE *fp = fopen(MESG_STATE_PATH, "rb");
    if (!fp) {
        return 1;
    }
    int c = fgetc(fp);
    (void)fclose(fp);
    return (c == 'n') ? 0 : 1;
}

int main(int argc, char **argv) {
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "--") == 0) {
        argi++;
    }
    int rem = argc - argi;
    if (rem < 1 || rem > 2) {
        (void)fprintf(stderr, "usage: write user_name [terminal]\n");
        return 1;
    }

    const char *target_user = argv[argi];
    const char *target_term = (rem == 2) ? argv[argi + 1] : NULL;

    struct passwd *pw = getpwnam(target_user);
    if (!pw) {
        (void)fprintf(stderr, "write: unknown user: %s\n", target_user);
        return 1;
    }
    if (getuid() != 0 && !mesg_allowed()) {
        (void)fprintf(stderr, "write: %s has messages disabled\n", target_user);
        return 1;
    }

    char path[256];
    if (target_term) {
        if (target_term[0] == '/') {
            (void)snprintf(path, sizeof(path), "%s", target_term);
        } else {
            (void)snprintf(path, sizeof(path), "/dev/%s", target_term);
        }
    } else {
        (void)snprintf(path, sizeof(path), "/tmp/write.%s", target_user);
    }

    FILE *out = fopen(path, "ab");
    if (!out) {
        perror(path);
        return 1;
    }

    const char *sender = getlogin();
    if (!sender || sender[0] == '\0') {
        struct passwd *me = getpwuid(getuid());
        sender = (me && me->pw_name) ? me->pw_name : "root";
    }
    const char *mytty = ttyname(STDIN_FILENO);
    if (!mytty) {
        mytty = "tty";
    }
    char hhmm[16];
    time_t now = time(NULL);
    struct tm tm_buf;
    if (gmtime_r(&now, &tm_buf)) {
        (void)strftime(hhmm, sizeof(hhmm), "%H:%M", &tm_buf);
    } else {
        (void)snprintf(hhmm, sizeof(hhmm), "00:00");
    }

    (void)fprintf(out, "Message from %s on %s at %s ...\n", sender, mytty,
                  hhmm);
    (void)fflush(out);

    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        (void)fputs(line, out);
        (void)fflush(out);
    }
    (void)fputs("EOF\n", out);
    (void)fclose(out);
    return 0;
}
