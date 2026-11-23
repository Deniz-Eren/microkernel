/**
 * @file     ctype.h
 * @brief    POSIX.1-2024 C locale character classification and case mapping
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CTYPE_H
#define _CTYPE_H

/* need:: ICD_LIBC_0005 ICD_LIBC_0012 */

#include <sys/_locale_t.h>

int isalnum(int c);
int isalnum_l(int c, locale_t locale);
int isalpha(int c);
int isalpha_l(int c, locale_t locale);
int isblank(int c);
int isblank_l(int c, locale_t locale);
int iscntrl(int c);
int iscntrl_l(int c, locale_t locale);
int isdigit(int c);
int isdigit_l(int c, locale_t locale);
int isgraph(int c);
int isgraph_l(int c, locale_t locale);
int islower(int c);
int islower_l(int c, locale_t locale);
int isprint(int c);
int isprint_l(int c, locale_t locale);
int ispunct(int c);
int ispunct_l(int c, locale_t locale);
int isspace(int c);
int isspace_l(int c, locale_t locale);
int isupper(int c);
int isupper_l(int c, locale_t locale);
int isxdigit(int c);
int isxdigit_l(int c, locale_t locale);
int tolower(int c);
int tolower_l(int c, locale_t locale);
int toupper(int c);
int toupper_l(int c, locale_t locale);

#endif /* _CTYPE_H */
