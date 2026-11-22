/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */
/* need:: COMP_LIBC_0006 */

/* x86-64 System V ABI setjmp, longjmp, and sigsetjmp.
 * jmp_buf layout (8 x 8 bytes = 64 bytes):
 *   0: rbx, 8: rbp, 16: r12, 24: r13, 32: r14, 40: r15, 48: rsp, 56: rip
 * sigjmp_buf layout:
 *   0..63: jmp_buf, 64: int savemask, 68: unsigned int sigmask */

.section .text

.globl setjmp
.type setjmp, @function
setjmp:
    movq %rbx,  0(%rdi)
    movq %rbp,  8(%rdi)
    movq %r12, 16(%rdi)
    movq %r13, 24(%rdi)
    movq %r14, 32(%rdi)
    movq %r15, 40(%rdi)
    leaq 8(%rsp), %rdx
    movq %rdx, 48(%rdi)
    movq (%rsp), %rdx
    movq %rdx, 56(%rdi)
    xorl %eax, %eax
    ret
.size setjmp, . - setjmp

.globl longjmp
.type longjmp, @function
longjmp:
    movq  0(%rdi), %rbx
    movq  8(%rdi), %rbp
    movq 16(%rdi), %r12
    movq 24(%rdi), %r13
    movq 32(%rdi), %r14
    movq 40(%rdi), %r15
    movq 48(%rdi), %rsp
    movl %esi, %eax
    testl %eax, %eax
    jnz 1f
    movl $1, %eax
1:
    jmp *56(%rdi)
.size longjmp, . - longjmp

.globl sigsetjmp
.type sigsetjmp, @function
sigsetjmp:
    movl %esi, 64(%rdi)
    testl %esi, %esi
    jz setjmp
    pushq %rdi
    leaq 68(%rdi), %rdx         /* oset = &env->__sigmask */
    xorl %esi, %esi             /* set = NULL */
    movl $2, %edi               /* SIG_SETMASK */
    callq sigprocmask
    popq %rdi
    jmp setjmp
.size sigsetjmp, . - sigsetjmp
