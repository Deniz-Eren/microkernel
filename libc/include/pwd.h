/**
 * @file     pwd.h
 * @brief    POSIX.1-2024 user password database structure and Base lookups
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PWD_H
#define _PWD_H

/* need:: ICD_PROC_0010 */

#include <stddef.h>
#include <sys/types.h>

struct passwd {
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};

struct passwd *getpwnam(const char *name);
int getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
               size_t bufsize, struct passwd **result);
struct passwd *getpwuid(uid_t uid);
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer, size_t bufsize,
               struct passwd **result);
void setpwent(void);
struct passwd *getpwent(void);
void endpwent(void);

#endif /* _PWD_H */
