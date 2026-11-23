/**
 * @file     logger.c
 * @brief    Ring 3 POSIX.1-2024 XCU logger utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

#define SYSLOG_PATH "/tmp/syslog"

static void write_log_line(const char *tag, const char *pri, int log_pid,
                           int to_stderr, const char *msg) {
    char ts[32];
    time_t now = time(NULL);
    struct tm tm_buf;
    if (gmtime_r(&now, &tm_buf)) {
        (void)strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    } else {
        (void)snprintf(ts, sizeof(ts), "1970-01-01T00:00:00Z");
    }

    char host[64];
    if (gethostname(host, sizeof(host)) != 0 || host[0] == '\0') {
        (void)snprintf(host, sizeof(host), "microkernel");
    }
    host[sizeof(host) - 1] = '\0';

    char line[1024];
    if (log_pid) {
        (void)snprintf(line, sizeof(line), "%s %s <%s> %s[%ld]: %s\n", ts, host,
                       pri, tag, (long)getpid(), msg);
    } else {
        (void)snprintf(line, sizeof(line), "%s %s <%s> %s: %s\n", ts, host, pri,
                       tag, msg);
    }

    FILE *fp = fopen(SYSLOG_PATH, "ab");
    if (fp) {
        (void)fputs(line, fp);
        (void)fclose(fp);
    }
    if (to_stderr) {
        (void)fputs(line, stderr);
    }
}

int main(int argc, char **argv) {
    const char *tag = getlogin();
    if (!tag || tag[0] == '\0') {
        tag = "root";
    }
    const char *pri = "user.notice";
    int log_pid = 0;
    int to_stderr = 0;
    int opt = 0;

    while ((opt = getopt(argc, argv, "ip:st:")) != -1) {
        switch (opt) {
        case 'i':
            log_pid = 1;
            break;
        case 'p':
            pri = optarg;
            break;
        case 's':
            to_stderr = 1;
            break;
        case 't':
            tag = optarg;
            break;
        default:
            (void)fprintf(stderr,
                          "usage: logger [-is] [-p pri] [-t tag] [message...]"
                          "\n");
            return 1;
        }
    }

    if (optind < argc) {
        char msg[768];
        size_t pos = 0;
        msg[0] = '\0';
        for (int i = optind; i < argc; i++) {
            int n = snprintf(msg + pos, sizeof(msg) - pos, "%s%s",
                             (i > optind) ? " " : "", argv[i]);
            if (n < 0 || (size_t)n >= sizeof(msg) - pos) {
                break;
            }
            pos += (size_t)n;
        }
        write_log_line(tag, pri, log_pid, to_stderr, msg);
        return 0;
    }

    char line[768];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        write_log_line(tag, pri, log_pid, to_stderr, line);
    }
    return 0;
}
