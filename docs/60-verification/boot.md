# Verification — BOOT

Domain `BOOT`: Verification items covering kernel boot, platform tables, and PID
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

```{ver} Verify x86-64 HAL isolation across build, QEMU, and header gates
:id: VER_BOOT_0003
:status: active
:tags: integration
:verifies: SWR_BOOT_0004

Verified by building with `-DMICROKERNEL_ARCH=x86-64`, running
`tests/test_runner.py --arch x86-64`, and running CTest `posix.headers` and
`posix.symbol_diff` with `--target x86_64-unknown-none-elf`
({need}`SWR_BOOT_0004`).
```

```{ver} Verify x86-64-v3 boot gate, XSAVE/XRSTOR isolation, and scalar Ring 0
:id: VER_BOOT_0004
:status: active
:tags: integration
:verifies: SWR_BOOT_0005

Verified by `tests/simdtest/test_simd.c` in headless QEMU (`AVX2`, `FMA`,
`BMI1`/`BMI2`, `boot_info_t` flags, preemptive `YMM`/`MXCSR` thread isolation,
and `fork` inheritance) and CTest `boot.x86_64_v3`
(`tests/boot/test_x86_64_v3.py`, legacy CPU boot halt, scalar Ring 0 audit, and
Ring 3 `AVX2`/`FMA` audit) ({need}`SWR_BOOT_0005`).
```
