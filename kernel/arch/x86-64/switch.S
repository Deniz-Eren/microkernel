/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */

/* Context switch, per-CPU GS syscall entry, and thread bootstrap.
 * Offsets into struct tcb must match kernel/include/kernel/sched.h:
 *   TCB_KRSP (32) : kernel_rsp   (asserted in sched.c via _Static_assert)
 *   TCB_FPU  (40) : fpu_area     (pointer to 64-byte aligned XSAVE image)
 * Offsets into struct cpu_local must match kernel/include/kernel/smp.h:
 *   CPU_SYSCALL_KSTACK  (8)  : active thread kernel stack top
 *   CPU_SYSCALL_RSP_TMP (16) : scratch slot for user RSP on syscall entry */

.set TCB_KRSP, 32
.set TCB_FPU,  40
.set CPU_SYSCALL_KSTACK,  8
.set CPU_SYSCALL_RSP_TMP, 16

.section .text

/* void switch_to(tcb_t *prev, tcb_t *next) — never returns on the caller's
 * stack; resumes `next` at its saved kernel_rsp. Called with g_sched_lock held
 * and IRQs disabled; caller calls sched_post_switch() after returning. */
.globl switch_to
.type switch_to, @function
switch_to:
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, TCB_KRSP(%rdi)

    /* Eager extended-state save/restore via XSAVE/XRSTOR: kernel stays scalar,
     * Ring 3 uses AVX/AVX2 (and optional AVX-512 when present). */
    movl g_xsave_mask_lo(%rip), %eax
    movl g_xsave_mask_hi(%rip), %edx
    movq TCB_FPU(%rdi), %rcx
    xsave (%rcx)
    movq TCB_FPU(%rsi), %rcx
    xrstor (%rcx)

    movq TCB_KRSP(%rsi), %rsp
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    ret
.size switch_to, . - switch_to

/* A freshly spawned user thread resumes here: its kernel stack holds
 * [6 callee-saved slots][&thread_bootstrap][trapframe]. Finalize outgoing
 * switch_to state and unlock g_sched_lock before returning to Ring 3. */
.globl thread_bootstrap
.type thread_bootstrap, @function
thread_bootstrap:
    callq sched_post_switch
    cli
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp              /* vec + err */
    swapgs
    iretq                       /* enter Ring 3 */
.size thread_bootstrap, . - thread_bootstrap

/* Fast syscall entry. On entry: RAX=nr, RCX=user RIP, R11=user RFLAGS,
 * IF=0 (SFMASK). Switches to per-CPU GS.base, builds a trapframe shaped
 * identically to the IDT stubs, enables IRQs inside syscall_handler, and
 * restores user GS.base before sysretq. */
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    swapgs
    movq %rsp, %gs:CPU_SYSCALL_RSP_TMP
    movq %gs:CPU_SYSCALL_KSTACK, %rsp

    pushq $0x1B                 /* user SS (UDATA|3) */
    pushq %gs:CPU_SYSCALL_RSP_TMP
    pushq %r11                  /* user RFLAGS */
    pushq $0x27                 /* user CS (UCODE|3) */
    pushq %rcx                  /* user RIP */
    pushq $0                    /* err */
    pushq $0xFFFFFFFFFFFFFFFF   /* vec marker (syscall) */

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    cld
    sti
    callq syscall_handler
    cli

    /* Restore: everything except RAX (holds the return value). */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx

    movq 0*8(%rsp), %rax        /* return value (rax slot) */
    movq 3*8(%rsp), %rcx        /* user RIP */
    movq 5*8(%rsp), %r11        /* user RFLAGS */
    movq 6*8(%rsp), %rsp        /* user RSP */
    swapgs
    sysretq
.size syscall_entry, . - syscall_entry
