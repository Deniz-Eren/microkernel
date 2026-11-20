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
