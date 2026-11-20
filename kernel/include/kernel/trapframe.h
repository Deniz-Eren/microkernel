// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
/* Register frame pushed by the IDT stubs (and synthesized for syscalls).
 * Memory layout, low -> high addresses, matches push order in isr.S. */
#include <stdint.h>

typedef struct trapframe {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec, err;
    uint64_t rip, cs, rflags, rsp, ss;
} trapframe_t;
