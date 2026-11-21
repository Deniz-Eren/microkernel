# Interfaces — BOOT

Domain `BOOT`: Boot information handoff contracts between kernel and PID 1.

```{icd} Boot information handoff interface
:id: ICD_BOOT_0001
:status: active
:refines: ARC_BOOT_0001
:satisfies: SWR_BOOT_0002

`libc/include/sys/bootinfo.h` specifies `boot_info_t`, `BOOTINFO_MAGIC`,
`BOOTINFO_VA` (`0x10000000`), and `AT_MICROKERNEL_BOOTINFO`. It passes bootfs
and ACPI metadata to PID 1 ({need}`ARC_BOOT_0001`, {need}`SWR_BOOT_0002`).
```
