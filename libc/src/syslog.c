/**
 * @file     syslog.c
 * @brief    POSIX.1-2024 XSI system logging (openlog, syslog, closelog)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/microkernel.h>
#include <syslog.h>
#include <unistd.h>

/* need:: COMP_LIBC_0028 */

static char g_log_ident[64];
static int g_log_opt;
static int g_log_facility = LOG_USER;
static int g_log_mask = 0xFF;

void openlog(const char *ident, int logopt, int facility) {
    if (ident) {
        (void)strlcpy(g_log_ident, ident, sizeof(g_log_ident));
    } else {
        g_log_ident[0] = '\0';
    }
    g_log_opt = logopt;
    if (facility != 0) {
        g_log_facility = facility & LOG_FACMASK;
    }
}

void closelog(void) {
    g_log_ident[0] = '\0';
    g_log_opt = 0;
    g_log_facility = LOG_USER;
}

int setlogmask(int maskpri) {
    int prev = g_log_mask;
    if (maskpri != 0) {
        g_log_mask = maskpri;
    }
    return prev;
}

void vsyslog(int priority, const char *message, va_list ap) {
    if (!message) {
        return;
    }
    int pri = LOG_PRI(priority);
    if ((g_log_mask & LOG_MASK(pri)) == 0) {
        return;
    }
    int saved_errno = errno;
    char body[384];
    (void)vsnprintf(body, sizeof(body), message, ap);

    char line[512];
    const char *id = g_log_ident[0] ? g_log_ident : "syslog";
    int n = 0;
    if (g_log_opt & LOG_PID) {
        n = snprintf(line, sizeof(line), "[syslog] %s[%d]: %s\n", id,
                     (int)getpid(), body);
    } else {
        n = snprintf(line, sizeof(line), "[syslog] %s: %s\n", id, body);
    }
    if (n <= 0) {
        errno = saved_errno;
        return;
    }
    size_t len = (size_t)n;
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1u;
    }

    int fd =
        open("/tmp/syslog", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644u);
    if (fd >= 0) {
        (void)write(fd, line, len);
        (void)close(fd);
    }
    if (g_log_opt & LOG_PERROR) {
        (void)write(STDERR_FILENO, line, len);
    } else if ((g_log_opt & LOG_CONS) && fd < 0) {
        (void)__sys2(SYS_DEBUG_LOG, (long)(uintptr_t)line, (long)len);
    }
    errno = saved_errno;
}

void syslog(int priority, const char *message, ...) {
    va_list ap;
    va_start(ap, message);
    vsyslog(priority, message, ap);
    va_end(ap);
}
