# Verification — BOOT

Domain `BOOT`: Verification items for kernel boot, platform tables, and PID 1
start.

```{ver} Test of higher-half boot and PID 1 start
:id: VER_BOOT_0001
:status: active
:tags: integration
:verifies: SYS_BOOT_0001, SWR_BOOT_0001, SWR_BOOT_0002

The QEMU test harness `tests/test_runner.py` makes sure that the kernel boots in
the higher half and initializes the PMM, VMM, and APIC. This test also makes
sure that the kernel gives `bootinfo` to PID 1 (`sysproc`) on entry into Ring 3
({need}`SYS_BOOT_0001`, {need}`SWR_BOOT_0001`, {need}`SWR_BOOT_0002`).
```

```{ver} Test of ACPI MADT scan, argument-configured drivers, and OPOST TX
:id: VER_BOOT_0002
:status: active
:tags: integration
:verifies: SWR_BOOT_0003

The QEMU test harness `tests/test_runner.py` makes sure that the kernel finds
`ACPI MADT` IOAPIC and ISO entries. This test also does a check of `ttyserv`
(`-p 0x3f8 -i 4`) with `OPOST` and `ECHO` TX control, and `rtcdrv` (`-p 0x70 -i
8`) ({need}`SWR_BOOT_0003`).
```

```{ver} Test of x86-64 HAL boundaries across build, QEMU, and header checks
:id: VER_BOOT_0003
:status: active
:tags: integration
:verifies: SWR_BOOT_0004

This test compiles the project with `-DMICROKERNEL_ARCH=x86-64` and starts
`tests/test_runner.py --arch x86-64`. CTest `posix.headers` and
`posix.symbol_diff` also do checks with `--target x86_64-unknown-none-elf`
({need}`SWR_BOOT_0004`).
```

```{ver} Test of x86-64-v3 boot, XSAVE/XRSTOR protection, and scalar Ring 0
:id: VER_BOOT_0004
:status: active
:tags: integration
:verifies: SWR_BOOT_0005

The QEMU test harness starts `tests/simdtest/test_simd.c` to do a test of
`AVX2`, `FMA`, `BMI1`, `BMI2`, and `boot_info_t` indicators. This test makes
sure that the kernel isolates `YMM` and `MXCSR` state across threads and keeps
it across `fork`. CTest `boot.x86_64_v3` (`tests/boot/test_x86_64_v3.py`) does a
check of older CPU boot stop, scalar Ring 0 code, and Ring 3 `AVX2` and `FMA`
instructions ({need}`SWR_BOOT_0005`).
```
