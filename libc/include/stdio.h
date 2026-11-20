// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#ifndef _STDIO_H
#define _STDIO_H

/* need:: ICD_LIBC_0001 */

#include <stddef.h>

int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int snprintf(char *buf, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int puts(const char *s);
int putchar(int c);

#endif /* _STDIO_H */
