/* Microkernel Ring 3 dynamic executable linker script (ET_DYN + PT_INTERP).
 * Image base 0 per the SysV PIE convention: the kernel maps the image at
 * ELF_DYN_BASE and reports AT_PHDR = bias + phdr vaddr, so /boot/ld.so can
 * locate the main load bias from the ELF identity on the first page. The
 * FILEHDR PHDRS clause keeps the ELF header and program header table inside
 * the first RX PT_LOAD so the kernel auxv reports a mapped AT_PHDR. Separate
 * RX / R / RW segments preserve the kernel and ld.so W^X policy. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text    PT_LOAD FILEHDR PHDRS FLAGS(5); /* R + X (with Ehdr + Phdrs) */
    rodata  PT_LOAD FLAGS(4);               /* R */
    data    PT_LOAD FLAGS(6);               /* R + W */
    interp  PT_INTERP;
    dynamic PT_DYNAMIC;
}

SECTIONS
{
    . = SIZEOF_HEADERS;

    .interp : { *(.interp) } :interp :text

    .text : {
        *(.text .text.*)
        *(.plt .plt.* .iplt .iplt.*)
    } :text

    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.*) } :rodata
    /* Synthesized dynamic sections keep their own output sections so the
     * section types stay disjoint; all stay in the read-only phdr. */
    .gnu.hash      : { *(.gnu.hash) } :rodata
    .hash          : { *(.hash) } :rodata
    .dynsym        : { *(.dynsym) } :rodata
    .dynstr        : { *(.dynstr) } :rodata
    .gnu.version   : { *(.gnu.version) } :rodata
    .gnu.version_r : { *(.gnu.version_r) } :rodata
    .rela.dyn      : { *(.rela.dyn .rela.iplt) } :rodata
    .rela.plt      : { *(.rela.plt) } :rodata

    . = ALIGN(4096);
    .dynamic : { *(.dynamic) } :dynamic :data
    .got : { *(.got .got.*) } :data
    .data : { *(.data .data.*) } :data
    __llvm_prf_data : ALIGN(8) {
        __start___llvm_prf_data = .;
        KEEP(*(__llvm_prf_data))
        __stop___llvm_prf_data = .;
    } :data
    __llvm_prf_cnts : ALIGN(8) {
        __start___llvm_prf_cnts = .;
        KEEP(*(__llvm_prf_cnts))
        __stop___llvm_prf_cnts = .;
    } :data
    __llvm_prf_names : ALIGN(8) {
        __start___llvm_prf_names = .;
        KEEP(*(__llvm_prf_names))
        __stop___llvm_prf_names = .;
    } :data
    /* Init/fini arrays stay inside the RW segment ahead of .bss so their
     * linker-provided boundary symbols always address mapped memory. */
    .init_array : {
        __init_array_start = .;
        KEEP(*(.init_array .init_array.*))
        __init_array_end = .;
    } :data

    .fini_array : {
        __fini_array_start = .;
        KEEP(*(.fini_array .fini_array.*))
        __fini_array_end = .;
    } :data

    .bss : { *(.bss .bss.*) *(COMMON) } :data

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
    }
}
