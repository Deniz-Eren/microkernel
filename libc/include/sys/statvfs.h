/**
 * @file     statvfs.h
 * @brief    POSIX.1-2024 file system information structures and declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_STATVFS_H
#define _SYS_STATVFS_H

/* need:: ICD_IPC_0016 */

#include <sys/types.h>

#define ST_RDONLY 1UL
#define ST_NOSUID 2UL

struct statvfs {
    unsigned long f_bsize;
    unsigned long f_frsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;
    fsfilcnt_t f_files;
    fsfilcnt_t f_ffree;
    fsfilcnt_t f_favail;
    unsigned long f_fsid;
    unsigned long f_flag;
    unsigned long f_namemax;
};

int statvfs(const char *restrict path, struct statvfs *restrict buf);
int fstatvfs(int fildes, struct statvfs *buf);

#endif /* _SYS_STATVFS_H */
