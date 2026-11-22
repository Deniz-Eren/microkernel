/**
 * @file     arch_trapframe.h
 * @brief    x86-64 saved CPU trapframe register layout and portable accessors
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_BOOT_0002 */

#include <stdint.h>
#include "gdt.h"

/* Register frame pushed by the IDT stubs (and synthesized for syscalls).
 * Memory layout, low -> high addresses, matches push order in isr.S. */
typedef struct trapframe {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec, err;
    uint64_t rip, cs, rflags, rsp, ss;
} trapframe_t;

static inline int arch_tf_is_user(const trapframe_t *tf) {
    return (tf->cs & 3ULL) != 0ULL;
}

static inline uint64_t arch_tf_syscall_num(const trapframe_t *tf) {
    return tf->rax;
}

static inline uint64_t arch_tf_arg0(const trapframe_t *tf) {
    return tf->rdi;
}

static inline uint64_t arch_tf_arg1(const trapframe_t *tf) {
    return tf->rsi;
}

static inline uint64_t arch_tf_arg2(const trapframe_t *tf) {
    return tf->rdx;
}

static inline uint64_t arch_tf_arg3(const trapframe_t *tf) {
    return tf->r10;
}

static inline uint64_t arch_tf_arg4(const trapframe_t *tf) {
    return tf->r8;
}

static inline uint64_t arch_tf_arg5(const trapframe_t *tf) {
    return tf->r9;
}

static inline void arch_tf_set_ret(trapframe_t *tf, long ret) {
    tf->rax = (uint64_t)ret;
}

static inline uint64_t arch_tf_user_sp(const trapframe_t *tf) {
    return tf->rsp;
}

static inline void arch_tf_init_user(trapframe_t *tf, uint64_t entry,
                                     uint64_t arg, uint64_t user_rsp,
                                     uint8_t iopl) {
    tf->rip = entry;
    tf->rdi = arg;
    tf->rsp = user_rsp;
    tf->cs = GDT_UCODE | 3ULL;
    tf->ss = GDT_UDATA | 3ULL;
    tf->rflags = 0x202ULL | (((uint64_t)iopl & 3ULL) << 12);
}

static inline void arch_tf_setup_fork_child(trapframe_t *dst,
                                            const trapframe_t *src,
                                            uint8_t iopl) {
    *dst = *src;
    dst->rax = 0;
    dst->cs = GDT_UCODE | 3ULL;
    dst->ss = GDT_UDATA | 3ULL;
    dst->rflags = (src->rflags & ~(uint64_t)(3ULL << 12)) | 0x202ULL |
                  (((uint64_t)iopl & 3ULL) << 12);
}

static inline void arch_tf_setup_signal_handler(trapframe_t *tf,
                                                uint64_t handler, unsigned sig,
                                                uint64_t ret_va, uint8_t iopl) {
    tf->rsp = ret_va;
    tf->rip = handler;
    tf->rdi = sig;
    tf->rax = 0;
    tf->rflags = 0x202ULL | (((uint64_t)iopl & 3ULL) << 12);
}
