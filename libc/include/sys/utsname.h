/**
 * @file     utsname.h
 * @brief    POSIX.1-2024 system identification structure and uname() interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

/* need:: ICD_LIBC_0006 */

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

int uname(struct utsname *name);

#endif /* _SYS_UTSNAME_H */
