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

### TSK_0075 — Implement native ldd in /boot/ld.so and ldconfig in bin/

```{task} Implement native ldd in /boot/ld.so and ldconfig in bin/
:id: TSK_0075
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001

Extend `/boot/ld.so` to detect `argv[0]` ending in `ldd` (or
`LD_TRACE_LOADED_OBJECTS=1`). Resolve `DT_NEEDED` dependencies across
`/etc/ld.so.conf` and the standard library paths. Print `libname => /path
(0xaddr)` without transferring control to `_start`. Install `/usr/bin/ldd` as a
symlink to `/boot/ld.so`. Implement native `/usr/sbin/ldconfig` in `bin/`
(dynamically linked against `libc.so`) to maintain `DT_SONAME` symlinks and exit
cleanly for build scripts. Verify against `zlibtest`, `openssltest`, and `sshd`
in QEMU ({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
```

### TSK_0076 — Implement getent and built-in C.UTF-8 locale support

```{task} Implement getent and built-in C.UTF-8 locale support
:id: TSK_0076
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, SYS_LIBC_0001

Implement `/usr/bin/getent` in Ring 3 `bin/` (dynamically linked against
`libc.so`). Support `passwd`, `group`, `hosts`, `services`, and `protocols`
lookups and enumeration via the existing `libc` NSS functions (`getpwnam`,
`getgrnam`, `getaddrinfo`, `getservbyname`, `getprotobyname`). Use minimal
`/etc/nsswitch.conf` defaults. Ensure `libc` (`libc.so` and `libc.a`) natively
supports `"C"` and `"C.UTF-8"` in `setlocale()` and `nl_langinfo()` without
requiring external binary `LC_*` files in `boot.tar`. Verify in QEMU and CTest
({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
```

### TSK_0077 — Build IANA tzcode (zic, zdump, tzselect) via CMake

```{task} Build IANA tzcode (zic, zdump, tzselect) via CMake
:id: TSK_0077
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054, SYS_LIBC_0001

Add `cmake/tzcode.cmake` to fetch and cross-compile public-domain IANA `tzcode`
(`zic` and `zdump`, dynamically linked against `libc.so`). Install `tzselect`
into the Ring 3 sysroot and `boot.tar`. Do not store upstream source in git
({need}`WDR_0012`, {need}`BDR_0003`, {need}`BDR_0004`). Record the notice in
`docs/THIRD-PARTY-NOTICES.md`. Verify `TZif` compilation and `zdump` inspection
in QEMU and CTest ({need}`SYS_LIBC_0001`).
```

### TSK_0078 — Build tar (libarchive/bsdtar) via CMake and link dynamically

```{task} Build tar (libarchive/bsdtar) via CMake and link dynamically
:id: TSK_0078
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054, SYS_LIBC_0001

Add `cmake/tar.cmake` to fetch and cross-compile `tar` (`libarchive` `bsdtar` or
GNU `tar`), linking dynamically against `libc.so` and `libz.so.1` with
`--dynamic-linker=/boot/ld.so`. Do not store upstream source in git. Update
`docs/THIRD-PARTY-NOTICES.md`. Install `/usr/bin/tar` into the Ring 3 sysroot
and `boot.tar`. Verify `PT_INTERP` and `DT_NEEDED` via `ldd` and `llvm-readelf`.
Test `.tar` and `.tar.gz` creation and extraction in QEMU and CTest
({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
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

### TSK_0079 — Implement and test `.` (POSIX.1-2024)

```{task} Implement and test `.` (POSIX.1-2024)
:id: TSK_0079
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `.` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dot.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0080 — Implement and test `:` (POSIX.1-2024)

```{task} Implement and test `:` (POSIX.1-2024)
:id: TSK_0080
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `:` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/colon.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0081 — Implement and test `admin` (POSIX.1-2024)

```{task} Implement and test `admin` (POSIX.1-2024)
:id: TSK_0081
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `admin` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/admin.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0082 — Implement and test `alias` (POSIX.1-2024)

