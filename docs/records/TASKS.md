# Tasks

## Purpose

This file records active, temporary tasks (`task`) for `microkernel`.

## Structure

This file shows tasks in priority sequence (highest priority first). Each task
connects to the normative model through `:traces:`.

## Format Rules

- Normative specifications and records must not connect to `TSK_` items.
- Move completed or canceled tasks with no text changes into
  [HISTORY.md](HISTORY.md) with status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0065 — Link OpenSSH Portable dynamically to shared libraries

```{task} Link OpenSSH Portable dynamically to shared libraries
:id: TSK_0065
:status: open
:traces: BDR_0003, BDR_0004, ADR_0054, ADR_0055, ADR_0056, SYS_IPC_0001

Change `cmake/openssh.cmake` to link `sshd`, `sshd-session`, and `ssh-keygen`
dynamically with `--dynamic-linker=/boot/ld.so`. Link those executables to
`libcrypto.so.3`, `libz.so.1`, and `libc.so`. Examine `PT_INTERP` and
`DT_NEEDED` headers with `llvm-readelf`. Do tests of RSA, ECDSA, Ed25519,
compressed (`zlib@openssh.com`), password, and PTY sessions in CTest
`ipc.sshd_e2e` ({need}`SYS_IPC_0001`).
```

### TSK_0066 — Link Ring 3 bin/ shell and utilities dynamically to libc.so

```{task} Link Ring 3 bin/ shell and utilities dynamically to libc.so
:id: TSK_0066
:status: open
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001, SYS_PROC_0001

Change `bin/` (`sh`, XCU utilities, `nc`, `ping`, `sshd-setup`) to dynamic
executables linked to `ld.so` and `libc.so`. Keep primary Ring 3 boot servers
statically linked. Make sure that the `boot.tar` byte count decreases. Do the
full QEMU and CTest suites. Make sure that all tests complete without errors
({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```

### TSK_0075 — Add native ldd in /boot/ld.so and ldconfig in bin/

```{task} Add native ldd in /boot/ld.so and ldconfig in bin/
:id: TSK_0075
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001

Change `/boot/ld.so` to find `argv[0]` values that end in `ldd` or
`LD_TRACE_LOADED_OBJECTS=1`. Find `DT_NEEDED` dependencies across
`/etc/ld.so.conf` and the standard library paths. Write `libname => /path
(0xaddr)` and do not give control to `_start`. Install `/usr/bin/ldd` as a
symlink to `/boot/ld.so`. Add native `/usr/sbin/ldconfig` in `bin/`, linked
dynamically to `libc.so`, to keep `DT_SONAME` symlinks. Make sure that
`ldconfig` exits without errors for build scripts. Do tests with `zlibtest`,
`openssltest`, and `sshd` in QEMU ({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
```

### TSK_0076 — Add getent and internal C.UTF-8 locale operation

```{task} Add getent and internal C.UTF-8 locale operation
:id: TSK_0076
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, SYS_LIBC_0001

Add `/usr/bin/getent` in Ring 3 `bin/`, linked dynamically to `libc.so`. Add
`passwd`, `group`, `hosts`, `services`, and `protocols` lookup with the active
`libc` NSS functions (`getpwnam`, `getgrnam`, `getaddrinfo`, `getservbyname`,
`getprotobyname`). Add full table enumeration for those databases. Use small
`/etc/nsswitch.conf` defaults. Make sure that `libc` (`libc.so` and `libc.a`)
operates with `"C"` and `"C.UTF-8"` in `setlocale()` and `nl_langinfo()`. Do not
make external binary `LC_*` files necessary in `boot.tar`. Do tests in QEMU and
CTest ({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
```

### TSK_0077 — Compile IANA tzcode (zic, zdump, tzselect) with CMake

```{task} Compile IANA tzcode (zic, zdump, tzselect) with CMake
:id: TSK_0077
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054, SYS_LIBC_0001

Add `cmake/tzcode.cmake` to download and compile public-domain IANA `tzcode`
(`zic` and `zdump`) for Ring 3. Link `zic` and `zdump` dynamically to `libc.so`.
Install `tzselect` into the Ring 3 sysroot and `boot.tar`. Do not keep upstream
source in git ({need}`WDR_0012`, {need}`BDR_0003`, {need}`BDR_0004`). Record the
notice in `docs/THIRD-PARTY-NOTICES.md`. Do tests of `TZif` compilation and
`zdump` output in QEMU and CTest ({need}`SYS_LIBC_0001`).
```

### TSK_0078 — Compile tar (libarchive/bsdtar) with CMake and link dynamically

```{task} Compile tar (libarchive/bsdtar) with CMake and link dynamically
:id: TSK_0078
:status: open
:traces: WDR_0012, BDR_0003, BDR_0004, ADR_0054, SYS_LIBC_0001

Add `cmake/tar.cmake` to download and compile `tar` (`libarchive` `bsdtar` or
GNU `tar`) for Ring 3. Link `tar` dynamically to `libc.so` and `libz.so.1` with
`--dynamic-linker=/boot/ld.so`. Do not keep upstream source in git. Change
`docs/THIRD-PARTY-NOTICES.md` to add the license notice. Install `/usr/bin/tar`
into the Ring 3 sysroot and `boot.tar`. Examine `PT_INTERP` and `DT_NEEDED` with
`ldd` and `llvm-readelf`. Do tests of `.tar` and `.tar.gz` archive creation and
extraction in QEMU and CTest ({need}`WDR_0012`, {need}`SYS_LIBC_0001`).
```

