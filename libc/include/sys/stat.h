/**
 * @file     stat.h
 * @brief    POSIX.1-2024 file status structures, mode macros, and declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

/* need:: ICD_IPC_0008 ICD_IPC_0016 ICD_LIBC_0011 */

#include <sys/types.h>
#include <time.h>

/* File type mask and type constants. */
#define S_IFMT 0170000u
#define S_IFIFO 0010000u
#define S_IFCHR 0020000u
#define S_IFDIR 0040000u
#define S_IFBLK 0060000u
#define S_IFREG 0100000u
#define S_IFLNK 0120000u
#define S_IFSOCK 0140000u

#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m)&S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m)&S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO)
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m)&S_IFMT) == S_IFSOCK)

/* File permission and special mode bits. */
#define S_ISUID 04000u
#define S_ISGID 02000u
#define S_ISVTX 01000u

#define S_IRWXU 00700u
#define S_IRUSR 00400u
#define S_IWUSR 00200u
#define S_IXUSR 00100u

#define S_IRWXG 00070u
#define S_IRGRP 00040u
#define S_IWGRP 00020u
#define S_IXGRP 00010u

#define S_IRWXO 00007u
#define S_IROTH 00004u
#define S_IWOTH 00002u
#define S_IXOTH 00001u

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
};

#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

/* Special tv_nsec values for futimens() and utimensat(). */
#define UTIME_NOW ((1L << 30) - 1L)
#define UTIME_OMIT ((1L << 30) - 2L)

int stat(const char *restrict path, struct stat *restrict buf);
int fstat(int fildes, struct stat *buf);
int lstat(const char *restrict path, struct stat *restrict buf);
int fstatat(int fd, const char *restrict path, struct stat *restrict buf,
            int flag);
int mkdir(const char *path, mode_t mode);
int mkdirat(int fd, const char *path, mode_t mode);
int mkfifo(const char *path, mode_t mode);
int mkfifoat(int fd, const char *path, mode_t mode);
int chmod(const char *path, mode_t mode);
int fchmod(int fildes, mode_t mode);
int fchmodat(int fd, const char *path, mode_t mode, int flag);
int futimens(int fd, const struct timespec times[2]);
int utimensat(int fd, const char *path, const struct timespec times[2],
              int flag);
mode_t umask(mode_t cmask);

#endif /* _SYS_STAT_H */