```{task} Implement and test `alias` (POSIX.1-2024)
:id: TSK_0082
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `alias` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/alias.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0083 — Implement and test `ar` (POSIX.1-2024)

```{task} Implement and test `ar` (POSIX.1-2024)
:id: TSK_0083
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ar` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ar.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the SD feature group, so gate it behind the feature
macro.
```

### TSK_0084 — Implement and test `asa` (POSIX.1-2024)

```{task} Implement and test `asa` (POSIX.1-2024)
:id: TSK_0084
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `asa` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/asa.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the FR feature group, so gate it behind the feature
macro.
```

### TSK_0085 — Implement and test `at` (POSIX.1-2024)

```{task} Implement and test `at` (POSIX.1-2024)
:id: TSK_0085
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `at` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/at.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0086 — Implement and test `awk` (POSIX.1-2024)

```{task} Implement and test `awk` (POSIX.1-2024)
:id: TSK_0086
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `awk` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/awk.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0087 — Implement and test `basename` (POSIX.1-2024)

```{task} Implement and test `basename` (POSIX.1-2024)
:id: TSK_0087
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `basename` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/basename.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0088 — Implement and test `batch` (POSIX.1-2024)

```{task} Implement and test `batch` (POSIX.1-2024)
:id: TSK_0088
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `batch` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/batch.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0089 — Implement and test `bc` (POSIX.1-2024)

```{task} Implement and test `bc` (POSIX.1-2024)
:id: TSK_0089
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `bc` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/bc.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0090 — Implement and test `bg` (POSIX.1-2024)

```{task} Implement and test `bg` (POSIX.1-2024)
:id: TSK_0090
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `bg` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/bg.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0091 — Implement and test `c17` (POSIX.1-2024)

```{task} Implement and test `c17` (POSIX.1-2024)
:id: TSK_0091
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `c17` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/c17.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0092 — Implement and test `cal` (POSIX.1-2024)

```{task} Implement and test `cal` (POSIX.1-2024)
:id: TSK_0092
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cal` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cal.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0093 — Implement and test `cat` (POSIX.1-2024)

```{task} Implement and test `cat` (POSIX.1-2024)
:id: TSK_0093
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cat` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cat.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0094 — Implement and test `cd` (POSIX.1-2024)

```{task} Implement and test `cd` (POSIX.1-2024)
:id: TSK_0094
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cd` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cd.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0095 — Implement and test `cflow` (POSIX.1-2024)

```{task} Implement and test `cflow` (POSIX.1-2024)
:id: TSK_0095
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cflow` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cflow.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0096 — Implement and test `chgrp` (POSIX.1-2024)

```{task} Implement and test `chgrp` (POSIX.1-2024)
:id: TSK_0096
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `chgrp` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chgrp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0097 — Implement and test `chmod` (POSIX.1-2024)

```{task} Implement and test `chmod` (POSIX.1-2024)
:id: TSK_0097
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `chmod` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chmod.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0098 — Implement and test `chown` (POSIX.1-2024)

```{task} Implement and test `chown` (POSIX.1-2024)
:id: TSK_0098
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `chown` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chown.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0099 — Implement and test `cksum` (POSIX.1-2024)

```{task} Implement and test `cksum` (POSIX.1-2024)
:id: TSK_0099
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cksum` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cksum.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0100 — Implement and test `cmp` (POSIX.1-2024)

```{task} Implement and test `cmp` (POSIX.1-2024)
:id: TSK_0100
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cmp` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cmp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0101 — Implement and test `comm` (POSIX.1-2024)

```{task} Implement and test `comm` (POSIX.1-2024)
:id: TSK_0101
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `comm` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/comm.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0102 — Implement and test `command` (POSIX.1-2024)

```{task} Implement and test `command` (POSIX.1-2024)
:id: TSK_0102
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `command` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/command.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0103 — Implement and test `compress` (POSIX.1-2024)

```{task} Implement and test `compress` (POSIX.1-2024)
:id: TSK_0103
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `compress` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/compress.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0104 — Implement and test `cp` (POSIX.1-2024)

```{task} Implement and test `cp` (POSIX.1-2024)
:id: TSK_0104
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cp` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0105 — Implement and test `crontab` (POSIX.1-2024)

```{task} Implement and test `crontab` (POSIX.1-2024)
:id: TSK_0105
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `crontab` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/crontab.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0106 — Implement and test `csplit` (POSIX.1-2024)

