/**
 * @file     wctype.h
 * @brief    POSIX.1-2024 wide character classification and case mapping APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _WCTYPE_H
#define _WCTYPE_H

/* need:: ICD_LIBC_0012 */

#define __need_wint_t
#include <stddef.h>
#include <sys/_locale_t.h>

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

typedef unsigned int wctype_t;
typedef unsigned int wctrans_t;

int iswalnum(wint_t wc);
int iswalnum_l(wint_t wc, locale_t locale);
int iswalpha(wint_t wc);
int iswalpha_l(wint_t wc, locale_t locale);
int iswblank(wint_t wc);
int iswblank_l(wint_t wc, locale_t locale);
int iswcntrl(wint_t wc);
int iswcntrl_l(wint_t wc, locale_t locale);
int iswdigit(wint_t wc);
int iswdigit_l(wint_t wc, locale_t locale);
int iswgraph(wint_t wc);
int iswgraph_l(wint_t wc, locale_t locale);
int iswlower(wint_t wc);
int iswlower_l(wint_t wc, locale_t locale);
int iswprint(wint_t wc);
int iswprint_l(wint_t wc, locale_t locale);
int iswpunct(wint_t wc);
int iswpunct_l(wint_t wc, locale_t locale);
int iswspace(wint_t wc);
int iswspace_l(wint_t wc, locale_t locale);
int iswupper(wint_t wc);
int iswupper_l(wint_t wc, locale_t locale);
int iswxdigit(wint_t wc);
int iswxdigit_l(wint_t wc, locale_t locale);

wctype_t wctype(const char *property);
wctype_t wctype_l(const char *property, locale_t locale);
int iswctype(wint_t wc, wctype_t charclass);
int iswctype_l(wint_t wc, wctype_t charclass, locale_t locale);

wint_t towlower(wint_t wc);
wint_t towlower_l(wint_t wc, locale_t locale);
wint_t towupper(wint_t wc);
wint_t towupper_l(wint_t wc, locale_t locale);

wctrans_t wctrans(const char *charclass);
wctrans_t wctrans_l(const char *charclass, locale_t locale);
wint_t towctrans(wint_t wc, wctrans_t desc);
wint_t towctrans_l(wint_t wc, wctrans_t desc, locale_t locale);

#endif /* _WCTYPE_H */