### TSK_0067 — Add ConsoleBackend and QEMU raspi4b ARM64 boot harness

```{task} Add ConsoleBackend and QEMU raspi4b ARM64 boot harness
:id: TSK_0067
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024, SYS_BOOT_0001

Add a compile-time `ConsoleBackend` interface in the kernel. Send initial
console output through `ConsoleBackend`. Add the `aarch64` QEMU `-M raspi4b`
boot path with the PL011 UART (`0xFE201000`, `-serial stdio`) as the first
backend. Change the QEMU test harness to make sure that ARM64 boots and writes
serial output ({need}`ADR_0058`, {need}`WDR_0011`, {need}`ADR_0024`,
{need}`SYS_BOOT_0001`).
```

### TSK_0068 — Add primary ARM64 BCM2711 architecture operation

```{task} Add primary ARM64 BCM2711 architecture operation
:id: TSK_0068
:status: open
:traces: ADR_0024, WDR_0011, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001,
    SYS_IPC_0001

Add primary ARM64 (BCM2711) architecture code. Include the `EL2` to `EL1` mode
change, MMU page tables, GIC-400 interrupt dispatch, generic timers, the
scheduler, and IPC paths. Do the test suite in QEMU `-M raspi4b`
({need}`ADR_0024`, {need}`WDR_0011`, {need}`SYS_BOOT_0001`,
{need}`SYS_MEM_0001`, {need}`SYS_PROC_0001`, {need}`SYS_IPC_0001`).
```

### TSK_0069 — Add platform-agnostic USB CDC-ACM protocol state machine

```{task} Add platform-agnostic USB CDC-ACM protocol state machine
:id: TSK_0069
:status: open
:traces: ADR_0058, WDR_0011

Add the platform-agnostic USB 2.0 and CDC-ACM protocol state machine. Include
device, configuration, and string descriptors, the `EP0` `SETUP` state machine,
and TX/RX ring buffers. Add host and QEMU unit tests for enumeration state
changes. Add USB code for `x86-64` and `arm64`. These two architectures have no
USB driver at this time ({need}`ADR_0058`, {need}`WDR_0011`).
```

### TSK_0070 — Add shared DWC2 PIO core driver and BCM2711 board code

```{task} Add shared DWC2 PIO core driver and BCM2711 board code
:id: TSK_0070
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024

Add the shared Synopsys DWC2 core register driver in device/PIO mode. Include
core initialization, FIFO partitions, interrupt dispatch, and PIO packet input
and output. Add the BCM2711 MMIO board interface at `0xFE980000` behind
`EARLY_USB_CDC_CONSOLE`. Record the new Ring 0 console modules in
`tests/arch/microkernel_allowlist.json` with `ADR_0058` ({need}`ADR_0058`,
{need}`WDR_0011`, {need}`ADR_0024`).
```

### TSK_0071 — Make board-configurable host HIL runner in tools/hil/

```{task} Make board-configurable host HIL runner in tools/hil/
:id: TSK_0071
:status: open
:traces: WDR_0011, ADR_0058

Make the board-configurable host HIL test runner in `tools/hil/`. Add modular
power backends (`uhubctl`), image loaders (`rpiboot`), and telemetry collectors
(`usbmon`, `lsusb`, and the console device). Add the `boards/rpi400.yaml` board
configuration ({need}`WDR_0011`, {need}`ADR_0058`).
```

### TSK_0072 — Do one-time Pi 400 setup through SSH

```{task} Do one-time Pi 400 setup through SSH
:id: TSK_0072
:status: open
:traces: WDR_0011

While the flash disk is active, do the one-time Pi 400 setup through SSH. Make
sure that `uhubctl` power control operates on the host. Set `BOOT_ORDER=0xf31`
with `rpi-eeprom-config`. Make copies of `start4.elf`, `fixup4.dat`,
`bcm2711-rpi-400.dtb`, and `overlays/dwc2.dtbo` on the host. Disconnect power
and remove the flash disk. The workflow does not use SSH or the SD card again
({need}`WDR_0011`).
```

### TSK_0073 — Do tests of DWC2 EP0 enumeration and descriptor markers

```{task} Do tests of DWC2 EP0 enumeration and descriptor markers
:id: TSK_0073
:status: open
:traces: ADR_0058, WDR_0011

Do tests of DWC2 peripheral `EP0` enumeration on the physical Pi 400 through the
`tools/hil/` loop. Make sure that the PHY and initialization handshake show in
host `dmesg`. Record raw `EP0` `SETUP` traffic with `usbmon`. Examine
`bcdDevice` and string descriptor markers with `lsusb -v` ({need}`ADR_0058`,
{need}`WDR_0011`).
```