```{task} Implement and test `csplit` (POSIX.1-2024)
:id: TSK_0106
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `csplit` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/csplit.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0107 — Implement and test `ctags` (POSIX.1-2024)

```{task} Implement and test `ctags` (POSIX.1-2024)
:id: TSK_0107
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ctags` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ctags.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0108 — Implement and test `cut` (POSIX.1-2024)

```{task} Implement and test `cut` (POSIX.1-2024)
:id: TSK_0108
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cut` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cut.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0109 — Implement and test `cxref` (POSIX.1-2024)

```{task} Implement and test `cxref` (POSIX.1-2024)
:id: TSK_0109
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `cxref` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cxref.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0110 — Implement and test `date` (POSIX.1-2024)

```{task} Implement and test `date` (POSIX.1-2024)
:id: TSK_0110
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `date` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/date.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0111 — Implement and test `dd` (POSIX.1-2024)

```{task} Implement and test `dd` (POSIX.1-2024)
:id: TSK_0111
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `dd` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dd.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0112 — Implement and test `delta` (POSIX.1-2024)

```{task} Implement and test `delta` (POSIX.1-2024)
:id: TSK_0112
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `delta` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/delta.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0113 — Implement and test `df` (POSIX.1-2024)

```{task} Implement and test `df` (POSIX.1-2024)
:id: TSK_0113
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `df` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/df.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0114 — Implement and test `diff` (POSIX.1-2024)

```{task} Implement and test `diff` (POSIX.1-2024)
:id: TSK_0114
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `diff` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/diff.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0115 — Implement and test `dirname` (POSIX.1-2024)

```{task} Implement and test `dirname` (POSIX.1-2024)
:id: TSK_0115
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `dirname` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dirname.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0116 — Implement and test `du` (POSIX.1-2024)

```{task} Implement and test `du` (POSIX.1-2024)
:id: TSK_0116
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `du` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/du.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0117 — Implement and test `echo` (POSIX.1-2024)

```{task} Implement and test `echo` (POSIX.1-2024)
:id: TSK_0117
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `echo` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/echo.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0118 — Implement and test `ed` (POSIX.1-2024)

```{task} Implement and test `ed` (POSIX.1-2024)
:id: TSK_0118
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ed` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ed.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0119 — Implement and test `env` (POSIX.1-2024)

```{task} Implement and test `env` (POSIX.1-2024)
:id: TSK_0119
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `env` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/env.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0120 — Implement and test `ex` (POSIX.1-2024)

```{task} Implement and test `ex` (POSIX.1-2024)
:id: TSK_0120
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ex` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ex.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0121 — Implement and test `expand` (POSIX.1-2024)

```{task} Implement and test `expand` (POSIX.1-2024)
:id: TSK_0121
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `expand` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/expand.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0122 — Implement and test `expr` (POSIX.1-2024)

```{task} Implement and test `expr` (POSIX.1-2024)
:id: TSK_0122
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `expr` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/expr.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0123 — Implement and test `false` (POSIX.1-2024)

```{task} Implement and test `false` (POSIX.1-2024)
:id: TSK_0123
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `false` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/false.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0124 — Implement and test `fc` (POSIX.1-2024)

```{task} Implement and test `fc` (POSIX.1-2024)
:id: TSK_0124
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `fc` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fc.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0125 — Implement and test `fg` (POSIX.1-2024)

```{task} Implement and test `fg` (POSIX.1-2024)
:id: TSK_0125
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `fg` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fg.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0126 — Implement and test `file` (POSIX.1-2024)

```{task} Implement and test `file` (POSIX.1-2024)
:id: TSK_0126
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `file` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/file.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0127 — Implement and test `find` (POSIX.1-2024)

```{task} Implement and test `find` (POSIX.1-2024)
:id: TSK_0127
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `find` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/find.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0128 — Implement and test `fold` (POSIX.1-2024)

