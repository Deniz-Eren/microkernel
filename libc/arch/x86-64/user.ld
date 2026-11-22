/* Microkernel Ring 3 user-space linker script.
 * Base 0x400000 per the platform VA layout. Separate RX / R / RW segments so
 * the kernel ELF loader can enforce W^X per PHDR. .init_array/.fini_array are
 * retained for future C++ global constructor/destructor support. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5); /* R + X */
    rodata PT_LOAD FLAGS(4); /* R */
    data   PT_LOAD FLAGS(6); /* R + W */
}

SECTIONS
{
    . = 0x400000;

    /* Page-align each segment so no page mixes code, rodata and data: the
     * kernel ELF loader enforces W^X per page. */
    .text : { *(.text .text.*) } :text
    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.*) } :rodata
    . = ALIGN(4096);
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
    .bss : { *(.bss .bss.*) *(COMMON) } :data
    . = ALIGN(4096);

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

    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
        *(.comment)
        *(.gnu.*)
    }
}
