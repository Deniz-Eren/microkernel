/**
 * @file     stdlib.h
 * @brief    POSIX.1-2024 process exit, environment, heap, and integer APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STDLIB_H
#define _STDLIB_H

/* need:: ICD_LIBC_0001 ICD_LIBC_0005 ICD_LIBC_0006 */

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int atexit(void (*func)(void));
_Noreturn void exit(int status);
_Noreturn void _Exit(int status);
_Noreturn void abort(void);
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t a, size_t b);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nelem, size_t elsize);
void qsort(void *base, size_t nel, size_t width,
           int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*compar)(const void *, const void *));
char *realpath(const char *restrict file_name, char *restrict resolved_name);
char *getenv(const char *name);
char *secure_getenv(const char *name);
int setenv(const char *envname, const char *envval, int overwrite);
int unsetenv(const char *name);
long strtol(const char *restrict str, char **restrict endptr, int base);
unsigned long strtoul(const char *restrict str, char **restrict endptr,
                      int base);
long long strtoll(const char *restrict str, char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict str, char **restrict endptr,
                            int base);
int atoi(const char *s);
long atol(const char *s);
long long atoll(const char *s);
int abs(int i);
long labs(long i);
long long llabs(long long i);

#endif /* _STDLIB_H */
