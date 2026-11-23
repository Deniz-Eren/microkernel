/**
 * @file     stdio_internal.h
 * @brief    Internal unlocked stdio helpers shared with scanf implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _LIBC_STDIO_INTERNAL_H
#define _LIBC_STDIO_INTERNAL_H

#include <stdio.h>
#include <wchar.h>

int __stdio_fgetc_unlocked(FILE *stream);
int __stdio_ungetc_unlocked(int c, FILE *stream);
wint_t __stdio_fgetwc_unlocked(FILE *stream);
wint_t __stdio_fputwc_unlocked(wchar_t wc, FILE *stream);
wint_t __stdio_ungetwc_unlocked(wint_t wc, FILE *stream);
void __stdio_unlock_cleanup(void *arg);

#endif /* _LIBC_STDIO_INTERNAL_H */
