/**
 * @file     fnmatch.h
 * @brief    POSIX.1-2024 filename wildcard matching constants and fnmatch()
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FNMATCH_H
#define _FNMATCH_H

/* need:: ICD_LIBC_0006 */

#define FNM_NOMATCH 1
#define FNM_PATHNAME 0x01
#define FNM_PERIOD 0x02
#define FNM_NOESCAPE 0x04
#define FNM_IGNORECASE 0x08

int fnmatch(const char *pattern, const char *string, int flags);

#endif /* _FNMATCH_H */
