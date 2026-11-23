/**
 * @file     grp.h
 * @brief    POSIX.1-2024 group database structure and Base lookups
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _GRP_H
#define _GRP_H

/* need:: ICD_PROC_0010 */

#include <stddef.h>
#include <sys/types.h>

struct group {
    char *gr_name;
    gid_t gr_gid;
    char **gr_mem;
};

struct group *getgrgid(gid_t gid);
int getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsize,
               struct group **result);
struct group *getgrnam(const char *name);
int getgrnam_r(const char *name, struct group *grp, char *buffer,
               size_t bufsize, struct group **result);
void setgrent(void);
struct group *getgrent(void);
void endgrent(void);

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) ||                  \
    defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_GNU_SOURCE)
int setgroups(size_t size, const gid_t *list);
int initgroups(const char *user, gid_t group);
int getgrouplist(const char *user, gid_t group, gid_t *groups, int *ngroups);
#endif

#endif /* _GRP_H */
