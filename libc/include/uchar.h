/**
 * @file     uchar.h
 * @brief    ISO C17 and POSIX.1-2024 Unicode UTF-16/UTF-32 character support
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _UCHAR_H
#define _UCHAR_H

/* need:: ICD_LIBC_0004 */

#include <stddef.h>
#include <stdint.h>

typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;

#ifndef _MBSTATE_T_DEFINED
#define _MBSTATE_T_DEFINED
typedef struct {
    uint32_t __codepoint;
    uint16_t __pending_low;
    uint8_t __expected;
    uint8_t __seen;
} mbstate_t;
#endif /* _MBSTATE_T_DEFINED */

size_t mbrtoc16(char16_t *restrict pc16, const char *restrict s, size_t n,
                mbstate_t *restrict ps);
size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict ps);
size_t mbrtoc32(char32_t *restrict pc32, const char *restrict s, size_t n,
                mbstate_t *restrict ps);
size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps);

#endif /* _UCHAR_H */
