# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0062 — Build shared libc.so and unify dlopen/dlsym with ld.so

```{task} Build shared libc.so and unify dlopen/dlsym with ld.so
:id: TSK_0062
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001, SYS_MEM_0001

Build `-fPIC` shared `libc.so` alongside static `libc.a` into the Ring 3 sysroot
and `boot.tar`. Update `crt0.S` and `__libc_start_main` handoff, add a dynamic
executable linker script with `FILEHDR PHDRS`, and delegate `<dlfcn.h>` runtime
calls to `ld.so` when loaded dynamically. Verify `stdio`, `malloc`, pthreads,
TLS `errno`, signals, `fork`/`execve`, and `dlopen` in QEMU
({need}`SYS_LIBC_0001`, {need}`SYS_MEM_0001`).
```

### TSK_0063 — Build shared libz.so.1 via CMake and link zlibtest dynamically

```{task} Build shared libz.so.1 via CMake and link zlibtest dynamically
:id: TSK_0063
:status: open
:traces: BDR_0003, BDR_0004, ADR_0055, SYS_LIBC_0001

Update `cmake/zlib.cmake` to build shared `libz.so.1` (with `libz.so` symlink)
alongside `libz.a` during the CMake build without storing upstream source in
git. Install `libz.so.1` into the Ring 3 sysroot and `boot.tar`. Link `zlibtest`
dynamically against `libz.so.1` and `libc.so`, and verify in QEMU
({need}`SYS_LIBC_0001`).
```

### TSK_0064 — Build shared libcrypto.so.3 and libssl.so.3 via CMake

```{task} Build shared libcrypto.so.3 and libssl.so.3 via CMake
:id: TSK_0064
:status: open
:traces: BDR_0003, BDR_0004, ADR_0056, SYS_LIBC_0001

Update `cmake/openssl_target.conf`, `cmake/build_openssl.sh`, and
`cmake/openssl.cmake` to build `-fPIC` shared `libcrypto.so.3` and `libssl.so.3`
alongside static archives during the CMake build without storing upstream source
in git. Install shared libraries into the sysroot and `boot.tar`, link
`openssltest` dynamically, and verify cryptography and TLS 1.3 in QEMU
({need}`SYS_LIBC_0001`).
```

### TSK_0065 — Dynamically link OpenSSH Portable against shared libs

```{task} Dynamically link OpenSSH Portable against shared libs
:id: TSK_0065
:status: open
:traces: BDR_0003, BDR_0004, ADR_0054, ADR_0055, ADR_0056, SYS_IPC_0001

Update `cmake/openssh.cmake` to link `sshd`, `sshd-session`, and `ssh-keygen`
dynamically with `--dynamic-linker=/boot/ld.so` against `libcrypto.so.3`,
`libz.so.1`, and `libc.so`. Verify `PT_INTERP` and `DT_NEEDED` headers via
`llvm-readelf` and verify RSA, ECDSA, Ed25519, compressed (`zlib@openssh.com`),
password, and PTY sessions in CTest `ipc.sshd_e2e` ({need}`SYS_IPC_0001`).
```

### TSK_0066 — Dynamically link Ring 3 bin/ shell and utilities against libc.so

```{task} Dynamically link Ring 3 bin/ shell and utilities against libc.so
:id: TSK_0066
:status: open
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001, SYS_PROC_0001

Convert `bin/` (`sh`, XCU utilities, `nc`, `ping`, `sshd-setup`) to dynamic
executables linked against `ld.so` and `libc.so` while keeping core Ring 3 boot
servers statically linked. Verify `boot.tar` size reduction and pass the full
QEMU and CTest suites ({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

### TSK_0067 — Introduce ConsoleBackend and QEMU raspi4b ARM64 boot harness

```{task} Introduce ConsoleBackend and QEMU raspi4b ARM64 boot harness
:id: TSK_0067
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024, SYS_BOOT_0001

Introduce a compile-time `ConsoleBackend` abstraction in the kernel and route
early console output through it. Add the `aarch64` QEMU `-M raspi4b` boot path
with the PL011 UART (`0xFE201000`, `-serial stdio`) as the first backend. Extend
the QEMU test harness to assert ARM64 boot and serial output ({need}`ADR_0058`,
{need}`WDR_0011`, {need}`ADR_0024`, {need}`SYS_BOOT_0001`).
```

### TSK_0068 — Implement core ARM64 BCM2711 architecture support

```{task} Implement core ARM64 BCM2711 architecture support
:id: TSK_0068
:status: open
:traces: ADR_0024, WDR_0011, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001,
    SYS_IPC_0001

