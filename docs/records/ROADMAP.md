# Roadmap

## Purpose

Track planned milestones and roadmap entries (`roadmap`) for `microkernel`.

## Structure

Order roadmap items by priority and link them to affected specifications through
`:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `RDM_` items.
- Move completed or cancelled roadmap items verbatim into
  [HISTORY.md](HISTORY.md) with status `done` or `cancelled`.

```{needtable} All roadmap items
:types: roadmap
:filter: docname == 'records/ROADMAP'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### RDM_0009 — Ring 3 dynamic linking and shared libraries

```{roadmap} Ring 3 dynamic linking and shared libraries
---
id: RDM_0009
status: open
traces: >
  BDR_0003, BDR_0004, ADR_0002, ADR_0054, ADR_0055, ADR_0056, SYS_BOOT_0001,
  SYS_PROC_0001, SYS_MEM_0001, SYS_IPC_0001, SYS_LIBC_0001
---

Add kernel `PT_INTERP` and SysV `auxv` handoff with zero new Ring 0 syscalls.
Build a freestanding Ring 3 `/boot/ld.so` dynamic linker. Build shared
`libc.so`, `libz.so.1`, `libcrypto.so.3`, and `libssl.so.3` into the sysroot and
`boot.tar`. Link OpenSSH Portable and `bin/` utilities dynamically while keeping
core boot servers static ({need}`ADR_0002`, {need}`ADR_0054`).
```

### RDM_0008 — Platform abstraction and future 64-bit ARM (arm64) support

```{roadmap} Platform abstraction and future 64-bit ARM (arm64) support
---
id: RDM_0008
status: open
traces: >
  ADR_0024, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001, SYS_IPC_0001,
  SYS_LIBC_0001
---

Keep `x86-64` as the active target while isolating architecture code under
`x86-64` platform folders. Add future 64-bit ARM (`arm64`) support for QEMU
`virt` (`EL1`/`EL0`, `TTBR0`/`TTBR1`, GIC, Generic Timer, `svc #0`, `TPIDR_EL0`,
and MMIO `PL011` UART) with no legacy 32-bit `x86` ({need}`ADR_0024`).
```

### RDM_0010 — QEMU-first ARM64 bring-up and autonomous Raspberry Pi 400 HIL

```{roadmap} QEMU-first ARM64 bring-up and autonomous Raspberry Pi 400 HIL
---
id: RDM_0010
status: open
traces: >
  ADR_0024, ADR_0058, WDR_0011, SYS_BOOT_0001, SYS_MEM_0001, SYS_PROC_0001,
  SYS_IPC_0001
---

Bring up the ARM64 (BCM2711) port in QEMU `-M raspi4b` behind a swappable
`ConsoleBackend` with the PL011 UART as the first backend. Implement the
platform-agnostic USB 2.0 and CDC-ACM protocol engine, the reusable Synopsys
DWC2 slave/PIO core driver, and the BCM2711 board glue behind
`EARLY_USB_CDC_CONSOLE` ({need}`ADR_0058`). Build the board-agnostic
`tools/hil/` host runner with per-board configuration files. Provision the Pi
400 once over SSH, then run the autonomous single-cable HIL loop and develop
bare-metal drivers (VideoCore Mailbox HDMI framebuffer, PCIe root complex, and
VL805 xHCI) through the four-phase workflow ({need}`WDR_0011`,
{need}`ADR_0024`).
```
