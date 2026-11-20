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

#define VM_MAP_PRIVATE 0x02
#define VM_MAP_FIXED 0x10
#define VM_MAP_ANON 0x20

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
