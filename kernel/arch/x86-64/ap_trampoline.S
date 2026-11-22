/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */

/*
 * x86-64 Application Processor (AP) SIPI trampoline.
 *
 * Copied by the BSP to physical address 0x00008000 (SIPI vector 0x08).
 * Transitions an AP from 16-bit real mode -> 32-bit protected mode -> 64-bit
 * long mode using a temporary identity+higher-half PML4 and transfers control
 * to the higher-half C entry point supplied in the mailbox at 0x8F00:
 *   0x8F00 : uint64_t tramp_cr3        (temporary PML4 physical address)
 *   0x8F08 : uint64_t tramp_stack_top  (16 KiB higher-half kernel stack top)
 *   0x8F10 : uint64_t tramp_entry      (higher-half arch_ap_tramp_entry VA)
 *   0x8F18 : uint64_t tramp_cpu_index  (logical CPU index 1..N-1)
 */

.set AP_TRAMP_BASE, 0x8000
.set AP_MAILBOX,    0x8F00

.section .rodata
.align 16
.globl ap_trampoline_start
.globl ap_trampoline_end

ap_trampoline_start:
.code16
    cli
    cld
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    /* Load flat 32/64-bit GDT located inside the 0x8000 trampoline page. */
    lgdtl (AP_TRAMP_BASE + (ap_gdtr - ap_trampoline_start))

    /* Enter 32-bit protected mode (CR0.PE = 1). */
    movl %cr0, %eax
    orl  $0x1, %eax
    movl %eax, %cr0

    ljmpl $0x08, $(AP_TRAMP_BASE + (ap_pm32 - ap_trampoline_start))

.code32
.align 8
ap_pm32:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    /* Load temporary trampoline PML4 physical address from mailbox. */
    movl (AP_MAILBOX), %eax
    movl %eax, %cr3

    /* Enable PAE (CR4.PAE = bit 5). */
    movl %cr4, %eax
    orl  $0x20, %eax
    movl %eax, %cr4

    /* Enable Long Mode and NXE in IA32_EFER. */
    movl $0xC0000080, %ecx
    rdmsr
    orl  $0x00000900, %eax
    wrmsr

    /* Enable Paging and Write Protect (CR0.PG | CR0.WP | CR0.PE). */
    movl %cr0, %eax
    orl  $0x80010001, %eax
    movl %eax, %cr0

    /* Far jump into 64-bit long mode code segment (selector 0x18). */
    ljmpl $0x18, $(AP_TRAMP_BASE + (ap_lm64 - ap_trampoline_start))

.code64
.align 8
ap_lm64:
    movw $0x20, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    /* Read higher-half stack, entry function, and CPU index from mailbox. */
    movl $AP_MAILBOX, %ebx
    movq 8(%rbx), %rsp
    xorq %rbp, %rbp
    movq 24(%rbx), %rdi
    movq 16(%rbx), %rax
    callq *%rax
1:
    cli
    hlt
    jmp 1b

.align 16
ap_gdt:
    .quad 0x0000000000000000        /* 0x00: null */
    .quad 0x00CF9A000000FFFF        /* 0x08: 32-bit flat code */
    .quad 0x00CF92000000FFFF        /* 0x10: 32-bit flat data */
    .quad 0x00AF9A000000FFFF        /* 0x18: 64-bit code (L=1) */
    .quad 0x00CF92000000FFFF        /* 0x20: 64-bit data */
ap_gdt_end:

ap_gdtr:
    .word ap_gdt_end - ap_gdt - 1
    .long (AP_TRAMP_BASE + (ap_gdt - ap_trampoline_start))

ap_trampoline_end:
