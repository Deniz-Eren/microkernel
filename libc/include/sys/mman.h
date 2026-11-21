/**
 * @file     mman.h
 * @brief    POSIX.1-2024 Base memory management declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

/* need:: ICD_MEM_0003 */

#include <stddef.h>
#include <sys/_posix_opts.h>
#include <sys/types.h>

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

#define MS_ASYNC 0x1
#define MS_INVALIDATE 0x2
#define MS_SYNC 0x4

void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
int munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);
int msync(void *addr, size_t len, int flags);

#if defined(_POSIX_MEMLOCK) && (_POSIX_MEMLOCK > 0)
#define MCL_CURRENT 0x1
#define MCL_FUTURE 0x2
int mlockall(int flags);
int munlockall(void);
#endif

#if defined(_POSIX_MEMLOCK_RANGE) && (_POSIX_MEMLOCK_RANGE > 0)
int mlock(const void *addr, size_t len);
int munlock(const void *addr, size_t len);
#endif

#if defined(_POSIX_SHARED_MEMORY_OBJECTS) && (_POSIX_SHARED_MEMORY_OBJECTS > 0)
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);
#endif

#endif /* _SYS_MMAN_H */
