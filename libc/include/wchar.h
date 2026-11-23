/**
 * @file     wchar.h
 * @brief    POSIX.1-2024 wide string, memory, multibyte, and conversion APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _WCHAR_H
#define _WCHAR_H

/* need:: ICD_LIBC_0012 */

#include <stddef.h>
#define __need_wint_t
#include <stddef.h>
#include <sys/_locale_t.h>
#include <sys/_mbstate_t.h>

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

#ifndef WCHAR_MAX
#define WCHAR_MAX __WCHAR_MAX__
#endif
#ifndef WCHAR_MIN
#ifdef __WCHAR_UNSIGNED__
#define WCHAR_MIN 0u
#else
#define WCHAR_MIN (-__WCHAR_MAX__ - 1)
#endif
#endif

struct tm;

/* Single-byte and restartable multibyte conversions. */
wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t *ps);
size_t mbrlen(const char *restrict s, size_t n, mbstate_t *restrict ps);
size_t mbrtowc(wchar_t *restrict pwc, const char *restrict s, size_t n,
               mbstate_t *restrict ps);
size_t wcrtomb(char *restrict s, wchar_t wc, mbstate_t *restrict ps);
size_t mbsrtowcs(wchar_t *restrict dst, const char **restrict src, size_t len,
                 mbstate_t *restrict ps);
size_t mbsnrtowcs(wchar_t *restrict dst, const char **restrict src, size_t nms,
                  size_t len, mbstate_t *restrict ps);
size_t wcsrtombs(char *restrict dst, const wchar_t **restrict src, size_t len,
                 mbstate_t *restrict ps);
size_t wcsnrtombs(char *restrict dst, const wchar_t **restrict src, size_t nwc,
                  size_t len, mbstate_t *restrict ps);

/* Wide memory operations. */
wchar_t *wmemchr(const wchar_t *ws, wchar_t wc, size_t n);
int wmemcmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);
wchar_t *wmemcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n);
wchar_t *wmemmove(wchar_t *ws1, const wchar_t *ws2, size_t n);
wchar_t *wmemset(wchar_t *ws, wchar_t wc, size_t n);

/* Wide string operations. */
size_t wcslen(const wchar_t *ws);
size_t wcsnlen(const wchar_t *ws, size_t maxlen);
wchar_t *wcscpy(wchar_t *restrict ws1, const wchar_t *restrict ws2);
wchar_t *wcsncpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n);
wchar_t *wcpcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2);
wchar_t *wcpncpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n);
size_t wcslcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2,
               size_t dstsize);
wchar_t *wcscat(wchar_t *restrict ws1, const wchar_t *restrict ws2);
wchar_t *wcsncat(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n);
size_t wcslcat(wchar_t *restrict ws1, const wchar_t *restrict ws2,
               size_t dstsize);
int wcscmp(const wchar_t *ws1, const wchar_t *ws2);
int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);
int wcscasecmp(const wchar_t *ws1, const wchar_t *ws2);
int wcscasecmp_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale);
int wcsncasecmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);
int wcsncasecmp_l(const wchar_t *ws1, const wchar_t *ws2, size_t n,
                  locale_t locale);
int wcscoll(const wchar_t *ws1, const wchar_t *ws2);
int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale);
size_t wcsxfrm(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n);
size_t wcsxfrm_l(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n,
                 locale_t locale);
wchar_t *wcschr(const wchar_t *ws, wchar_t wc);
wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc);
size_t wcsspn(const wchar_t *ws1, const wchar_t *ws2);
size_t wcscspn(const wchar_t *ws1, const wchar_t *ws2);
wchar_t *wcspbrk(const wchar_t *ws1, const wchar_t *ws2);
wchar_t *wcsstr(const wchar_t *ws1, const wchar_t *ws2);
wchar_t *wcstok(wchar_t *restrict ws1, const wchar_t *restrict ws2,
                wchar_t **restrict ptr);
wchar_t *wcsdup(const wchar_t *string);

/* Wide numeric conversions. */
double wcstod(const wchar_t *restrict nptr, wchar_t **restrict endptr);
float wcstof(const wchar_t *restrict nptr, wchar_t **restrict endptr);
long double wcstold(const wchar_t *restrict nptr, wchar_t **restrict endptr);
long wcstol(const wchar_t *restrict nptr, wchar_t **restrict endptr, int base);
unsigned long wcstoul(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                      int base);
long long wcstoll(const wchar_t *restrict nptr, wchar_t **restrict endptr,
                  int base);
unsigned long long wcstoull(const wchar_t *restrict nptr,
                            wchar_t **restrict endptr, int base);

/* Wide calendar formatting. */
size_t wcsftime(wchar_t *restrict wcs, size_t maxsize,
                const wchar_t *restrict format,
                const struct tm *restrict timeptr);

#endif /* _WCHAR_H */
