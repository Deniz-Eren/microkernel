/**
 * @file     fcntl.h
 * @brief    POSIX.1-2024 file descriptor control flags and fcntl declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FCNTL_H
#define _FCNTL_H

/* need:: ICD_IPC_0004 */

#include <sys/types.h>

/* File access modes. */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_ACCMODE 0x0003

/* File creation and status flags. */
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_NOCTTY 0x0100
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_NONBLOCK 0x0800
#define O_DSYNC 0x1000
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW 0x20000
#define O_CLOEXEC 0x80000
#define O_SYNC 0x101000

/* *at() special directory descriptor and flags. */
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_EACCESS 0x200
#define AT_SYMLINK_FOLLOW 0x400

/* File descriptor flags (F_GETFD / F_SETFD). */
#define FD_CLOEXEC 1

/* fcntl() commands. */
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_DUPFD_CLOEXEC 1030

int open(const char *path, int oflag, ...);
int openat(int fd, const char *path, int oflag, ...);
int creat(const char *path, mode_t mode);
int fcntl(int fd, int cmd, ...);

#endif /* _FCNTL_H */