### TSK_0074 — Do tests of CDC-ACM console and start hardware driver work

```{task} Do tests of CDC-ACM console and start hardware driver work
:id: TSK_0074
:status: open
:traces: ADR_0058, WDR_0011, ADR_0024

After `SET_CONFIGURATION` completes, do tests of the CDC-ACM `/dev/ttyACM0`
input and output console on the physical Pi 400. Include the CDC communication
and data interfaces. Start automatic hardware driver development through the
active console. Start with the VideoCore Mailbox HDMI framebuffer, the PCIe root
complex, and the VL805 xHCI host controller ({need}`ADR_0058`, {need}`WDR_0011`,
{need}`ADR_0024`).
```

### TSK_0079 — Add `.` and do tests (POSIX.1-2024)

```{task} Add . and do tests (POSIX.1-2024)
:id: TSK_0079
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `.` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dot.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0080 — Add `:` and do tests (POSIX.1-2024)

```{task} Add : and do tests (POSIX.1-2024)
:id: TSK_0080
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `:` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/colon.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0081 — Add `admin` and do tests (POSIX.1-2024)

```{task} Add admin and do tests (POSIX.1-2024)
:id: TSK_0081
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `admin` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/admin.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0082 — Add `alias` and do tests (POSIX.1-2024)

```{task} Add alias and do tests (POSIX.1-2024)
:id: TSK_0082
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `alias` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/alias.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0083 — Add `ar` and do tests (POSIX.1-2024)

```{task} Add ar and do tests (POSIX.1-2024)
:id: TSK_0083
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ar` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ar.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the SD option
group, put it behind the feature macro.
```

### TSK_0084 — Add `asa` and do tests (POSIX.1-2024)

```{task} Add asa and do tests (POSIX.1-2024)
:id: TSK_0084
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `asa` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/asa.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the FR option
group, put it behind the feature macro.
```

### TSK_0085 — Add `at` and do tests (POSIX.1-2024)

```{task} Add at and do tests (POSIX.1-2024)
:id: TSK_0085
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `at` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/at.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0086 — Add `awk` and do tests (POSIX.1-2024)

```{task} Add awk and do tests (POSIX.1-2024)
:id: TSK_0086
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `awk` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/awk.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0087 — Add `basename` and do tests (POSIX.1-2024)

```{task} Add basename and do tests (POSIX.1-2024)
:id: TSK_0087
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `basename` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/basename.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0088 — Add `batch` and do tests (POSIX.1-2024)

```{task} Add batch and do tests (POSIX.1-2024)
:id: TSK_0088
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `batch` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/batch.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0089 — Add `bc` and do tests (POSIX.1-2024)

```{task} Add bc and do tests (POSIX.1-2024)
:id: TSK_0089
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `bc` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/bc.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0090 — Add `bg` and do tests (POSIX.1-2024)

```{task} Add bg and do tests (POSIX.1-2024)
:id: TSK_0090
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `bg` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/bg.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0091 — Add `c17` and do tests (POSIX.1-2024)

```{task} Add c17 and do tests (POSIX.1-2024)
:id: TSK_0091
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `c17` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/c17.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0092 — Add `cal` and do tests (POSIX.1-2024)

```{task} Add cal and do tests (POSIX.1-2024)
:id: TSK_0092
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cal` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cal.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0093 — Add `cat` and do tests (POSIX.1-2024)

```{task} Add cat and do tests (POSIX.1-2024)
:id: TSK_0093
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cat` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cat.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0094 — Add `cd` and do tests (POSIX.1-2024)

```{task} Add cd and do tests (POSIX.1-2024)
:id: TSK_0094
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cd` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cd.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0095 — Add `cflow` and do tests (POSIX.1-2024)

```{task} Add cflow and do tests (POSIX.1-2024)
:id: TSK_0095
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cflow` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cflow.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0096 — Add `chgrp` and do tests (POSIX.1-2024)

```{task} Add chgrp and do tests (POSIX.1-2024)
:id: TSK_0096
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `chgrp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chgrp.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0097 — Add `chmod` and do tests (POSIX.1-2024)

```{task} Add chmod and do tests (POSIX.1-2024)
:id: TSK_0097
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `chmod` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chmod.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0098 — Add `chown` and do tests (POSIX.1-2024)

```{task} Add chown and do tests (POSIX.1-2024)
:id: TSK_0098
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `chown` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/chown.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0099 — Add `cksum` and do tests (POSIX.1-2024)

```{task} Add cksum and do tests (POSIX.1-2024)
:id: TSK_0099
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cksum` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cksum.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0100 — Add `cmp` and do tests (POSIX.1-2024)

```{task} Add cmp and do tests (POSIX.1-2024)
:id: TSK_0100
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cmp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cmp.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0101 — Add `comm` and do tests (POSIX.1-2024)

```{task} Add comm and do tests (POSIX.1-2024)
:id: TSK_0101
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `comm` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/comm.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0102 — Add `command` and do tests (POSIX.1-2024)

