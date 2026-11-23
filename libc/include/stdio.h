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

/* need:: ICD_LIBC_0001 ICD_LIBC_0005 ICD_LIBC_0006 ICD_PROC_0009 */
/* need:: ICD_LIBC_0009 */

#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>

#define EOF (-1)
#define BUFSIZ 512
#define FILENAME_MAX 256
#define FOPEN_MAX 32
#define L_ctermid 16
#define L_tmpnam 32
#define TMP_MAX 10000

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

typedef struct __microkernel_file FILE;

typedef struct {
    off_t __pos;
} fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *restrict pathname, const char *restrict mode);
FILE *freopen(const char *restrict pathname, const char *restrict mode,
              FILE *restrict stream);
FILE *fdopen(int fildes, const char *mode);
FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode);
FILE *open_memstream(char **bufp, size_t *sizep);
int fclose(FILE *stream);

size_t fread(void *restrict ptr, size_t size, size_t nitems,
             FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems,
              FILE *restrict stream);
char *fgets(char *restrict s, int n, FILE *restrict stream);
ssize_t getdelim(char **restrict lineptr, size_t *restrict n, int delimiter,
                 FILE *restrict stream);
ssize_t getline(char **restrict lineptr, size_t *restrict n,
                FILE *restrict stream);
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
int fgetpos(FILE *restrict stream, fpos_t *restrict pos);
int fsetpos(FILE *stream, const fpos_t *pos);
void rewind(FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
int fileno(FILE *stream);
FILE *popen(const char *restrict command, const char *restrict mode);
int pclose(FILE *stream);
int fflush(FILE *stream);
int setvbuf(FILE *restrict stream, char *restrict buf, int type, size_t size);
void setbuf(FILE *restrict stream, char *restrict buf);

int vprintf(const char *restrict fmt, va_list ap);
int vfprintf(FILE *restrict stream, const char *restrict fmt, va_list ap);
int vsnprintf(char *restrict buf, size_t n, const char *restrict fmt,
              va_list ap);
int vsprintf(char *restrict buf, const char *restrict fmt, va_list ap);
int vdprintf(int fildes, const char *restrict fmt, va_list ap);
int vasprintf(char **restrict ptr, const char *restrict fmt, va_list ap);
int printf(const char *restrict fmt, ...) __attribute__((format(printf, 1, 2)));
int fprintf(FILE *restrict stream, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
int snprintf(char *restrict buf, size_t n, const char *restrict fmt, ...)
    __attribute__((format(printf, 3, 4)));
int sprintf(char *restrict buf, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
int dprintf(int fildes, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));
int asprintf(char **restrict ptr, const char *restrict fmt, ...)
    __attribute__((format(printf, 2, 3)));

int vfscanf(FILE *restrict stream, const char *restrict fmt, va_list ap);
int vsscanf(const char *restrict s, const char *restrict fmt, va_list ap);
int vscanf(const char *restrict fmt, va_list ap);
int fscanf(FILE *restrict stream, const char *restrict fmt, ...)
    __attribute__((format(scanf, 2, 3)));
int sscanf(const char *restrict s, const char *restrict fmt, ...)
    __attribute__((format(scanf, 2, 3)));
int scanf(const char *restrict fmt, ...) __attribute__((format(scanf, 1, 2)));
void perror(const char *s);

int rename(const char *oldpath, const char *newpath);
int renameat(int oldfd, const char *oldpath, int newfd, const char *newpath);
int remove(const char *path);
FILE *tmpfile(void);
char *tmpnam(char *s);

void flockfile(FILE *file);
int ftrylockfile(FILE *file);
void funlockfile(FILE *file);

char *ctermid(char *s);

#endif /* _STDIO_H */