```{task} Implement and test `fold` (POSIX.1-2024)
:id: TSK_0128
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `fold` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fold.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0129 — Implement and test `fuser` (POSIX.1-2024)

```{task} Implement and test `fuser` (POSIX.1-2024)
:id: TSK_0129
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `fuser` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fuser.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0130 — Implement and test `gencat` (POSIX.1-2024)

```{task} Implement and test `gencat` (POSIX.1-2024)
:id: TSK_0130
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `gencat` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/gencat.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0131 — Implement and test `get` (POSIX.1-2024)

```{task} Implement and test `get` (POSIX.1-2024)
:id: TSK_0131
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `get` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/get.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0132 — Implement and test `getconf` (POSIX.1-2024)

```{task} Implement and test `getconf` (POSIX.1-2024)
:id: TSK_0132
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `getconf` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/getconf.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0133 — Implement and test `getopts` (POSIX.1-2024)

```{task} Implement and test `getopts` (POSIX.1-2024)
:id: TSK_0133
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `getopts` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/getopts.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0134 — Implement and test `gettext` (POSIX.1-2024)

```{task} Implement and test `gettext` (POSIX.1-2024)
:id: TSK_0134
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `gettext` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/gettext.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0135 — Implement and test `grep` (POSIX.1-2024)

```{task} Implement and test `grep` (POSIX.1-2024)
:id: TSK_0135
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `grep` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/grep.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0136 — Implement and test `hash` (POSIX.1-2024)

```{task} Implement and test `hash` (POSIX.1-2024)
:id: TSK_0136
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `hash` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/hash.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0137 — Implement and test `head` (POSIX.1-2024)

```{task} Implement and test `head` (POSIX.1-2024)
:id: TSK_0137
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `head` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/head.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0138 — Implement and test `iconv` (POSIX.1-2024)

```{task} Implement and test `iconv` (POSIX.1-2024)
:id: TSK_0138
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `iconv` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/iconv.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0139 — Implement and test `id` (POSIX.1-2024)

```{task} Implement and test `id` (POSIX.1-2024)
:id: TSK_0139
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `id` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/id.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0140 — Implement and test `ipcrm` (POSIX.1-2024)

```{task} Implement and test `ipcrm` (POSIX.1-2024)
:id: TSK_0140
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ipcrm` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ipcrm.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0141 — Implement and test `ipcs` (POSIX.1-2024)

```{task} Implement and test `ipcs` (POSIX.1-2024)
:id: TSK_0141
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ipcs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ipcs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0142 — Implement and test `jobs` (POSIX.1-2024)

```{task} Implement and test `jobs` (POSIX.1-2024)
:id: TSK_0142
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `jobs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/jobs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0143 — Implement and test `join` (POSIX.1-2024)

```{task} Implement and test `join` (POSIX.1-2024)
:id: TSK_0143
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `join` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/join.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0144 — Implement and test `kill` (POSIX.1-2024)

```{task} Implement and test `kill` (POSIX.1-2024)
:id: TSK_0144
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `kill` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/kill.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0145 — Implement and test `lex` (POSIX.1-2024)

```{task} Implement and test `lex` (POSIX.1-2024)
:id: TSK_0145
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `lex` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/lex.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the CD feature group, so gate it behind the feature
macro.
```

### TSK_0146 — Implement and test `link` (POSIX.1-2024)

```{task} Implement and test `link` (POSIX.1-2024)
:id: TSK_0146
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `link` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/link.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0147 — Implement and test `ln` (POSIX.1-2024)

```{task} Implement and test `ln` (POSIX.1-2024)
:id: TSK_0147
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ln` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ln.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0148 — Implement and test `locale` (POSIX.1-2024)

```{task} Implement and test `locale` (POSIX.1-2024)
:id: TSK_0148
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `locale` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/locale.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0149 — Implement and test `localedef` (POSIX.1-2024)

```{task} Implement and test `localedef` (POSIX.1-2024)
:id: TSK_0149
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `localedef` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/localedef.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0150 — Implement and test `logger` (POSIX.1-2024)

