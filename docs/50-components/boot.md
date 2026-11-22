# Components — BOOT

Domain `BOOT`: White-box specifications for x86-64 platform startup and ELF
loading.

```{comp} x86-64 platform bring-up
:id: COMP_BOOT_0001
:status: active
:refines: ARC_BOOT_0001
:satisfies: SWR_BOOT_0001

`kernel/arch/x86-64/` (`boot.S`, `gdt.c`, `idt.c`, `apic.c`, `serial.c`) and
`kernel/core/multiboot.c` do 32-to-64-bit entry and parse Multiboot2 tags. They
start the serial console, GDT, IDT, TSS, and LAPIC timer
({need}`ARC_BOOT_0001`).
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

```{comp} ACPI MADT/SPCR parser and parameterized driver startup
:id: COMP_BOOT_0003
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0001
:satisfies: SWR_BOOT_0003

`kernel/core/acpi.c` parses RSDP, RSDT/XSDT, `MADT` IOAPIC and ISO entries, and
optional `SPCR` tables. `kernel/core/intr.c` and `kernel/core/proc.c` apply ISO
flags, pass `-p`/`-i` arguments to `ttyserv` and `rtcdrv`, and support `init=sh`
interactive boot ({need}`ICD_BOOT_0001`, {need}`SWR_BOOT_0003`).
```

```{comp} x86-64 HAL backend and parameterized platform build
:id: COMP_BOOT_0004
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0002
:satisfies: SWR_BOOT_0004

`kernel/arch/x86-64/arch.c` implements the portable kernel HAL hooks for
`x86-64`. `cmake/arch/x86-64.cmake`, `libc/arch/x86-64/`, and driver `x86-64`
backends isolate platform code under `-DMICROKERNEL_ARCH=x86-64`
({need}`ICD_BOOT_0002`, {need}`SWR_BOOT_0004`).
```
