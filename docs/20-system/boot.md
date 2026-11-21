# System and Software Requirements — BOOT

Domain `BOOT`: hardware startup, Multiboot2 higher-half entry, boot archive
load, and PID 1 initialization.

```{sys} Boot x86-64 kernel and start PID 1
:id: SYS_BOOT_0001
:status: active

The system must boot the x86-64 kernel in the higher half with Multiboot2. It
must initialize core CPU tables and timers, and start PID 1 (`sysproc`) in Ring
3.
```

```{swr} Initialize x86-64 platform tables and timer
:id: SWR_BOOT_0001
:status: active
:derives: SYS_BOOT_0001

The boot subsystem must parse the Multiboot2 information block. It must
initialize the serial console, GDT, IDT, TSS, and PIT-calibrated LAPIC timer
({need}`SYS_BOOT_0001`).
```

```{swr} Load boot archive ELF programs and give boot info
:id: SWR_BOOT_0002
:status: active
:derives: SYS_BOOT_0001

The boot subsystem must load user ELF executables from the `ustar` `boot.tar`
module with page permissions. It must map a read-only `boot_info_t` page for PID
1 through `AT_MICROKERNEL_BOOTINFO` ({need}`SYS_BOOT_0001`).
```

```{swr} Discover IOAPIC and serial resources via ACPI MADT and SPCR
:id: SWR_BOOT_0003
:status: draft
:derives: SYS_BOOT_0001
:traces: ADR_0003

The system must parse ACPI `MADT` IOAPIC bases and Interrupt Source Overrides
(ISOs) and `SPCR` when present (falling back to COM1). Ring 3 drivers
(`ttyserv`, `rtcdrv`) must accept discovered `argv` port and IRQ arguments and
own user `OPOST` TX with early boot fallback ({need}`SYS_BOOT_0001`).
```
