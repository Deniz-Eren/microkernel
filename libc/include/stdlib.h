/**
 * @file     stdlib.h
 * @brief    Freestanding Ring 3 process exit and heap declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STDLIB_H
#define _STDLIB_H

/* need:: ICD_LIBC_0001 */

#include <stddef.h>

_Noreturn void exit(int status);
_Noreturn void _Exit(int status);
_Noreturn void abort(void);
void *malloc(size_t n);
void free(void *p);
void *calloc(size_t a, size_t b);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nelem, size_t elsize);
char *getenv(const char *name);
char *secure_getenv(const char *name);
long strtol(const char *s, char **end, int base);
int atoi(const char *s);

#endif /* _STDLIB_H */