```{task} Implement and test `logger` (POSIX.1-2024)
:id: TSK_0150
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `logger` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/logger.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0151 — Implement and test `logname` (POSIX.1-2024)

```{task} Implement and test `logname` (POSIX.1-2024)
:id: TSK_0151
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `logname` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/logname.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0152 — Implement and test `lp` (POSIX.1-2024)

```{task} Implement and test `lp` (POSIX.1-2024)
:id: TSK_0152
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `lp` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/lp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0153 — Implement and test `ls` (POSIX.1-2024)

```{task} Implement and test `ls` (POSIX.1-2024)
:id: TSK_0153
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ls` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ls.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0154 — Implement and test `m4` (POSIX.1-2024)

```{task} Implement and test `m4` (POSIX.1-2024)
:id: TSK_0154
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `m4` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/m4.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0155 — Implement and test `mailx` (POSIX.1-2024)

```{task} Implement and test `mailx` (POSIX.1-2024)
:id: TSK_0155
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `mailx` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mailx.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0156 — Implement and test `make` (POSIX.1-2024)

```{task} Implement and test `make` (POSIX.1-2024)
:id: TSK_0156
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `make` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the SD feature group, so gate it behind the feature
macro.
```

### TSK_0157 — Implement and test `man` (POSIX.1-2024)

```{task} Implement and test `man` (POSIX.1-2024)
:id: TSK_0157
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `man` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/man.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0158 — Implement and test `mesg` (POSIX.1-2024)

```{task} Implement and test `mesg` (POSIX.1-2024)
:id: TSK_0158
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `mesg` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mesg.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0159 — Implement and test `mkdir` (POSIX.1-2024)

```{task} Implement and test `mkdir` (POSIX.1-2024)
:id: TSK_0159
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `mkdir` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mkdir.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0160 — Implement and test `mkfifo` (POSIX.1-2024)

```{task} Implement and test `mkfifo` (POSIX.1-2024)
:id: TSK_0160
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `mkfifo` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mkfifo.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0161 — Implement and test `more` (POSIX.1-2024)

```{task} Implement and test `more` (POSIX.1-2024)
:id: TSK_0161
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `more` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/more.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0162 — Implement and test `msgfmt` (POSIX.1-2024)

```{task} Implement and test `msgfmt` (POSIX.1-2024)
:id: TSK_0162
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `msgfmt` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/msgfmt.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0163 — Implement and test `mv` (POSIX.1-2024)

```{task} Implement and test `mv` (POSIX.1-2024)
:id: TSK_0163
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `mv` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mv.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0164 — Implement and test `newgrp` (POSIX.1-2024)

```{task} Implement and test `newgrp` (POSIX.1-2024)
:id: TSK_0164
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `newgrp` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/newgrp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0165 — Implement and test `ngettext` (POSIX.1-2024)

```{task} Implement and test `ngettext` (POSIX.1-2024)
:id: TSK_0165
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ngettext` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ngettext.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0166 — Implement and test `nice` (POSIX.1-2024)

```{task} Implement and test `nice` (POSIX.1-2024)
:id: TSK_0166
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `nice` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nice.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0167 — Implement and test `nl` (POSIX.1-2024)

```{task} Implement and test `nl` (POSIX.1-2024)
:id: TSK_0167
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `nl` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nl.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0168 — Implement and test `nm` (POSIX.1-2024)

```{task} Implement and test `nm` (POSIX.1-2024)
:id: TSK_0168
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `nm` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nm.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0169 — Implement and test `nohup` (POSIX.1-2024)

```{task} Implement and test `nohup` (POSIX.1-2024)
:id: TSK_0169
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `nohup` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nohup.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0170 — Implement and test `od` (POSIX.1-2024)

```{task} Implement and test `od` (POSIX.1-2024)
:id: TSK_0170
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `od` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/od.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0171 — Implement and test `paste` (POSIX.1-2024)

```{task} Implement and test `paste` (POSIX.1-2024)
:id: TSK_0171
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `paste` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/paste.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0172 — Implement and test `patch` (POSIX.1-2024)

```{task} Implement and test `patch` (POSIX.1-2024)
:id: TSK_0172
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `patch` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/patch.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0173 — Implement and test `pathchk` (POSIX.1-2024)

