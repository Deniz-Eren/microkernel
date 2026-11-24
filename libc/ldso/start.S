/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */

/* Microkernel Ring 3 dynamic linker entry. The kernel builds the initial
 * stack: [argc][argv...][NULL][envp...][NULL][auxv pairs...][AT_NULL]. RSP
 * points at argc on entry; ld.so passes it unchanged to AT_ENTRY. */

.section .text
.globl _start
.type _start, @function
_start:
    xor %ebp, %ebp                  /* mark outermost frame */
    mov %rsp, %rdi                  /* arg: pointer to argc block */
    and $-16, %rsp                  /* align stack per SysV ABI */
    call ldso_main
.Lhang:
    hlt
    jmp .Lhang
.size _start, . - _start

/* Jump to the main executable entry with the original kernel-built stack.
 * SysV args: rdi = entry address, rsi = argc block pointer. */
.globl ldso_jump
.type ldso_jump, @function
ldso_jump:
    mov %rsi, %rsp                  /* restore initial stack (RSP -> argc) */
    jmp *%rdi                       /* tail-jump to AT_ENTRY */
.size ldso_jump, . - ldso_jump
