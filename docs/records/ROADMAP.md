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

### RDM_0003 — Work toward POSIX.1-2024 (Issue 8) Base compatibility

```{roadmap} Work toward POSIX.1-2024 (Issue 8) Base compatibility
---
id: RDM_0003
status: open
traces: >
  BDR_0003, ADR_0003, SYS_LIBC_0001, SYS_PROC_0001, SYS_SIG_0001,
  SYS_MEM_0001, SYS_IPC_0001
---

Work toward compatibility with interfaces from POSIX.1-2024 (IEEE Std
1003.1-2024 / ISO/IEC/IEEE 9945:2026) across headers, `errno`, signals,
`pthread_*`, `sem_*`, clocks/timers, `mmap`, `pipe2`/`dup3`/`ppoll`, and
`aio_*`. Cover command shell utilities (`$'...'`, `set -o pipefail`, `read -d`)
with automated compatibility gates ({need}`BDR_0003`, {need}`ADR_0003`).
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
