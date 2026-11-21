# Verification — BOOT

Domain `BOOT`: Verification items covering x86-64 boot, platform tables, and PID
1 startup.

```{ver} Verify higher-half boot and PID 1 launch
:id: VER_BOOT_0001
:status: active
:tags: integration
:verifies: SYS_BOOT_0001, SWR_BOOT_0001, SWR_BOOT_0002

Verified by the headless QEMU serial-output harness `tests/test_runner.py`,
confirming higher-half boot, PMM/VMM/APIC initialization, `bootinfo` handoff,
and PID 1 (`sysproc`) entry into Ring 3 ({need}`SYS_BOOT_0001`,
{need}`SWR_BOOT_0001`, {need}`SWR_BOOT_0002`).
```

```{ver} Verify ACPI MADT discovery, parameterized drivers, and OPOST TX
:id: VER_BOOT_0002
:status: active
:tags: integration
:verifies: SWR_BOOT_0003

The headless QEMU harness `tests/test_runner.py` checks `ACPI MADT` IOAPIC and
ISO discovery, parameterized `ttyserv` (`-p 0x3f8 -i 4`) with `OPOST`/`ECHO` TX
ownership, and parameterized `rtcdrv` (`-p 0x70 -i 8`) ({need}`SWR_BOOT_0003`).
```
