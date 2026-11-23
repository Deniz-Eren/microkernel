/**
 * @file     monetary.h
 * @brief    POSIX.1-2024 monetary formatting APIs (strfmon and strfmon_l)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MONETARY_H
#define _MONETARY_H

/* need:: ICD_LIBC_0015 */

#include <stddef.h>
#include <sys/_locale_t.h>
#include <sys/types.h>

ssize_t strfmon(char *restrict s, size_t maxsize, const char *restrict format,
                ...);
ssize_t strfmon_l(char *restrict s, size_t maxsize, locale_t locale,
                  const char *restrict format, ...);

#endif /* _MONETARY_H */
