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
        if (fildes < 0) {
            errno = EBADF;
            return MAP_FAILED;
        }
        long r = __sys6(SYS_SHM_MAP, (long)(uintptr_t)addr, (long)len,
                        (long)prot, (long)flags, (long)fildes, (long)off);
        if (r < 0) {
            errno = (int)(-r);
            return MAP_FAILED;
        }
        return (void *)(uintptr_t)r;
    }
    unsigned kflags =
        ((flags & MAP_SHARED) ? VM_MAP_SHARED : VM_MAP_PRIVATE) | VM_MAP_ANON;
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

int mlock(const void *addr, size_t len) {
    long r =
        __sys3(SYS_VM_MLOCK, VM_MLOCK_RANGE, (long)(uintptr_t)addr, (long)len);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int munlock(const void *addr, size_t len) {
    long r = __sys3(SYS_VM_MLOCK, VM_MUNLOCK_RANGE, (long)(uintptr_t)addr,
                    (long)len);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int mlockall(int flags) {
    long r = __sys3(SYS_VM_MLOCK, VM_MLOCK_ALL, (long)flags, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int munlockall(void) {
    long r = __sys3(SYS_VM_MLOCK, VM_MUNLOCK_ALL, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int shm_open(const char *name, int oflag, mode_t mode) {
    long r = __sys4(SYS_SHM_OP, SHM_OP_OPEN, (long)(uintptr_t)name, (long)oflag,
                    (long)mode);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (int)r;
}

int shm_unlink(const char *name) {
    long r = __sys4(SYS_SHM_OP, SHM_OP_UNLINK, (long)(uintptr_t)name, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}
