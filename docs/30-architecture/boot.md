# Architecture — BOOT

Domain `BOOT`: Structural architecture elements for x86-64 boot and ELF
initialization.

```{arc} Higher-half x86-64 boot and ELF initialization subsystem
:id: ARC_BOOT_0001
:status: active
:satisfies: SYS_BOOT_0001
:traces: ADR_0002

The boot subsystem transitions the CPU from 32-bit Multiboot2 entry to 64-bit
higher-half execution, initializes platform tables and timers, and loads initial
Ring 3 ELF programs from `boot.tar` to satisfy kernel bring-up
({need}`SYS_BOOT_0001`).
```
