# System and Software Requirements — BOOT

Domain `BOOT`: Hardware bring-up, Multiboot2 higher-half entry, boot archive
loading, and PID 1 initialization.

```{sys} Boot x86-64 kernel and launch PID 1
:id: SYS_BOOT_0001
:status: active

The system shall boot the x86-64 kernel in the higher half via Multiboot2,
initialize core CPU tables and timers, and launch PID 1 (`sysproc`) in Ring 3.
```

```{swr} Initialize x86-64 platform tables and timer
:id: SWR_BOOT_0001
:status: active
:derives: SYS_BOOT_0001

The boot subsystem shall parse the Multiboot2 information block and initialize
the serial console, GDT, IDT, TSS, and PIT-calibrated LAPIC timer to support
kernel startup ({need}`SYS_BOOT_0001`).
```

```{swr} Load boot archive ELF programs and hand off boot info
:id: SWR_BOOT_0002
:status: active
:derives: SYS_BOOT_0001

The boot subsystem shall load user ELF executables from the `ustar` `boot.tar`
module with page-level permissions and deliver a read-only `boot_info_t` page to
PID 1 through `AT_MICROKERNEL_BOOTINFO` ({need}`SYS_BOOT_0001`).
```
