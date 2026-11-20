/**
 * @file     mman.c
 * @brief    POSIX.1-2024 Base mmap, munmap, mprotect, and msync wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <sys/microkernel.h>
#include <sys/mman.h>
#include <sys/vm.h>

/* need:: COMP_MEM_0003 */

void *mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off) {
    if (len == 0 || (off & 0xFFF) != 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    int share = flags & (MAP_SHARED | MAP_PRIVATE);
    if (share == 0 || share == (MAP_SHARED | MAP_PRIVATE)) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    if (!(flags & MAP_ANONYMOUS)) {
        (void)fildes;
        errno = ENOSYS;
        return MAP_FAILED;
    }
    unsigned kflags = VM_MAP_PRIVATE | VM_MAP_ANON;
    if (flags & MAP_FIXED) {
        kflags |= VM_MAP_FIXED;
    }
    long r = VMMap((uintptr_t)addr, len, (unsigned)prot, kflags);
    if (r < 0) {
        errno = (int)(-r);
        return MAP_FAILED;
    }
    return (void *)(uintptr_t)r;
}

int munmap(void *addr, size_t len) {
    long r = VMUnmap((uintptr_t)addr, len);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int mprotect(void *addr, size_t len, int prot) {
    long r = VMProtect((uintptr_t)addr, len, (unsigned)prot);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int msync(void *addr, size_t len, int flags) {
    if (len == 0 || ((uintptr_t)addr & 0xFFFU) != 0) {
        errno = EINVAL;
        return -1;
    }
    if ((flags & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE)) != 0 ||
        ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
