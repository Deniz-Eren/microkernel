/**
 * @file     wordexp.h
 * @brief    POSIX.1-2024 shell word expansion declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _WORDEXP_H
#define _WORDEXP_H

/* need:: ICD_LIBC_0014 */

#include <stddef.h>

typedef struct {
    size_t we_wordc;
    char **we_wordv;
    size_t we_offs;
} wordexp_t;

/* wordexp() flags. */
#define WRDE_APPEND 0x01
#define WRDE_DOOFFS 0x02
#define WRDE_NOCMD 0x04
#define WRDE_REUSE 0x08
#define WRDE_SHOWERR 0x10
#define WRDE_UNDEF 0x20

/* wordexp() error return values. */
#define WRDE_BADCHAR 1
#define WRDE_BADVAL 2
#define WRDE_CMDSUB 3
#define WRDE_NOSPACE 4
#define WRDE_SYNTAX 5

int wordexp(const char *restrict words, wordexp_t *restrict pwordexp,
            int flags);
void wordfree(wordexp_t *pwordexp);

#endif /* _WORDEXP_H */
