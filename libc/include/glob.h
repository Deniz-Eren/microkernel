/**
 * @file     glob.h
 * @brief    POSIX.1-2024 pathname pattern matching declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _GLOB_H
#define _GLOB_H

/* need:: ICD_LIBC_0014 */

#include <stddef.h>

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
} glob_t;

/* glob() flags. */
#define GLOB_APPEND 0x01
#define GLOB_DOOFFS 0x02
#define GLOB_ERR 0x04
#define GLOB_MARK 0x08
#define GLOB_NOCHECK 0x10
#define GLOB_NOESCAPE 0x20
#define GLOB_NOSORT 0x40

/* glob() error return values. */
#define GLOB_ABORTED 1
#define GLOB_NOMATCH 2
#define GLOB_NOSPACE 3

int glob(const char *restrict pattern, int flags,
         int (*errfunc)(const char *epath, int eerrno), glob_t *restrict pglob);
void globfree(glob_t *pglob);

#endif /* _GLOB_H */
