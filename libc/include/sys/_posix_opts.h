/**
 * @file     _posix_opts.h
 * @brief    POSIX.1-2024 active option group feature macros
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS__POSIX_OPTS_H
#define _SYS__POSIX_OPTS_H

/* need:: ICD_LIBC_0002 */

/* Active POSIX.1-2024 (Issue 8) option groups. */
#define _POSIX_IPV6 202405L
#define _POSIX_SHARED_MEMORY_OBJECTS 202405L
#define _POSIX_MEMLOCK 202405L
#define _POSIX_MEMLOCK_RANGE 202405L
#define _POSIX_PRIORITY_SCHEDULING 202405L
#define _POSIX_SYNCHRONIZED_IO 202405L
#define _POSIX_THREAD_ATTR_STACKADDR 202405L
#define _POSIX_THREAD_PRIO_PROTECT 202405L

#endif /* _SYS__POSIX_OPTS_H */