```{task} Implement and test `pathchk` (POSIX.1-2024)
:id: TSK_0173
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `pathchk` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pathchk.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0174 — Implement and test `pax` (POSIX.1-2024)

```{task} Implement and test `pax` (POSIX.1-2024)
:id: TSK_0174
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `pax` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pax.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0175 — Implement and test `pr` (POSIX.1-2024)

```{task} Implement and test `pr` (POSIX.1-2024)
:id: TSK_0175
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `pr` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pr.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0176 — Implement and test `printf` (POSIX.1-2024)

```{task} Implement and test `printf` (POSIX.1-2024)
:id: TSK_0176
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `printf` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/printf.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0177 — Implement and test `prs` (POSIX.1-2024)

```{task} Implement and test `prs` (POSIX.1-2024)
:id: TSK_0177
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `prs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/prs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0178 — Implement and test `ps` (POSIX.1-2024)

```{task} Implement and test `ps` (POSIX.1-2024)
:id: TSK_0178
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ps` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ps.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0179 — Implement and test `pwd` (POSIX.1-2024)

```{task} Implement and test `pwd` (POSIX.1-2024)
:id: TSK_0179
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `pwd` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pwd.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0180 — Implement and test `read` (POSIX.1-2024)

```{task} Implement and test `read` (POSIX.1-2024)
:id: TSK_0180
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `read` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/read.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0181 — Implement and test `readlink` (POSIX.1-2024)

```{task} Implement and test `readlink` (POSIX.1-2024)
:id: TSK_0181
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `readlink` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/readlink.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0182 — Implement and test `realpath` (POSIX.1-2024)

```{task} Implement and test `realpath` (POSIX.1-2024)
:id: TSK_0182
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `realpath` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/realpath.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0183 — Implement and test `renice` (POSIX.1-2024)

```{task} Implement and test `renice` (POSIX.1-2024)
:id: TSK_0183
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `renice` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/renice.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0184 — Implement and test `rm` (POSIX.1-2024)

```{task} Implement and test `rm` (POSIX.1-2024)
:id: TSK_0184
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `rm` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rm.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0185 — Implement and test `rmdel` (POSIX.1-2024)

```{task} Implement and test `rmdel` (POSIX.1-2024)
:id: TSK_0185
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `rmdel` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rmdel.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0186 — Implement and test `rmdir` (POSIX.1-2024)

```{task} Implement and test `rmdir` (POSIX.1-2024)
:id: TSK_0186
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `rmdir` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rmdir.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0187 — Implement and test `sact` (POSIX.1-2024)

```{task} Implement and test `sact` (POSIX.1-2024)
:id: TSK_0187
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sact` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sact.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0188 — Implement and test `sccs` (POSIX.1-2024)

```{task} Implement and test `sccs` (POSIX.1-2024)
:id: TSK_0188
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sccs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sccs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0189 — Implement and test `sed` (POSIX.1-2024)

```{task} Implement and test `sed` (POSIX.1-2024)
:id: TSK_0189
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sed` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sed.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0190 — Implement and test `sh` (POSIX.1-2024)

```{task} Implement and test `sh` (POSIX.1-2024)
:id: TSK_0190
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sh` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sh.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0191 — Implement and test `sleep` (POSIX.1-2024)

```{task} Implement and test `sleep` (POSIX.1-2024)
:id: TSK_0191
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sleep` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sleep.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0192 — Implement and test `sort` (POSIX.1-2024)

```{task} Implement and test `sort` (POSIX.1-2024)
:id: TSK_0192
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `sort` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sort.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0193 — Implement and test `split` (POSIX.1-2024)

```{task} Implement and test `split` (POSIX.1-2024)
:id: TSK_0193
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `split` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/split.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0194 — Implement and test `strings` (POSIX.1-2024)

```{task} Implement and test `strings` (POSIX.1-2024)
:id: TSK_0194
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `strings` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/strings.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0195 — Implement and test `strip` (POSIX.1-2024)

```{task} Implement and test `strip` (POSIX.1-2024)
:id: TSK_0195
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `strip` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/strip.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the SD feature group, so gate it behind the feature
macro.
```

