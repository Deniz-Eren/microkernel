# Components — BOOT

The `BOOT` domain includes internal specifications for `x86-64` platform start
and ELF loader components.

```{comp} x86-64 platform initialization
:id: COMP_BOOT_0001
:status: active
:refines: ARC_BOOT_0001
:satisfies: SWR_BOOT_0001

`kernel/arch/x86-64/` (`boot.S`, `gdt.c`, `idt.c`, `apic.c`, `serial.c`) and
`kernel/core/multiboot.c` do 32-to-64-bit entry and parse Multiboot2 records.
They initialize the serial console, GDT, IDT, TSS, and LAPIC timer
({need}`ARC_BOOT_0001`).
```

```{comp} Bootfs ustar reader and ELF loader
:id: COMP_BOOT_0002
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0001
:satisfies: SWR_BOOT_0002

`kernel/core/ustar.c`, `kernel/core/elf.c`, and `proc_spawn_init` read
`boot.tar`, map ELF `PT_LOAD` segments with W^X flags, and write `boot_info_t`
({need}`ICD_BOOT_0001`) for the `BOOT` subsystem ({need}`ARC_BOOT_0001`).
```

```{comp} ACPI MADT/SPCR parser and driver parameter start
:id: COMP_BOOT_0003
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0001
:satisfies: SWR_BOOT_0003

`kernel/core/acpi.c` parses RSDP, RSDT/XSDT, `MADT` IOAPIC and ISO entries, and
optional `SPCR` tables. `kernel/core/intr.c` and `kernel/core/proc.c` use ISO
flags, give `-p` and `-i` options to `ttyserv` and `rtcdrv`, and give the
`init=sh` interactive boot process ({need}`ICD_BOOT_0001`,
{need}`SWR_BOOT_0003`).
```

```{comp} x86-64 HAL backend and platform build configuration
:id: COMP_BOOT_0004
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0002
:satisfies: SWR_BOOT_0004

`kernel/arch/x86-64/arch.c` gives the standard kernel HAL functions for
`x86-64`. `cmake/arch/x86-64.cmake`, `libc/arch/x86-64/`, and driver `x86-64`
backends isolate platform code with `-DMICROKERNEL_ARCH=x86-64`
({need}`ICD_BOOT_0002`, {need}`SWR_BOOT_0004`).
```

```{comp} x86-64-v3 CPUID check, XSAVE/XRSTOR state change, and AVX2 user space
:id: COMP_BOOT_0005
:status: active
:refines: ARC_BOOT_0001, ICD_BOOT_0001, ICD_BOOT_0002
:satisfies: SWR_BOOT_0005

`kernel/arch/x86-64/arch.c` examines `x86-64-v3` flags with `CPUID` during the
initial boot process, stops older CPUs, sets `CR4.OSXSAVE` and `XCR0` (with
optional `AVX-512`), and gives the `XSAVE` length. `kernel/arch/x86-64/switch.S`
and `kernel/core/sched.c` change 64-byte-aligned `XSAVE` areas. Ring 3 compiles
with `-march=x86-64-v3`, but Ring 0 does not use SIMD instructions
({need}`ICD_BOOT_0001`, {need}`ICD_BOOT_0002`, {need}`SWR_BOOT_0005`).
```
