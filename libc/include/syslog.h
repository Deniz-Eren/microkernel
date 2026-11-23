/**
 * @file     syslog.h
 * @brief    POSIX.1-2024 XSI system logging constants and functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYSLOG_H
#define _SYSLOG_H

/* need:: ICD_LIBC_0018 */

#include <stdarg.h>

/* Priority levels. */
#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#define LOG_PRIMASK 0x07
#define LOG_PRI(p) ((p)&LOG_PRIMASK)
#define LOG_MASK(pri) (1 << (pri))
#define LOG_UPTO(pri) ((1 << ((pri) + 1)) - 1)

/* Facility codes. */
#define LOG_KERN (0 << 3)
#define LOG_USER (1 << 3)
#define LOG_MAIL (2 << 3)
#define LOG_DAEMON (3 << 3)
#define LOG_AUTH (4 << 3)
#define LOG_SYSLOG (5 << 3)
#define LOG_LPR (6 << 3)
#define LOG_NEWS (7 << 3)
#define LOG_UUCP (8 << 3)
#define LOG_CRON (9 << 3)
#define LOG_AUTHPRIV (10 << 3)
#define LOG_LOCAL0 (16 << 3)
#define LOG_LOCAL1 (17 << 3)
#define LOG_LOCAL2 (18 << 3)
#define LOG_LOCAL3 (19 << 3)
#define LOG_LOCAL4 (20 << 3)
#define LOG_LOCAL5 (21 << 3)
#define LOG_LOCAL6 (22 << 3)
#define LOG_LOCAL7 (23 << 3)

#define LOG_FACMASK 0x03F8
#define LOG_FAC(p) (((p)&LOG_FACMASK) >> 3)

/* Option flags for openlog(). */
#define LOG_PID 0x01
#define LOG_CONS 0x02
#define LOG_ODELAY 0x04
#define LOG_NDELAY 0x08
#define LOG_NOWAIT 0x10
#define LOG_PERROR 0x20

void closelog(void);
void openlog(const char *ident, int logopt, int facility);
int setlogmask(int maskpri);
void syslog(int priority, const char *message, ...);

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ||                  \
    defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_GNU_SOURCE)
void vsyslog(int priority, const char *message, va_list ap);
#endif

#endif /* _SYSLOG_H */
