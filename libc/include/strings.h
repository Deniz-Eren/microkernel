/**
 * @file     strings.h
 * @brief    POSIX.1-2024 Base case-insensitive string comparison declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STRINGS_H
#define _STRINGS_H

/* need:: ICD_LIBC_0005 ICD_LIBC_0012 */

#include <stddef.h>
#include <sys/_locale_t.h>

int strcasecmp(const char *s1, const char *s2);
int strcasecmp_l(const char *s1, const char *s2, locale_t locale);
int strncasecmp(const char *s1, const char *s2, size_t n);
int strncasecmp_l(const char *s1, const char *s2, size_t n, locale_t locale);

#endif /* _STRINGS_H */
