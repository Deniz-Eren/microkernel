/**
 * @file     dirent.h
 * @brief    POSIX.1-2024 directory entry and stream declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _DIRENT_H
#define _DIRENT_H

/* need:: ICD_IPC_0009 */

#include <sys/types.h>

struct dirent {
    ino_t d_ino;
    char d_name[256];
};

typedef struct __dirstream DIR;

DIR *opendir(const char *dirname);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
int closedir(DIR *dirp);
int dirfd(DIR *dirp);

#endif /* _DIRENT_H */
