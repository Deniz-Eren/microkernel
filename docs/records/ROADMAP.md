# Roadmap

## Purpose

Record planned milestones and roadmap entries (`roadmap`) for `microkernel`.

## Structure

Put roadmap items in priority sequence and connect them to affected
specifications through `:traces:`.

## Format Rules

- Normative specifications and records must not connect to `RDM_` items.
- Move completed or canceled roadmap items verbatim to [HISTORY.md](HISTORY.md)
  with status `done` or `cancelled`.

```{needtable} All roadmap items
:types: roadmap
:filter: docname == 'records/ROADMAP'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### RDM_0009 — Ring 3 dynamic ELF loader and shared libraries

```{roadmap} Ring 3 dynamic ELF loader and shared libraries
---
id: RDM_0009
status: open
traces: >
  BDR_0003, BDR_0004, ADR_0002, ADR_0054, ADR_0055, ADR_0056, SYS_BOOT_0001,
  SYS_PROC_0001, SYS_MEM_0001, SYS_IPC_0001, SYS_LIBC_0001
---

Add kernel `PT_INTERP` and SysV `auxv` handoff with no new Ring 0 syscalls. Make
a freestanding Ring 3 `/boot/ld.so` dynamic linker. Compile shared `libc.so`,
`libz.so.1`, `libcrypto.so.3`, and `libssl.so.3` into the sysroot and
`boot.tar`. Link OpenSSH Portable and `bin/` utilities dynamically while boot
servers stay static ({need}`ADR_0002`, {need}`ADR_0054`).
```

### RDM_0011 — Post-POSIX runtime utilities and third-party user-space tools

```{roadmap} Post-POSIX runtime utilities and third-party user-space tools
---
id: RDM_0011
status: open
traces: >
  WDR_0012, BDR_0003, BDR_0004, ADR_0002, ADR_0054, SYS_LIBC_0001
---

Add post-POSIX runtime utilities in two groups. Make OS-native Ring 3 utilities
in the repository: `ldd` through `/boot/ld.so`, `ldconfig` in `bin/`, `getent`
in `bin/`, and built-in `C.UTF-8` locale data in `libc` ({need}`WDR_0012`,
{need}`BDR_0003`). Make out-of-tree CMake ports for third-party user-space
tools: `zic`, `zdump`, and `tzselect` from IANA `tzcode`, and `tar` from
`libarchive` (`bsdtar`). CMake downloads their archives during the build and
does not keep upstream source in git ({need}`WDR_0012`, {need}`BDR_0004`,
{need}`ADR_0054`).
```

### RDM_0012 — Full POSIX.1-2024 utility compatibility

```{roadmap} Full POSIX.1-2024 utility compatibility
---
id: RDM_0012
status: open
traces: >
  BDR_0003, SYS_LIBC_0001
---

Add and do tests for all POSIX.1-2024 (Issue 8) utilities from the
`tests/posix/issue8-manifest.tsv` manifest. Each utility gets a task in
`docs/records/TASKS.md`. That task includes options, operands, synopsis lines,
`stdin`, `stdout`, `stderr`, input files, output files, environment parameters,
asynchronous signals, and exit status.
```

### RDM_0008 — Platform abstraction and future 64-bit ARM (arm64) target

```{roadmap} Platform abstraction and future 64-bit ARM (arm64) target
---
id: RDM_0008
status: open
traces: >
  ADR_0024, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001, SYS_IPC_0001,
  SYS_LIBC_0001
---

Keep `x86-64` as the active target and isolate architecture code in `x86-64`
platform folders. Add a future 64-bit ARM (`arm64`) target for QEMU `virt`
(`EL1`/`EL0`, `TTBR0`/`TTBR1`, GIC, Generic Timer, `svc #0`, `TPIDR_EL0`, and
MMIO `PL011` UART) with no 32-bit `x86` ({need}`ADR_0024`).
```

### RDM_0010 — QEMU-first ARM64 startup and automatic Raspberry Pi 400 HIL

```{roadmap} QEMU-first ARM64 startup and automatic Raspberry Pi 400 HIL
---
id: RDM_0010
status: open
traces: >
  ADR_0024, ADR_0058, WDR_0011, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001,
  SYS_IPC_0001
---

Start the ARM64 (BCM2711) port in QEMU `-M raspi4b` behind a selectable
`ConsoleBackend` with the PL011 UART as the first backend. Add the USB 2.0 and
CDC-ACM protocol module, the Synopsys DWC2 core driver, and the BCM2711 adapter
behind `EARLY_USB_CDC_CONSOLE` ({need}`ADR_0058`). Make the board-neutral
`tools/hil/` host tool with one configuration file for each board. Configure the
Pi 400 one time through SSH, then do the automatic one-cable HIL loop and make
bare-metal drivers (VideoCore Mailbox HDMI framebuffer, PCIe root complex, and
VL805 xHCI) through the four-step workflow ({need}`WDR_0011`, {need}`ADR_0024`).
```