```{task} Add command and do tests (POSIX.1-2024)
:id: TSK_0102
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `command` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/command.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0103 — Add `compress` and do tests (POSIX.1-2024)

```{task} Add compress and do tests (POSIX.1-2024)
:id: TSK_0103
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `compress` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/compress.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0104 — Add `cp` and do tests (POSIX.1-2024)

```{task} Add cp and do tests (POSIX.1-2024)
:id: TSK_0104
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cp.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0105 — Add `crontab` and do tests (POSIX.1-2024)

```{task} Add crontab and do tests (POSIX.1-2024)
:id: TSK_0105
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `crontab` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/crontab.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0106 — Add `csplit` and do tests (POSIX.1-2024)

```{task} Add csplit and do tests (POSIX.1-2024)
:id: TSK_0106
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `csplit` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/csplit.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0107 — Add `ctags` and do tests (POSIX.1-2024)

```{task} Add ctags and do tests (POSIX.1-2024)
:id: TSK_0107
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ctags` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ctags.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0108 — Add `cut` and do tests (POSIX.1-2024)

```{task} Add cut and do tests (POSIX.1-2024)
:id: TSK_0108
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cut` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cut.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0109 — Add `cxref` and do tests (POSIX.1-2024)

```{task} Add cxref and do tests (POSIX.1-2024)
:id: TSK_0109
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `cxref` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/cxref.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0110 — Add `date` and do tests (POSIX.1-2024)

```{task} Add date and do tests (POSIX.1-2024)
:id: TSK_0110
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `date` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/date.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0111 — Add `dd` and do tests (POSIX.1-2024)

```{task} Add dd and do tests (POSIX.1-2024)
:id: TSK_0111
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `dd` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dd.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0112 — Add `delta` and do tests (POSIX.1-2024)

```{task} Add delta and do tests (POSIX.1-2024)
:id: TSK_0112
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `delta` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/delta.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0113 — Add `df` and do tests (POSIX.1-2024)

```{task} Add df and do tests (POSIX.1-2024)
:id: TSK_0113
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `df` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/df.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0114 — Add `diff` and do tests (POSIX.1-2024)

```{task} Add diff and do tests (POSIX.1-2024)
:id: TSK_0114
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `diff` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/diff.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0115 — Add `dirname` and do tests (POSIX.1-2024)

```{task} Add dirname and do tests (POSIX.1-2024)
:id: TSK_0115
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `dirname` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/dirname.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0116 — Add `du` and do tests (POSIX.1-2024)

```{task} Add du and do tests (POSIX.1-2024)
:id: TSK_0116
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `du` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/du.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0117 — Add `echo` and do tests (POSIX.1-2024)

```{task} Add echo and do tests (POSIX.1-2024)
:id: TSK_0117
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `echo` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/echo.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0118 — Add `ed` and do tests (POSIX.1-2024)

```{task} Add ed and do tests (POSIX.1-2024)
:id: TSK_0118
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ed` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ed.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0119 — Add `env` and do tests (POSIX.1-2024)

```{task} Add env and do tests (POSIX.1-2024)
:id: TSK_0119
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `env` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/env.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0120 — Add `ex` and do tests (POSIX.1-2024)

```{task} Add ex and do tests (POSIX.1-2024)
:id: TSK_0120
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ex` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ex.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0121 — Add `expand` and do tests (POSIX.1-2024)

```{task} Add expand and do tests (POSIX.1-2024)
:id: TSK_0121
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `expand` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/expand.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0122 — Add `expr` and do tests (POSIX.1-2024)

```{task} Add expr and do tests (POSIX.1-2024)
:id: TSK_0122
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `expr` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/expr.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0123 — Add `false` and do tests (POSIX.1-2024)

```{task} Add false and do tests (POSIX.1-2024)
:id: TSK_0123
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `false` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/false.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0124 — Add `fc` and do tests (POSIX.1-2024)

```{task} Add fc and do tests (POSIX.1-2024)
:id: TSK_0124
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `fc` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fc.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0125 — Add `fg` and do tests (POSIX.1-2024)

```{task} Add fg and do tests (POSIX.1-2024)
:id: TSK_0125
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `fg` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fg.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0126 — Add `file` and do tests (POSIX.1-2024)

```{task} Add file and do tests (POSIX.1-2024)
:id: TSK_0126
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `file` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/file.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0127 — Add `find` and do tests (POSIX.1-2024)

```{task} Add find and do tests (POSIX.1-2024)
:id: TSK_0127
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `find` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/find.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0128 — Add `fold` and do tests (POSIX.1-2024)

```{task} Add fold and do tests (POSIX.1-2024)
:id: TSK_0128
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `fold` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fold.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0129 — Add `fuser` and do tests (POSIX.1-2024)

```{task} Add fuser and do tests (POSIX.1-2024)
:id: TSK_0129
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `fuser` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/fuser.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0130 — Add `gencat` and do tests (POSIX.1-2024)

