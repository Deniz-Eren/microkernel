# Architecture — BOOT

Domain `BOOT`: Architecture items for kernel boot and ELF load.

```{arc} Higher-half kernel boot and ELF loader subsystem
:id: ARC_BOOT_0001
:status: active
:satisfies: SYS_BOOT_0001
:traces: ADR_0002

The boot subsystem moves the CPU from 32-bit Multiboot2 entry to 64-bit
higher-half mode and starts platform timers. It loads the initial Ring 3 ELF
executables from `boot.tar` ({need}`SYS_BOOT_0001`).
```

## Boot Sequence

The boot sequence moves from GRUB Multiboot2 32-bit entry through 64-bit
higher-half page tables to Ring 3 server start.

```mermaid
flowchart TD
    GRUB["GRUB Multiboot2 (32-bit Protected Mode)"]
    BOOTS["boot.S (4-Level Page Tables and Long Mode)"]
    KMAIN["kmain (GDT, IDT, COM1, PMM, VMM, HHDM, kheap)"]
    ACPI["ACPI RSDP, MADT (IOAPIC and ISO), and SPCR Tables"]
    APIC["LAPIC Timer and ISO IOAPIC Configuration"]
    TAR["ustar boot.tar ELF Loader"]
    INIT["Start sysproc (110), ttyserv -p/-i (105), Servers, and rtcdrv (100)"]
    MODE["Command Line init=sh -> /boot/sh | Usual -> Test Suite"]
    SCHED["sched_activate (Enter Ring 3 with iretq)"]

    GRUB --> BOOTS --> KMAIN --> ACPI --> APIC
    APIC --> TAR --> INIT --> MODE --> SCHED
```
