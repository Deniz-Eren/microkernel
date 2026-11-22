/* Microkernel x86-64 higher-half kernel linker script.
 *
 * Physical load address : 0x00100000 (GRUB Multiboot2 ELF loader)
 * Virtual run address   : 0xFFFFFFFF80000000 (kernel code model)
 * .multiboot is placed at offset 0 of the first PT_LOAD so the Multiboot2
 * magic (0xE85250D6) sits well within the first 32 KB of kernel.elf.
 */
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_LMA    = 0x00100000;
KERNEL_VMA    = 0xFFFFFFFF80000000;
KERNEL_OFFSET = KERNEL_VMA - KERNEL_LMA;

SECTIONS
{
    . = KERNEL_LMA;
    __kernel_phys_start = .;

    .multiboot : {
        KEEP(*(.multiboot))
    }

    .boot.text : {
        *(.boot.text .boot.text.*)
    }

    .boot.data : {
        *(.boot.data .boot.data.*)
    }

    .boot.bss (NOLOAD) : {
        *(.boot.bss .boot.bss.*)
    }

    . = ALIGN(4096);
    __boot_phys_end = .;

    /* Switch to higher-half virtual addresses. */
    . = . + KERNEL_OFFSET;
    __kernel_virt_start = .;

    /*
     * Higher-half sections: LMA is derived from the section's own VMA so the
     * invariant phys = VMA - KERNEL_OFFSET holds exactly, even when the linker
     * aligns a section's VMA upward beyond the running cursor. Each section is
     * page-aligned so text/rodata/data never share a page (W^X remap needs
     * per-page permission boundaries).
     */
    . = ALIGN(4096);
    .text : AT(ADDR(.text) - KERNEL_OFFSET) {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - KERNEL_OFFSET) {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    . = ALIGN(4096);
    .data : AT(ADDR(.data) - KERNEL_OFFSET) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    . = ALIGN(4096);
    .bss (NOLOAD) : AT(ADDR(.bss) - KERNEL_OFFSET) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;

        /* Kernel bootstrap stack — outside the zeroed .bss range. */
        . = ALIGN(16);
        __kboot_stack_bottom = .;
        . += 0x20000;
        __kboot_stack_top = .;
    }

    __kernel_virt_end = .;
    __kernel_phys_end = __kernel_virt_end - KERNEL_OFFSET;

    /* Physical aliases so early boot code can zero .bss before paging. */
    __bss_phys_start = __bss_start - KERNEL_OFFSET;
    __bss_phys_end   = __bss_end   - KERNEL_OFFSET;

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
        *(.gnu.*)
    }
}
