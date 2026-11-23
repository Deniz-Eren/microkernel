/**
 * @file     string.h
 * @brief    POSIX.1-2024 memory, string, tokenization, and strerror APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STRING_H
#define _STRING_H

/* need:: ICD_LIBC_0001 ICD_LIBC_0005 ICD_LIBC_0009 ICD_LIBC_0012 */

#include <stddef.h>
#include <sys/_locale_t.h>

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
void *memmem(const void *haystack, size_t haystacklen, const void *needle,
             size_t needlelen);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strcoll(const char *s1, const char *s2);
int strcoll_l(const char *s1, const char *s2, locale_t locale);
size_t strxfrm(char *restrict s1, const char *restrict s2, size_t n);
size_t strxfrm_l(char *restrict s1, const char *restrict s2, size_t n,
                 locale_t locale);
char *strcpy(char *restrict dst, const char *restrict src);
char *strncpy(char *restrict dst, const char *restrict src, size_t n);
char *stpcpy(char *restrict dst, const char *restrict src);
char *stpncpy(char *restrict dst, const char *restrict src, size_t n);
char *strcat(char *restrict s1, const char *restrict s2);
char *strncat(char *restrict s1, const char *restrict s2, size_t n);
size_t strlcpy(char *restrict dst, const char *restrict src, size_t dstsize);
size_t strlcat(char *restrict dst, const char *restrict src, size_t dstsize);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strtok(char *restrict s, const char *restrict sep);
char *strtok_r(char *restrict s, const char *restrict sep,
               char **restrict state);
char *strdup(const char *s);
char *strndup(const char *s, size_t size);
char *strerror(int errnum);
char *strerror_l(int errnum, locale_t locale);
int strerror_r(int errnum, char *strerrbuf, size_t buflen);
char *strsignal(int sig);

#endif /* _STRING_H */
