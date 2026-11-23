/**
 * @file     nl_types.h
 * @brief    POSIX.1-2024 message catalog types and catopen/catgets/catclose
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _NL_TYPES_H
#define _NL_TYPES_H

/* need:: ICD_LIBC_0015 */

typedef int nl_item;
typedef void *nl_catd;

#define NL_SETD 1
#define NL_CAT_LOCALE 1

nl_catd catopen(const char *name, int oflag);
char *catgets(nl_catd catd, int set_id, int msg_id, const char *s);
int catclose(nl_catd catd);

#endif /* _NL_TYPES_H */
