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

/* need:: ICD_LIBC_0005 */

#include <stddef.h>

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif /* _STRINGS_H */