```{task} Add gencat and do tests (POSIX.1-2024)
:id: TSK_0130
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `gencat` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/gencat.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0131 — Add `get` and do tests (POSIX.1-2024)

```{task} Add get and do tests (POSIX.1-2024)
:id: TSK_0131
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `get` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/get.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0132 — Add `getconf` and do tests (POSIX.1-2024)

```{task} Add getconf and do tests (POSIX.1-2024)
:id: TSK_0132
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `getconf` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/getconf.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0133 — Add `getopts` and do tests (POSIX.1-2024)

```{task} Add getopts and do tests (POSIX.1-2024)
:id: TSK_0133
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `getopts` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/getopts.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0134 — Add `gettext` and do tests (POSIX.1-2024)

```{task} Add gettext and do tests (POSIX.1-2024)
:id: TSK_0134
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `gettext` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/gettext.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0135 — Add `grep` and do tests (POSIX.1-2024)

```{task} Add grep and do tests (POSIX.1-2024)
:id: TSK_0135
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `grep` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/grep.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0136 — Add `hash` and do tests (POSIX.1-2024)

```{task} Add hash and do tests (POSIX.1-2024)
:id: TSK_0136
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `hash` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/hash.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0137 — Add `head` and do tests (POSIX.1-2024)

```{task} Add head and do tests (POSIX.1-2024)
:id: TSK_0137
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `head` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/head.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0138 — Add `iconv` and do tests (POSIX.1-2024)

```{task} Add iconv and do tests (POSIX.1-2024)
:id: TSK_0138
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `iconv` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/iconv.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0139 — Add `id` and do tests (POSIX.1-2024)

```{task} Add id and do tests (POSIX.1-2024)
:id: TSK_0139
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `id` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/id.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0140 — Add `ipcrm` and do tests (POSIX.1-2024)

```{task} Add ipcrm and do tests (POSIX.1-2024)
:id: TSK_0140
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ipcrm` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ipcrm.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0141 — Add `ipcs` and do tests (POSIX.1-2024)

```{task} Add ipcs and do tests (POSIX.1-2024)
:id: TSK_0141
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ipcs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ipcs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0142 — Add `jobs` and do tests (POSIX.1-2024)

```{task} Add jobs and do tests (POSIX.1-2024)
:id: TSK_0142
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `jobs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/jobs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0143 — Add `join` and do tests (POSIX.1-2024)

```{task} Add join and do tests (POSIX.1-2024)
:id: TSK_0143
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `join` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/join.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0144 — Add `kill` and do tests (POSIX.1-2024)

```{task} Add kill and do tests (POSIX.1-2024)
:id: TSK_0144
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `kill` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/kill.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0145 — Add `lex` and do tests (POSIX.1-2024)

```{task} Add lex and do tests (POSIX.1-2024)
:id: TSK_0145
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `lex` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/lex.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the CD option
group, put it behind the feature macro.
```

### TSK_0146 — Add `link` and do tests (POSIX.1-2024)

```{task} Add link and do tests (POSIX.1-2024)
:id: TSK_0146
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `link` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/link.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0147 — Add `ln` and do tests (POSIX.1-2024)

```{task} Add ln and do tests (POSIX.1-2024)
:id: TSK_0147
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ln` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ln.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0148 — Add `locale` and do tests (POSIX.1-2024)

```{task} Add locale and do tests (POSIX.1-2024)
:id: TSK_0148
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `locale` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/locale.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0149 — Add `localedef` and do tests (POSIX.1-2024)

```{task} Add localedef and do tests (POSIX.1-2024)
:id: TSK_0149
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `localedef` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/localedef.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0150 — Add `logger` and do tests (POSIX.1-2024)

```{task} Add logger and do tests (POSIX.1-2024)
:id: TSK_0150
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `logger` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/logger.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0151 — Add `logname` and do tests (POSIX.1-2024)

```{task} Add logname and do tests (POSIX.1-2024)
:id: TSK_0151
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `logname` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/logname.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0152 — Add `lp` and do tests (POSIX.1-2024)

```{task} Add lp and do tests (POSIX.1-2024)
:id: TSK_0152
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `lp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/lp.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0153 — Add `ls` and do tests (POSIX.1-2024)

```{task} Add ls and do tests (POSIX.1-2024)
:id: TSK_0153
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ls` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ls.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0154 — Add `m4` and do tests (POSIX.1-2024)

```{task} Add m4 and do tests (POSIX.1-2024)
:id: TSK_0154
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `m4` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/m4.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0155 — Add `mailx` and do tests (POSIX.1-2024)

```{task} Add mailx and do tests (POSIX.1-2024)
:id: TSK_0155
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `mailx` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mailx.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0156 — Add `make` and do tests (POSIX.1-2024)

```{task} Add make and do tests (POSIX.1-2024)
:id: TSK_0156
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `make` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the SD option
group, put it behind the feature macro.
```

### TSK_0157 — Add `man` and do tests (POSIX.1-2024)

```{task} Add man and do tests (POSIX.1-2024)
:id: TSK_0157
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `man` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/man.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0158 — Add `mesg` and do tests (POSIX.1-2024)

```{task} Add mesg and do tests (POSIX.1-2024)
:id: TSK_0158
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `mesg` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mesg.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0159 — Add `mkdir` and do tests (POSIX.1-2024)

