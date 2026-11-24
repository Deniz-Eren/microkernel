/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */

/* Microkernel dynamic test executable entry (freestanding, ET_DYN with
 * PT_INTERP). The kernel-built stack layout matches crt0.S: RSP points at
 * argc on entry; ld.so jumps here with the same stack. */

.section .text
.globl _start
.type _start, @function
_start:
    xor %ebp, %ebp                  /* mark outermost frame */
    mov %rsp, %rdi                  /* arg: pointer to argc block */
    and $-16, %rsp                  /* align stack per SysV ABI */
    call dyn_main
.Lhang:
    hlt
    jmp .Lhang
.size _start, . - _start
