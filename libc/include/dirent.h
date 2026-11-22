/**
 * @file     dirent.h
 * @brief    POSIX.1-2024 directory entry, posix_getdents, and scandir API
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DIRENT_H
#define _DIRENT_H

/* need:: ICD_IPC_0009 ICD_IPC_0016 */

#include <stddef.h>
#include <sys/types.h>

#define DT_UNKNOWN 0u
#define DT_FIFO 1u
#define DT_CHR 2u
#define DT_DIR 4u
#define DT_BLK 6u
#define DT_REG 8u
#define DT_LNK 10u
#define DT_SOCK 12u

struct dirent {
    ino_t d_ino;
    char d_name[256];
};

struct posix_dent {
    ino_t d_ino;
    reclen_t d_reclen;
    unsigned char d_type;
    char d_name[];
};

typedef struct __dirstream DIR;

DIR *opendir(const char *dirname);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dirp);
int readdir_r(DIR *restrict dirp, struct dirent *restrict entry,
              struct dirent **restrict result);
void rewinddir(DIR *dirp);
int closedir(DIR *dirp);
int dirfd(DIR *dirp);
ssize_t posix_getdents(int fildes, void *buf, size_t nbyte, int flags);
int alphasort(const struct dirent **d1, const struct dirent **d2);
int scandir(const char *dir, struct dirent ***namelist,
            int (*sel)(const struct dirent *),
            int (*compar)(const struct dirent **, const struct dirent **));

#endif /* _DIRENT_H */