```{task} Add mkdir and do tests (POSIX.1-2024)
:id: TSK_0159
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `mkdir` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mkdir.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0160 — Add `mkfifo` and do tests (POSIX.1-2024)

```{task} Add mkfifo and do tests (POSIX.1-2024)
:id: TSK_0160
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `mkfifo` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mkfifo.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0161 — Add `more` and do tests (POSIX.1-2024)

```{task} Add more and do tests (POSIX.1-2024)
:id: TSK_0161
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `more` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/more.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0162 — Add `msgfmt` and do tests (POSIX.1-2024)

```{task} Add msgfmt and do tests (POSIX.1-2024)
:id: TSK_0162
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `msgfmt` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/msgfmt.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0163 — Add `mv` and do tests (POSIX.1-2024)

```{task} Add mv and do tests (POSIX.1-2024)
:id: TSK_0163
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `mv` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/mv.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0164 — Add `newgrp` and do tests (POSIX.1-2024)

```{task} Add newgrp and do tests (POSIX.1-2024)
:id: TSK_0164
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `newgrp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/newgrp.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0165 — Add `ngettext` and do tests (POSIX.1-2024)

```{task} Add ngettext and do tests (POSIX.1-2024)
:id: TSK_0165
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ngettext` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ngettext.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0166 — Add `nice` and do tests (POSIX.1-2024)

```{task} Add nice and do tests (POSIX.1-2024)
:id: TSK_0166
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `nice` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nice.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0167 — Add `nl` and do tests (POSIX.1-2024)

```{task} Add nl and do tests (POSIX.1-2024)
:id: TSK_0167
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `nl` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nl.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0168 — Add `nm` and do tests (POSIX.1-2024)

```{task} Add nm and do tests (POSIX.1-2024)
:id: TSK_0168
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `nm` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nm.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0169 — Add `nohup` and do tests (POSIX.1-2024)

```{task} Add nohup and do tests (POSIX.1-2024)
:id: TSK_0169
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `nohup` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/nohup.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0170 — Add `od` and do tests (POSIX.1-2024)

```{task} Add od and do tests (POSIX.1-2024)
:id: TSK_0170
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `od` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/od.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0171 — Add `paste` and do tests (POSIX.1-2024)

```{task} Add paste and do tests (POSIX.1-2024)
:id: TSK_0171
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `paste` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/paste.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0172 — Add `patch` and do tests (POSIX.1-2024)

```{task} Add patch and do tests (POSIX.1-2024)
:id: TSK_0172
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `patch` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/patch.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0173 — Add `pathchk` and do tests (POSIX.1-2024)

```{task} Add pathchk and do tests (POSIX.1-2024)
:id: TSK_0173
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `pathchk` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pathchk.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0174 — Add `pax` and do tests (POSIX.1-2024)

```{task} Add pax and do tests (POSIX.1-2024)
:id: TSK_0174
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `pax` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pax.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0175 — Add `pr` and do tests (POSIX.1-2024)

```{task} Add pr and do tests (POSIX.1-2024)
:id: TSK_0175
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `pr` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pr.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0176 — Add `printf` and do tests (POSIX.1-2024)

```{task} Add printf and do tests (POSIX.1-2024)
:id: TSK_0176
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `printf` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/printf.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0177 — Add `prs` and do tests (POSIX.1-2024)

```{task} Add prs and do tests (POSIX.1-2024)
:id: TSK_0177
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `prs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/prs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0178 — Add `ps` and do tests (POSIX.1-2024)

```{task} Add ps and do tests (POSIX.1-2024)
:id: TSK_0178
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ps` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ps.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0179 — Add `pwd` and do tests (POSIX.1-2024)

```{task} Add pwd and do tests (POSIX.1-2024)
:id: TSK_0179
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `pwd` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/pwd.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0180 — Add `read` and do tests (POSIX.1-2024)

```{task} Add read and do tests (POSIX.1-2024)
:id: TSK_0180
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `read` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/read.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0181 — Add `readlink` and do tests (POSIX.1-2024)

```{task} Add readlink and do tests (POSIX.1-2024)
:id: TSK_0181
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `readlink` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/readlink.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0182 — Add `realpath` and do tests (POSIX.1-2024)

```{task} Add realpath and do tests (POSIX.1-2024)
:id: TSK_0182
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `realpath` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/realpath.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0183 — Add `renice` and do tests (POSIX.1-2024)

```{task} Add renice and do tests (POSIX.1-2024)
:id: TSK_0183
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `renice` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/renice.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0184 — Add `rm` and do tests (POSIX.1-2024)

```{task} Add rm and do tests (POSIX.1-2024)
:id: TSK_0184
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `rm` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rm.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0185 — Add `rmdel` and do tests (POSIX.1-2024)