### TSK_0196 — Implement and test `stty` (POSIX.1-2024)

```{task} Implement and test `stty` (POSIX.1-2024)
:id: TSK_0196
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `stty` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/stty.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0197 — Implement and test `tabs` (POSIX.1-2024)

```{task} Implement and test `tabs` (POSIX.1-2024)
:id: TSK_0197
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tabs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tabs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0198 — Implement and test `tail` (POSIX.1-2024)

```{task} Implement and test `tail` (POSIX.1-2024)
:id: TSK_0198
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tail` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tail.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0199 — Implement and test `talk` (POSIX.1-2024)

```{task} Implement and test `talk` (POSIX.1-2024)
:id: TSK_0199
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `talk` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/talk.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0200 — Implement and test `tee` (POSIX.1-2024)

```{task} Implement and test `tee` (POSIX.1-2024)
:id: TSK_0200
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tee` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tee.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0201 — Implement and test `test` (POSIX.1-2024)

```{task} Implement and test `test` (POSIX.1-2024)
:id: TSK_0201
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `test` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/test.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0202 — Implement and test `time` (POSIX.1-2024)

```{task} Implement and test `time` (POSIX.1-2024)
:id: TSK_0202
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `time` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/time.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0203 — Implement and test `timeout` (POSIX.1-2024)

```{task} Implement and test `timeout` (POSIX.1-2024)
:id: TSK_0203
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `timeout` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/timeout.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0204 — Implement and test `touch` (POSIX.1-2024)

```{task} Implement and test `touch` (POSIX.1-2024)
:id: TSK_0204
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `touch` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/touch.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0205 — Implement and test `tput` (POSIX.1-2024)

```{task} Implement and test `tput` (POSIX.1-2024)
:id: TSK_0205
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tput` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tput.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0206 — Implement and test `tr` (POSIX.1-2024)

```{task} Implement and test `tr` (POSIX.1-2024)
:id: TSK_0206
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tr` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tr.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0207 — Implement and test `true` (POSIX.1-2024)

```{task} Implement and test `true` (POSIX.1-2024)
:id: TSK_0207
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `true` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/true.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0208 — Implement and test `tsort` (POSIX.1-2024)

```{task} Implement and test `tsort` (POSIX.1-2024)
:id: TSK_0208
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tsort` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tsort.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0209 — Implement and test `tty` (POSIX.1-2024)

```{task} Implement and test `tty` (POSIX.1-2024)
:id: TSK_0209
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `tty` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tty.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0210 — Implement and test `type` (POSIX.1-2024)

```{task} Implement and test `type` (POSIX.1-2024)
:id: TSK_0210
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `type` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/type.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0211 — Implement and test `ulimit` (POSIX.1-2024)

```{task} Implement and test `ulimit` (POSIX.1-2024)
:id: TSK_0211
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `ulimit` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ulimit.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0212 — Implement and test `umask` (POSIX.1-2024)

```{task} Implement and test `umask` (POSIX.1-2024)
:id: TSK_0212
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `umask` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/umask.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0213 — Implement and test `unalias` (POSIX.1-2024)

```{task} Implement and test `unalias` (POSIX.1-2024)
:id: TSK_0213
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `unalias` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unalias.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0214 — Implement and test `uname` (POSIX.1-2024)

```{task} Implement and test `uname` (POSIX.1-2024)
:id: TSK_0214
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uname` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uname.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0215 — Implement and test `uncompress` (POSIX.1-2024)

```{task} Implement and test `uncompress` (POSIX.1-2024)
:id: TSK_0215
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uncompress` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uncompress.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0216 — Implement and test `unexpand` (POSIX.1-2024)

```{task} Implement and test `unexpand` (POSIX.1-2024)
:id: TSK_0216
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `unexpand` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unexpand.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0217 — Implement and test `unget` (POSIX.1-2024)

```{task} Implement and test `unget` (POSIX.1-2024)
:id: TSK_0217
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `unget` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unget.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0218 — Implement and test `uniq` (POSIX.1-2024)

```{task} Implement and test `uniq` (POSIX.1-2024)
:id: TSK_0218
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uniq` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uniq.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0219 — Implement and test `unlink` (POSIX.1-2024)

```{task} Implement and test `unlink` (POSIX.1-2024)
:id: TSK_0219
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `unlink` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unlink.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0220 — Implement and test `uucp` (POSIX.1-2024)

