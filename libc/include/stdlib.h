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

/* need:: ICD_LIBC_0001 ICD_LIBC_0005 ICD_LIBC_0006 ICD_IPC_0016 */
/* need:: ICD_LIBC_0009 ICD_LIBC_0012 */

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

size_t __libc_mb_cur_max(void);
#define MB_CUR_MAX (__libc_mb_cur_max())

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

int atexit(void (*func)(void));
int at_quick_exit(void (*func)(void));
int system(const char *command);
_Noreturn void exit(int status);
_Noreturn void quick_exit(int status);
_Noreturn void _Exit(int status);
_Noreturn void abort(void);
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t a, size_t b);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nelem, size_t elsize);
void *aligned_alloc(size_t alignment, size_t size);
void qsort(void *base, size_t nel, size_t width,
           int (*compar)(const void *, const void *));
void qsort_r(void *base, size_t nel, size_t width,
             int (*compar)(const void *, const void *, void *), void *arg);
void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*compar)(const void *, const void *));
char *realpath(const char *restrict file_name, char *restrict resolved_name);
int mkstemp(char *template);
int mkostemp(char *template, int flags);
char *mkdtemp(char *template);
char *getenv(const char *name);
char *secure_getenv(const char *name);
int setenv(const char *envname, const char *envval, int overwrite);
int unsetenv(const char *name);
int getsubopt(char **optionp, char *const *keylistp, char **valuep);
double strtod(const char *restrict nptr, char **restrict endptr);
float strtof(const char *restrict nptr, char **restrict endptr);
long double strtold(const char *restrict nptr, char **restrict endptr);
double atof(const char *str);
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
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);
int rand(void);
void srand(unsigned int seed);
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n);
int wctomb(char *s, wchar_t wchar);
size_t mbstowcs(wchar_t *restrict pwcs, const char *restrict s, size_t n);
size_t wcstombs(char *restrict s, const wchar_t *restrict pwcs, size_t n);

#endif /* _STDLIB_H */