Implement core ARM64 (BCM2711) architecture support. Cover the `EL2` to `EL1`
transition, MMU page tables, GIC-400 interrupt handling, generic timers, and the
scheduler and IPC paths. Verify the port in QEMU `-M raspi4b` through the
existing test suite ({need}`ADR_0024`, {need}`WDR_0011`, {need}`SYS_BOOT_0001`,
{need}`SYS_MEM_0001`, {need}`SYS_PROC_0001`, {need}`SYS_IPC_0001`).
```

### TSK_0069 — Implement platform-agnostic USB CDC-ACM protocol state machine

```{task} Implement platform-agnostic USB CDC-ACM protocol state machine
:id: TSK_0069
:status: open
:traces: ADR_0058, WDR_0011

Implement the platform-agnostic USB 2.0 and CDC-ACM protocol state machine:
device, configuration, and string descriptors, the `EP0` `SETUP` state machine,
and TX/RX ring buffers. Add host and QEMU unit tests for enumeration state
transitions. Implement the USB support for both `x86-64` and `arm64`, since
neither architecture has a USB driver today ({need}`ADR_0058`,
{need}`WDR_0011`).
```

### TSK_0070 — Implement reusable DWC2 PIO core driver and BCM2711 board glue

```{task} Implement reusable DWC2 PIO core driver and BCM2711 board glue
:id: TSK_0070
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024

Implement the reusable Synopsys DWC2 core register driver in slave/PIO mode with
core soft-reset, FIFO partitioning, interrupt dispatch, and PIO packet push and
pop. Add the BCM2711 MMIO board glue at `0xFE980000` behind
`EARLY_USB_CDC_CONSOLE`. Register the new Ring 0 console modules in
`tests/arch/microkernel_allowlist.json` with `ADR_0058` as justification
({need}`ADR_0058`, {need}`WDR_0011`, {need}`ADR_0024`).
```

### TSK_0071 — Build board-configurable host HIL runner in tools/hil/

```{task} Build board-configurable host HIL runner in tools/hil/
:id: TSK_0071
:status: open
:traces: WDR_0011, ADR_0058

Build the board-configurable host HIL runner under `tools/hil/`. Add pluggable
power backends (`uhubctl`), image deployers (`rpiboot`), and telemetry
collectors (`usbmon`, `lsusb`, and the console device). Add the
`boards/rpi400.yaml` board configuration ({need}`WDR_0011`, {need}`ADR_0058`).
```

### TSK_0072 — Perform one-time Pi 400 provisioning over SSH

```{task} Perform one-time Pi 400 provisioning over SSH
:id: TSK_0072
:status: open
:traces: WDR_0011

Perform the one-time Pi 400 provisioning over SSH while the flash disk is still
active. Verify `uhubctl` power switching on the host, set `BOOT_ORDER=0xf31`
with `rpi-eeprom-config`, and harvest `start4.elf`, `fixup4.dat`,
`bcm2711-rpi-400.dtb`, and `overlays/dwc2.dtbo` to the host. Power off and
remove the flash disk. Neither SSH nor the SD card is used again
({need}`WDR_0011`).
```

### TSK_0073 — Validate DWC2 EP0 enumeration and descriptor breadcrumbs

```{task} Validate DWC2 EP0 enumeration and descriptor breadcrumbs
:id: TSK_0073
:status: open
:traces: ADR_0058, WDR_0011

Validate DWC2 peripheral `EP0` enumeration on the real Pi 400 through the
`tools/hil/` loop. Confirm the PHY and reset handshake in host `dmesg`. Trace
raw `EP0` `SETUP` traffic with `usbmon`. Verify `bcdDevice` and string
descriptor breadcrumbs with `lsusb -v` ({need}`ADR_0058`, {need}`WDR_0011`).
```

### TSK_0074 — Validate CDC-ACM console and start bare-metal driver work

```{task} Validate CDC-ACM console and start bare-metal driver work
:id: TSK_0074
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024

Validate the CDC-ACM `/dev/ttyACM0` bidirectional console on the real Pi 400
after `SET_CONFIGURATION` with the CDC communication and data interfaces. Begin
autonomous bare-metal driver development through the live console. Start with
the VideoCore Mailbox HDMI framebuffer, the PCIe root complex, and the VL805
xHCI host controller ({need}`ADR_0058`, {need}`WDR_0011`, {need}`ADR_0024`).
```