```{task} Implement and test `uucp` (POSIX.1-2024)
:id: TSK_0220
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uucp` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uucp.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UU feature group, so gate it behind the feature
macro.
```

### TSK_0221 — Implement and test `uudecode` (POSIX.1-2024)

```{task} Implement and test `uudecode` (POSIX.1-2024)
:id: TSK_0221
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uudecode` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uudecode.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0222 — Implement and test `uuencode` (POSIX.1-2024)

```{task} Implement and test `uuencode` (POSIX.1-2024)
:id: TSK_0222
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uuencode` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uuencode.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0223 — Implement and test `uustat` (POSIX.1-2024)

```{task} Implement and test `uustat` (POSIX.1-2024)
:id: TSK_0223
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uustat` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uustat.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UU feature group, so gate it behind the feature
macro.
```

### TSK_0224 — Implement and test `uux` (POSIX.1-2024)

```{task} Implement and test `uux` (POSIX.1-2024)
:id: TSK_0224
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `uux` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uux.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UU feature group, so gate it behind the feature
macro.
```

### TSK_0225 — Implement and test `val` (POSIX.1-2024)

```{task} Implement and test `val` (POSIX.1-2024)
:id: TSK_0225
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `val` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/val.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0226 — Implement and test `vi` (POSIX.1-2024)

```{task} Implement and test `vi` (POSIX.1-2024)
:id: TSK_0226
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `vi` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/vi.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the UP feature group, so gate it behind the feature
macro.
```

### TSK_0227 — Implement and test `wait` (POSIX.1-2024)

```{task} Implement and test `wait` (POSIX.1-2024)
:id: TSK_0227
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `wait` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/wait.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0228 — Implement and test `wc` (POSIX.1-2024)

```{task} Implement and test `wc` (POSIX.1-2024)
:id: TSK_0228
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `wc` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/wc.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0229 — Implement and test `what` (POSIX.1-2024)

```{task} Implement and test `what` (POSIX.1-2024)
:id: TSK_0229
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `what` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/what.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0230 — Implement and test `who` (POSIX.1-2024)

```{task} Implement and test `who` (POSIX.1-2024)
:id: TSK_0230
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `who` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/who.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

### TSK_0231 — Implement and test `write` (POSIX.1-2024)

```{task} Implement and test `write` (POSIX.1-2024)
:id: TSK_0231
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `write` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/write.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0232 — Implement and test `xargs` (POSIX.1-2024)

```{task} Implement and test `xargs` (POSIX.1-2024)
:id: TSK_0232
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `xargs` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/xargs.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
```

### TSK_0233 — Implement and test `xgettext` (POSIX.1-2024)

```{task} Implement and test `xgettext` (POSIX.1-2024)
:id: TSK_0233
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `xgettext` utility to full POSIX.1-2024 (Issue 8) support. Add a
test suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/xgettext.html`.
Cover every command-line option, operand, and synopsis. Verify stdin, stdout,
and stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the CD feature group, so gate it behind the feature
macro.
```

### TSK_0234 — Implement and test `yacc` (POSIX.1-2024)

```{task} Implement and test `yacc` (POSIX.1-2024)
:id: TSK_0234
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `yacc` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/yacc.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the CD feature group, so gate it behind the feature
macro.
```

### TSK_0235 — Implement and test `zcat` (POSIX.1-2024)

```{task} Implement and test `zcat` (POSIX.1-2024)
:id: TSK_0235
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Implement the `zcat` utility to full POSIX.1-2024 (Issue 8) support. Add a test
suite for the complete interface defined in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/zcat.html`. Cover
every command-line option, operand, and synopsis. Verify stdin, stdout, and
stderr behavior, input and output files, and the environment variables in
V1_chap08. Check asynchronous signal handling and the exit status for each case.
The utility is part of the XSI feature group, so gate it behind the feature
macro.
```

