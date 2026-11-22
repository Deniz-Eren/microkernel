/* Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com> */
/* SPDX-License-Identifier: MIT */

/*
 * Microkernel Multiboot2 boot trampoline.
 *
 * Entry state (from GRUB): 32-bit protected mode, paging off,
 * EAX = 0x36D76289 (Multiboot2 magic), EBX = physical MBI address.
 *
 * This code is linked at its physical address (VMA == LMA) because it runs
 * before the higher-half mapping is installed. It:
 *   1. zeroes the kernel .bss (via its physical alias),
 *   2. builds initial 4-level page tables:
 *        - identity map 0 .. 1 GiB (2 MiB pages)
 *        - HHDM    0xFFFF800000000000 .. (phys + HHDM_OFFSET)
 *        - kernel  0xFFFFFFFF80000000 .. -> physical 0x00100000 (4 KiB pages;
 *          the kernel load address 0x100000 is not 2 MiB aligned, so a leaf
 *          PD entry would carry a reserved-bit violation)
 *   3. enables PAE, LME, paging, loads the 64-bit GDT and far-jumps to
 *      64-bit mode, then transfers control to kmain64() in the higher half.
 */

.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH_I386, 0

.section .multiboot, "a"
.align 8
mb2_header_start:
    .long MB2_MAGIC
    .long MB2_ARCH_I386
    .long mb2_header_end - mb2_header_start
    .long -(MB2_MAGIC + MB2_ARCH_I386 + (mb2_header_end - mb2_header_start))
.align 8
    /* end tag */
    .word 0
    .word 0
    .long 8
mb2_header_end:

.section .boot.text, "ax"
.code32
.extern build_page_tables
.extern kmain64

.globl _start
_start:
    cli
    cld
    cmp $0x36D76289, %eax           /* Multiboot2 boot magic */
    jne .Lno_multiboot
    mov %ebx, %edi                  /* preserve MBI physical address */

    /* Zero the higher-half .bss through its physical alias. */
    movl $__bss_phys_start, %edi
    movl $__bss_phys_end, %ecx
    subl %edi, %ecx
    shrl $2, %ecx
    xorl %eax, %eax
    rep stosl

    /* Preserve the MBI physical address (build_page_tables clobbers EDI). */
    movl %ebx, (mbi_phys_slot)

    /* Set up the 32-bit boot stack. */
    mov $boot_stack_top, %esp
    xor %ebp, %ebp

    call build_page_tables          /* EAX = PML4 physical address */
    mov %eax, %cr3

    mov %cr4, %eax
    or  $0x00000020, %eax           /* CR4.PAE */
    mov %eax, %cr4

    mov $0xC0000080, %ecx           /* IA32_EFER */
    rdmsr
    or  $0x00000900, %eax           /* EFER.LME | EFER.NXE (NX PTEs legal) */
    wrmsr

    mov %cr0, %eax
    or  $0x80010000, %eax           /* CR0.PG | CR0.PE */
    mov %eax, %cr0

    lgdt gdtr32
    ljmp $0x08, $long_mode_entry

.Lno_multiboot:
    hlt
    jmp .Lno_multiboot

.code64
long_mode_entry:
    /* Still executing at physical addresses via the identity map. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    movabs $__kboot_stack_top, %rsp   /* switch to the higher-half kernel stack */
    xor %ebp, %ebp
    movl (mbi_phys_slot), %edi       /* MBI physical -> RDI (zero-extended) */
    movabs $kmain64, %rax
    call *%rax
.Lhang64:
    hlt
    jmp .Lhang64

/*
 * build_page_tables: fills the boot page tables described above.
 * Returns PML4 physical address in EAX. Clobbers EAX, ECX, EDI.
 */
.code32
build_page_tables:
    movl $boot_pml4, %edi
    movl $(7 * 4096 / 4), %ecx
    xorl %eax, %eax
    rep stosl

    /* PML4[0] -> pdpt_id (identity) */
    movl $boot_pdpt_id, %eax
    or  $0x03, %eax                 /* present | writable */
    movl %eax, boot_pml4 + 0 * 8
    /* PML4[256] -> pdpt_hh (HHDM) */
    movl $boot_pdpt_hh, %eax
    or  $0x03, %eax
    movl %eax, boot_pml4 + 256 * 8
    /* PML4[511] -> pdpt_k (kernel image) */
    movl $boot_pdpt_k, %eax
    or  $0x03, %eax
    movl %eax, boot_pml4 + 511 * 8

    /* pdpt_id[0] and pdpt_hh[0] -> pd_id0 (shared 1 GiB of 2 MiB pages) */
    movl $boot_pd_id0, %eax
    or  $0x03, %eax
    movl %eax, boot_pdpt_id + 0 * 8
    movl %eax, boot_pdpt_hh + 0 * 8

    /* pd_id0: identity map physical 0 .. 1 GiB with 2 MiB pages */
    movl $boot_pd_id0, %edi
    movl $0x00000083, %eax          /* phys 0 | present | write | huge */
    movl $512, %ecx
.Lfill_pd_id0:
    movl %eax, (%edi)
    addl $0x00200000, %eax
    addl $8, %edi
    loop .Lfill_pd_id0

    /* pt_k0: VA 0xFFFFFFFF80000000 + i*4KiB -> PA 0x00100000 + i*4KiB
     * (first 2 MiB of the kernel window: image + boot stack) */
    movl $boot_pt_k0, %edi
    movl $0x00100003, %eax          /* PA 0x100000 | present | write */
    movl $512, %ecx
.Lfill_pt_k0:
    movl %eax, (%edi)
    addl $0x00001000, %eax
    addl $8, %edi
    loop .Lfill_pt_k0

    /* pd_k[0] -> pt_k0; remaining PD entries stay unmapped (VMM extends). */
    movl $boot_pt_k0, %eax
    or  $0x03, %eax
    movl %eax, boot_pd_k + 0 * 8

    /* pdpt_k[510] -> pd_k (covers VA 0xFFFFFFFF80000000 .. +1 GiB) */
    movl $boot_pd_k, %eax
    or  $0x03, %eax
    movl %eax, boot_pdpt_k + 510 * 8

    movl $boot_pml4, %eax
    ret

.section .boot.data, "aw"
.align 16
.globl mbi_phys_slot
mbi_phys_slot:
    .long 0

gdt64:
    .quad 0x0000000000000000        /* null descriptor */
    .quad 0x00AF9A000000FFFF        /* 0x08: 64-bit code (L=1) */
    .quad 0x00CF92000000FFFF        /* 0x10: data */
gdt64_end:
gdtr32:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .boot.bss, "aw", @nobits
.align 4096
boot_pml4:    .space 4096
boot_pdpt_id: .space 4096
boot_pd_id0:  .space 4096
boot_pdpt_hh: .space 4096
boot_pdpt_k:  .space 4096
boot_pd_k:    .space 4096
boot_pt_k0:   .space 4096

.align 16
boot_stack:
    .space 16384
.globl boot_stack_top
boot_stack_top:
