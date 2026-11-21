/**
 * @file     stdio.h
 * @brief    POSIX.1-2024 freestanding Ring 3 formatted I/O and stream locking
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STDIO_H
#define _STDIO_H

/* need:: ICD_LIBC_0001 */

#include <stddef.h>

#define EOF (-1)

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __microkernel_file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *restrict fmt, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *restrict stream, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...)
    __attribute__((format(printf, 3, 4)));
int puts(const char *s);
int fputs(const char *restrict s, FILE *restrict stream);
int putchar(int c);
int putchar_unlocked(int c);
int fputc(int c, FILE *stream);
int fflush(FILE *stream);

void flockfile(FILE *file);
int ftrylockfile(FILE *file);
void funlockfile(FILE *file);

#endif /* _STDIO_H */
