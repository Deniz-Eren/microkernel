/* Context switch, syscall entry, and thread bootstrap.
 * Offsets into struct tcb must match kernel/include/kernel/sched.h:
 *   TCB_KRSP (32) : kernel_rsp   (asserted in sched.c via _Static_assert)
 *   TCB_FPU  (40) : fpu_area     (pointer to 512-byte aligned image) */

.set TCB_KRSP, 32
.set TCB_FPU,  40

.section .text

/* void switch_to(tcb_t *prev, tcb_t *next) — never returns on the caller's
 * stack; resumes `next` at its saved kernel_rsp. */
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

    /* Eager FPU save/restore: kernel code never touches XMM, Ring 3 may. */
    movq TCB_FPU(%rdi), %rax
    fxsave (%rax)
    movq TCB_FPU(%rsi), %rax
    fxrstor (%rax)

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
 * [6 callee-saved slots][&thread_bootstrap][trapframe]. */
.globl thread_bootstrap
.type thread_bootstrap, @function
thread_bootstrap:
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
    iretq                       /* enter Ring 3 */
.size thread_bootstrap, . - thread_bootstrap

/* Kernel-side storage used by the syscall entry (single-CPU for now;
 * moves to a per-CPU GS area when SMP lands). */
.section .data
.globl g_syscall_kstack
g_syscall_kstack:   .quad 0
.globl g_syscall_rsp_scratch
g_syscall_rsp_scratch: .quad 0

.section .text
/* Fast syscall entry. On entry: RAX=nr, RCX=user RIP, R11=user RFLAGS,
 * IF=0 (SFMASK). Builds a trapframe shaped identically to the IDT stubs. */
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
    movq %rsp, g_syscall_rsp_scratch(%rip)
    movq g_syscall_kstack(%rip), %rsp

    pushq $0x1B                 /* user SS (UDATA|3) */
    pushq g_syscall_rsp_scratch(%rip)
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
    callq syscall_handler

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
    sysretq
.size syscall_entry, . - syscall_entry
