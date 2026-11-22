/**
 * @file     vm.h
 * @brief    Ring 3 native virtual memory map, unmap, protect, and COW wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_MEM_0002 */
#include <stddef.h>
#include <stdint.h>
#include <sys/microkernel.h>

#define VM_PROT_NONE 0
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
#define VM_PROT_EXEC 4

#define VM_MAP_SHARED 0x01
#define VM_MAP_PRIVATE 0x02
#define VM_MAP_FIXED 0x10
#define VM_MAP_ANON 0x20

#define SHM_OP_OPEN 1u
#define SHM_OP_UNLINK 2u
#define SHM_OP_TRUNCATE 3u
#define SHM_OP_STAT 4u

#define VM_MLOCK_RANGE 1u
#define VM_MUNLOCK_RANGE 2u
#define VM_MLOCK_ALL 3u
#define VM_MUNLOCK_ALL 4u

struct __k_shm_stat {
    uint64_t size;
    uint32_t mode;
    uint32_t id;
};

struct __k_sys_stats {
    uint64_t pmm_total_bytes;
    uint64_t pmm_free_bytes;
    uint32_t live_procs;
    uint32_t live_threads;
    uint32_t open_fds;
    uint32_t active_shm_objs;
    uint32_t online_cpus;
};

static inline long SysStats(struct __k_sys_stats *out) {
    return __sys1(SYS_SYS_STATS, (long)(uintptr_t)out);
}

static inline long VMMap(uintptr_t va_hint, size_t len, unsigned prot,
                         unsigned flags) {
    return __sys4(SYS_VM_MAP, (long)va_hint, (long)len, (long)prot,
                  (long)flags);
}

static inline long VMUnmap(uintptr_t va, size_t len) {
    return __sys2(SYS_VM_UNMAP, (long)va, (long)len);
}

static inline long VMProtect(uintptr_t va, size_t len, unsigned prot) {
    return __sys3(SYS_VM_PROTECT, (long)va, (long)len, (long)prot);
}

static inline long VMCloneCow(uint32_t dst_pid, uintptr_t va, size_t len) {
    return __sys3(SYS_VM_CLONE_COW, (long)dst_pid, (long)va, (long)len);
}
