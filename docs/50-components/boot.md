# Components — BOOT

Domain `BOOT`: White-box specifications for x86-64 platform startup and ELF
loading.

```{comp} x86-64 platform bring-up
:id: COMP_BOOT_0001
:status: active
:refines: ARC_BOOT_0001
:satisfies: SWR_BOOT_0001

`kernel/arch/x86_64/` (`boot.S`, `gdt.c`, `idt.c`, `apic.c`, `serial.c`) and
`kernel/core/multiboot.c` perform 32-to-64-bit higher-half entry, parse
Multiboot2 tags, and initialize the serial console, GDT, IDT, TSS, and LAPIC
timer for the boot subsystem ({need}`ARC_BOOT_0001`).
```

```{comp} Bootfs ustar reader and ELF loader
:id: COMP_BOOT_0002
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0001
:satisfies: SWR_BOOT_0002

`kernel/core/ustar.c`, `kernel/core/elf.c`, and `proc_spawn_init` unpack
`boot.tar`, map ELF `PT_LOAD` segments with W^X flags, and populate
`boot_info_t` ({need}`ICD_BOOT_0001`) for the boot subsystem
({need}`ARC_BOOT_0001`).
```
