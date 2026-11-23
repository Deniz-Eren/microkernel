/**
 * @file     iconv.h
 * @brief    POSIX.1-2024 codeset conversion interface (iconv_open/iconv/close)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ICONV_H
#define _ICONV_H

/* need:: ICD_LIBC_0015 */

#include <stddef.h>

typedef void *iconv_t;

iconv_t iconv_open(const char *tocode, const char *fromcode);
size_t iconv(iconv_t cd, char **restrict inbuf, size_t *restrict inbytesleft,
             char **restrict outbuf, size_t *restrict outbytesleft);
int iconv_close(iconv_t cd);

#endif /* _ICONV_H */
