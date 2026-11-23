/**
 * @file     vfs.h
 * @brief    Ring 3 /dev/vfs IPC protocol structures and operation codes
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0007 ICD_IPC_0016 */

#include <stdint.h>

#define VFS_PATH_MAX 256u
#define VFS_DATA_MAX 384u

#define VFS_OP_OPEN 1u
#define VFS_OP_CLOSE 2u
#define VFS_OP_DUP 3u
#define VFS_OP_READ 4u
#define VFS_OP_WRITE 5u
#define VFS_OP_LSEEK 6u
#define VFS_OP_FTRUNCATE 7u
#define VFS_OP_STAT 8u
#define VFS_OP_FSTAT 9u
#define VFS_OP_MKDIR 10u
#define VFS_OP_UNLINK 11u
#define VFS_OP_READDIR 12u
#define VFS_OP_RESOLVE 13u
#define VFS_OP_STATS 14u
#define VFS_OP_RENAME 15u
#define VFS_OP_CHMOD 16u
#define VFS_OP_CHOWN 17u
#define VFS_OP_LINK 18u
#define VFS_OP_SYMLINK 19u
#define VFS_OP_READLINK 20u
#define VFS_OP_MKFIFO 21u
#define VFS_OP_PREAD 22u
#define VFS_OP_PWRITE 23u
#define VFS_OP_STATVFS 24u
#define VFS_OP_UTIMENS 25u

#define VFS_FLAG_BY_FD 0x80000000u
#define VFS_READDIR_NEXT 0x40000000u

#define VFS_RENAME_PATH_MAX 188u

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_size;
    int64_t st_atime_sec;
    int64_t st_atime_nsec;
    int64_t st_mtime_sec;
    int64_t st_mtime_nsec;
    int64_t st_ctime_sec;
    int64_t st_ctime_nsec;
} vfs_stat_t;

typedef struct {
    uint64_t f_bsize;
    uint64_t f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    uint64_t f_favail;
    uint64_t f_fsid;
    uint64_t f_flag;
    uint64_t f_namemax;
} vfs_statvfs_t;

typedef struct {
    uint32_t op;
    uint32_t handle;
    uint32_t flags;
    uint32_t mode;
    int64_t offset;
    uint32_t whence;
    uint32_t len;
    union {
        char path[VFS_PATH_MAX];
        uint8_t data[VFS_DATA_MAX];
        struct {
            uint32_t new_handle;
            char old_path[VFS_RENAME_PATH_MAX];
            char new_path[VFS_RENAME_PATH_MAX];
        } ren;
        struct {
            int64_t atime_sec;
            int64_t atime_nsec;
            int64_t mtime_sec;
            int64_t mtime_nsec;
            char path[VFS_PATH_MAX];
        } utimes;
    } u;
} vfs_req_t;

typedef struct {
    int32_t err;
    uint32_t handle;
    uint32_t len;
    uint32_t flags;
    int64_t offset;
    union {
        vfs_stat_t st;
        vfs_statvfs_t stvfs;
        char path[VFS_PATH_MAX];
        uint8_t data[VFS_DATA_MAX];
        struct {
            uint64_t d_ino;
            char d_name[256];
        } dent;
    } u;
} vfs_rep_t;

_Static_assert(sizeof(vfs_req_t) <= 512u, "vfs_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(vfs_rep_t) <= 512u, "vfs_rep_t exceeds IPC_MSG_MAX");