```{task} Add rmdel and do tests (POSIX.1-2024)
:id: TSK_0185
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `rmdel` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rmdel.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0186 — Add `rmdir` and do tests (POSIX.1-2024)

```{task} Add rmdir and do tests (POSIX.1-2024)
:id: TSK_0186
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `rmdir` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/rmdir.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0187 — Add `sact` and do tests (POSIX.1-2024)

```{task} Add sact and do tests (POSIX.1-2024)
:id: TSK_0187
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sact` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sact.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0188 — Add `sccs` and do tests (POSIX.1-2024)

```{task} Add sccs and do tests (POSIX.1-2024)
:id: TSK_0188
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sccs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sccs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0189 — Add `sed` and do tests (POSIX.1-2024)

```{task} Add sed and do tests (POSIX.1-2024)
:id: TSK_0189
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sed` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sed.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0190 — Add `sh` and do tests (POSIX.1-2024)

```{task} Add sh and do tests (POSIX.1-2024)
:id: TSK_0190
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sh` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sh.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0191 — Add `sleep` and do tests (POSIX.1-2024)

```{task} Add sleep and do tests (POSIX.1-2024)
:id: TSK_0191
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sleep` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sleep.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0192 — Add `sort` and do tests (POSIX.1-2024)

```{task} Add sort and do tests (POSIX.1-2024)
:id: TSK_0192
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `sort` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/sort.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0193 — Add `split` and do tests (POSIX.1-2024)

```{task} Add split and do tests (POSIX.1-2024)
:id: TSK_0193
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `split` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/split.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0194 — Add `strings` and do tests (POSIX.1-2024)

```{task} Add strings and do tests (POSIX.1-2024)
:id: TSK_0194
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `strings` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/strings.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0195 — Add `strip` and do tests (POSIX.1-2024)

```{task} Add strip and do tests (POSIX.1-2024)
:id: TSK_0195
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `strip` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/strip.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the SD option
group, put it behind the feature macro.
```

### TSK_0196 — Add `stty` and do tests (POSIX.1-2024)

```{task} Add stty and do tests (POSIX.1-2024)
:id: TSK_0196
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `stty` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/stty.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0197 — Add `tabs` and do tests (POSIX.1-2024)

```{task} Add tabs and do tests (POSIX.1-2024)
:id: TSK_0197
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tabs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tabs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0198 — Add `tail` and do tests (POSIX.1-2024)

```{task} Add tail and do tests (POSIX.1-2024)
:id: TSK_0198
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tail` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tail.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0199 — Add `talk` and do tests (POSIX.1-2024)

```{task} Add talk and do tests (POSIX.1-2024)
:id: TSK_0199
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `talk` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/talk.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0200 — Add `tee` and do tests (POSIX.1-2024)

```{task} Add tee and do tests (POSIX.1-2024)
:id: TSK_0200
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tee` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tee.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0201 — Add `test` and do tests (POSIX.1-2024)

```{task} Add test and do tests (POSIX.1-2024)
:id: TSK_0201
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `test` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/test.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0202 — Add `time` and do tests (POSIX.1-2024)

```{task} Add time and do tests (POSIX.1-2024)
:id: TSK_0202
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `time` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/time.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0203 — Add `timeout` and do tests (POSIX.1-2024)

```{task} Add timeout and do tests (POSIX.1-2024)
:id: TSK_0203
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `timeout` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/timeout.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0204 — Add `touch` and do tests (POSIX.1-2024)

```{task} Add touch and do tests (POSIX.1-2024)
:id: TSK_0204
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `touch` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/touch.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0205 — Add `tput` and do tests (POSIX.1-2024)

```{task} Add tput and do tests (POSIX.1-2024)
:id: TSK_0205
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tput` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tput.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0206 — Add `tr` and do tests (POSIX.1-2024)

```{task} Add tr and do tests (POSIX.1-2024)
:id: TSK_0206
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tr` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tr.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0207 — Add `true` and do tests (POSIX.1-2024)

```{task} Add true and do tests (POSIX.1-2024)
:id: TSK_0207
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `true` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/true.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0208 — Add `tsort` and do tests (POSIX.1-2024)

```{task} Add tsort and do tests (POSIX.1-2024)
:id: TSK_0208
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tsort` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tsort.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0209 — Add `tty` and do tests (POSIX.1-2024)

```{task} Add tty and do tests (POSIX.1-2024)
:id: TSK_0209
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `tty` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/tty.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0210 — Add `type` and do tests (POSIX.1-2024)

```{task} Add type and do tests (POSIX.1-2024)
:id: TSK_0210
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `type` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/type.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0211 — Add `ulimit` and do tests (POSIX.1-2024)

```{task} Add ulimit and do tests (POSIX.1-2024)
:id: TSK_0211
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `ulimit` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ulimit.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0212 — Add `umask` and do tests (POSIX.1-2024)

```{task} Add umask and do tests (POSIX.1-2024)
:id: TSK_0212
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `umask` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/umask.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0213 — Add `unalias` and do tests (POSIX.1-2024)

