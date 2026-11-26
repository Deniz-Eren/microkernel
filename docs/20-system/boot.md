# System and Software Requirements — BOOT

The `BOOT` domain includes hardware start, Multiboot2 higher-half entry,
`boot.tar` archive access, and the start of PID 1.

```{sys} Boot 64-bit kernel and start PID 1
:id: SYS_BOOT_0001
:status: active

The system must boot the 64-bit kernel in the higher half. It must initialize
the primary CPU tables and timers, and start PID 1 (`sysproc`) in Ring 3.
```

```{swr} Initialize x86-64 platform tables and timer
:id: SWR_BOOT_0001
:status: active
:derives: SYS_BOOT_0001

The `BOOT` subsystem must parse the Multiboot2 information block. It must
initialize the serial console, GDT, IDT, and TSS, and calibrate the LAPIC timer
with the PIT ({need}`SYS_BOOT_0001`).
```

```{swr} Load ELF executables from boot archive and give boot info
:id: SWR_BOOT_0002
:status: active
:derives: SYS_BOOT_0001

The `BOOT` subsystem must load user ELF executables from the `ustar` `boot.tar`
module with page permissions. It must map a read-only `boot_info_t` page for PID
1 through `AT_MICROKERNEL_BOOTINFO` ({need}`SYS_BOOT_0001`).
```

```{swr} Find IOAPIC and serial resources through ACPI MADT and SPCR
:id: SWR_BOOT_0003
:status: active
:derives: SYS_BOOT_0001
:traces: ADR_0003, ADR_0022

The system must parse ACPI `MADT` IOAPIC addresses, Interrupt Source Overrides
(ISOs), and `SPCR` when available, and use COM1 as an alternative. Ring 3
drivers (`ttyserv`, `rtcdrv`) must accept port and IRQ `argv` values and control
user `OPOST` TX, with an initial console path as an alternative
({need}`SYS_BOOT_0001`).
```

```{swr} Isolate x86-64 platform code behind arch/x86-64 HAL boundaries
:id: SWR_BOOT_0004
:status: active
:derives: SYS_BOOT_0001
:traces: ADR_0024

The build system, kernel, `libc`, and hardware drivers must isolate `x86-64`
code in `x86-64` directories that `-DMICROKERNEL_ARCH=x86-64` selects. The
kernel, `libc`, and Ring 3 servers must use standard HAL interfaces. As a
result, new `arm64` code can connect without changes to general code
({need}`SYS_BOOT_0001`).
```

```{swr} Examine x86-64-v3 CPU flags, change XSAVE state, and find AVX-512
:id: SWR_BOOT_0005
:status: active
:derives: SYS_BOOT_0001
:traces: ADR_0024, ADR_0026

On `x86-64`, the initial boot process must do a check of `x86-64-v3` (`XSAVE`,
`AVX`, `AVX2`, `FMA`, `BMI1`, `BMI2`) with `CPUID` and stop with an error on
older CPUs. The kernel must set `CR4.OSXSAVE` and `XCR0`, include optional
`AVX-512` when available, and change extended CPU state with `XSAVE` and
`XRSTOR`. Ring 3 must compile with `-march=x86-64-v3`, but Ring 0 must not use
SIMD instructions ({need}`SYS_BOOT_0001`).
```
