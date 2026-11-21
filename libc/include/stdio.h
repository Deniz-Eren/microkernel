/**
 * @file     stdio.h
 * @brief    POSIX.1-2024 buffered Ring 3 standard I/O and stream locking
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STDIO_H
#define _STDIO_H

/* need:: ICD_LIBC_0001 ICD_LIBC_0005 */

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

#define EOF (-1)
#define BUFSIZ 512
#define FILENAME_MAX 256
#define FOPEN_MAX 32

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __microkernel_file FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *restrict pathname, const char *restrict mode);
FILE *fdopen(int fildes, const char *mode);
int fclose(FILE *stream);

size_t fread(void *restrict ptr, size_t size, size_t nitems,
             FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems,
              FILE *restrict stream);
char *fgets(char *restrict s, int n, FILE *restrict stream);
int fgetc(FILE *stream);
int getc(FILE *stream);
int getchar(void);
int getc_unlocked(FILE *stream);
int getchar_unlocked(void);
int ungetc(int c, FILE *stream);

int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);
int putc_unlocked(int c, FILE *stream);
int putchar(int c);
int putchar_unlocked(int c);
int fputs(const char *restrict s, FILE *restrict stream);
int puts(const char *s);

int fseek(FILE *stream, long offset, int whence);
int fseeko(FILE *stream, off_t offset, int whence);
long ftell(FILE *stream);
off_t ftello(FILE *stream);
void rewind(FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
int fflush(FILE *stream);
int setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size);
void setbuf(FILE *restrict stream, char *restrict buf);

int vprintf(const char *restrict fmt, va_list ap);
int vfprintf(FILE *restrict stream, const char *restrict fmt, va_list ap);
int vsnprintf(char *restrict buf, size_t n, const char *restrict fmt,
              va_list ap);
int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap);
int printf(const char *restrict fmt, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *restrict stream, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...)
    __attribute__((format(printf, 3, 4)));
int sprintf(char *restrict buf, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
void perror(const char *s);

void flockfile(FILE *file);
int ftrylockfile(FILE *file);
void funlockfile(FILE *file);

#endif /* _STDIO_H */