```{task} Add unalias and do tests (POSIX.1-2024)
:id: TSK_0213
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `unalias` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unalias.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0214 — Add `uname` and do tests (POSIX.1-2024)

```{task} Add uname and do tests (POSIX.1-2024)
:id: TSK_0214
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uname` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uname.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0215 — Add `uncompress` and do tests (POSIX.1-2024)

```{task} Add uncompress and do tests (POSIX.1-2024)
:id: TSK_0215
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uncompress` utility for full POSIX.1-2024 (Issue 8) compatibility. Add
a test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uncompress.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0216 — Add `unexpand` and do tests (POSIX.1-2024)

```{task} Add unexpand and do tests (POSIX.1-2024)
:id: TSK_0216
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `unexpand` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unexpand.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0217 — Add `unget` and do tests (POSIX.1-2024)

```{task} Add unget and do tests (POSIX.1-2024)
:id: TSK_0217
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `unget` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unget.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0218 — Add `uniq` and do tests (POSIX.1-2024)

```{task} Add uniq and do tests (POSIX.1-2024)
:id: TSK_0218
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uniq` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uniq.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0219 — Add `unlink` and do tests (POSIX.1-2024)

```{task} Add unlink and do tests (POSIX.1-2024)
:id: TSK_0219
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `unlink` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/unlink.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0220 — Add `uucp` and do tests (POSIX.1-2024)

```{task} Add uucp and do tests (POSIX.1-2024)
:id: TSK_0220
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uucp` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uucp.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UU option
group, put it behind the feature macro.
```

### TSK_0221 — Add `uudecode` and do tests (POSIX.1-2024)

```{task} Add uudecode and do tests (POSIX.1-2024)
:id: TSK_0221
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uudecode` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uudecode.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0222 — Add `uuencode` and do tests (POSIX.1-2024)

```{task} Add uuencode and do tests (POSIX.1-2024)
:id: TSK_0222
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uuencode` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uuencode.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0223 — Add `uustat` and do tests (POSIX.1-2024)

```{task} Add uustat and do tests (POSIX.1-2024)
:id: TSK_0223
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uustat` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uustat.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UU option
group, put it behind the feature macro.
```

### TSK_0224 — Add `uux` and do tests (POSIX.1-2024)

```{task} Add uux and do tests (POSIX.1-2024)
:id: TSK_0224
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `uux` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/uux.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UU option
group, put it behind the feature macro.
```

### TSK_0225 — Add `val` and do tests (POSIX.1-2024)

```{task} Add val and do tests (POSIX.1-2024)
:id: TSK_0225
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `val` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/val.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0226 — Add `vi` and do tests (POSIX.1-2024)

```{task} Add vi and do tests (POSIX.1-2024)
:id: TSK_0226
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `vi` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/vi.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the UP option
group, put it behind the feature macro.
```

### TSK_0227 — Add `wait` and do tests (POSIX.1-2024)

```{task} Add wait and do tests (POSIX.1-2024)
:id: TSK_0227
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `wait` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/wait.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0228 — Add `wc` and do tests (POSIX.1-2024)

```{task} Add wc and do tests (POSIX.1-2024)
:id: TSK_0228
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `wc` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/wc.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0229 — Add `what` and do tests (POSIX.1-2024)

```{task} Add what and do tests (POSIX.1-2024)
:id: TSK_0229
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `what` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/what.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0230 — Add `who` and do tests (POSIX.1-2024)

```{task} Add who and do tests (POSIX.1-2024)
:id: TSK_0230
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `who` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/who.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

### TSK_0231 — Add `write` and do tests (POSIX.1-2024)

```{task} Add write and do tests (POSIX.1-2024)
:id: TSK_0231
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `write` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/write.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0232 — Add `xargs` and do tests (POSIX.1-2024)

```{task} Add xargs and do tests (POSIX.1-2024)
:id: TSK_0232
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `xargs` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/xargs.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition.
```

### TSK_0233 — Add `xgettext` and do tests (POSIX.1-2024)

```{task} Add xgettext and do tests (POSIX.1-2024)
:id: TSK_0233
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `xgettext` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a
test suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/xgettext.html`.
Include each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the CD option
group, put it behind the feature macro.
```

### TSK_0234 — Add `yacc` and do tests (POSIX.1-2024)

```{task} Add yacc and do tests (POSIX.1-2024)
:id: TSK_0234
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `yacc` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/yacc.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the CD option
group, put it behind the feature macro.
```

### TSK_0235 — Add `zcat` and do tests (POSIX.1-2024)

```{task} Add zcat and do tests (POSIX.1-2024)
:id: TSK_0235
:status: open
:traces: BDR_0003, SYS_LIBC_0001, RDM_0012

Add the `zcat` utility for full POSIX.1-2024 (Issue 8) compatibility. Add a test
suite for the full interface in the specification at
`https://pubs.opengroup.org/onlinepubs/9799919799/utilities/zcat.html`. Include
each command-line option, operand, and synopsis. Make sure that `stdin`,
`stdout`, `stderr`, input files, output files, and `V1_chap08` environment
parameters operate correctly. Do a check of asynchronous signal operation and
the exit status for each condition. Because this utility is in the XSI option
group, put it behind the feature macro.
```

