# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must never link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0029 — Enforce x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512

```{task} Enforce x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512
:id: TSK_0029
:status: open
:traces: ADR_0024, ADR_0026, SWR_BOOT_0005

Add early `CPUID` gating for `x86-64-v3` (`XSAVE`, `AVX`, `AVX2`, `FMA`,
`BMI1`/`BMI2`) that halts legacy CPUs on boot. Upgrade `switch.S` to
`XSAVE`/`XRSTOR` with optional `AVX-512` `XCR0` detection, build Ring 3 with
`-march=x86-64-v3`, and add positive and negative QEMU tests
({need}`SWR_BOOT_0005`).
```

### TSK_0030 — Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA support

```{task} Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA support
:id: TSK_0030
:status: open
:traces: ADR_0003, ADR_0026, SWR_LIBC_0009

Add `<fenv.h>` (`fegetround`, `fesetround`, exception flags) and `<math.h>`
(`sqrt`, `fma`, rounding, classification, `exp`/`log`/`pow`, and trigonometric
functions including `sin`/`cos`/`tan`/`asin`/`acos`/`atan`) using `x86-64-v3`
hardware `FMA` with Gates 1 to 3 and QEMU verification ({need}`SWR_LIBC_0009`).
```

### TSK_0024 — Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver

```{task} Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver
:id: TSK_0024
:status: open
:traces: ADR_0003, ADR_0008, ADR_0016, SWR_IPC_0010

Add `CAP_SYS_INTR` MMIO device mapping, PCI configuration, and DMA buffer
syscalls. Build a Ring 3 PCI `virtio-blk` driver (`/dev/blk`) using IOAPIC IRQ
pulses, and verify sector persistence across QEMU restarts
({need}`SWR_IPC_0010`).
```

### TSK_0028 — Mount writable on-disk file system in vfsserv over /dev/blk

```{task} Mount writable on-disk file system in vfsserv over /dev/blk
:id: TSK_0028
:status: open
:traces: ADR_0003, ADR_0016, SWR_IPC_0011

Mount a writable on-disk file system (such as `ext2`) inside `vfsserv` over
`/dev/blk` so files and directories stay across QEMU restarts
({need}`SWR_IPC_0011`).
```

### TSK_0025 — Boot secondary CPUs via ACPI MADT and harden SMP locks

```{task} Boot secondary CPUs via ACPI MADT and harden SMP locks
:id: TSK_0025
:status: open
:traces: ADR_0003, ADR_0022, ADR_0024, SWR_PROC_0009

Start secondary CPUs under `-smp 2` behind portable HAL hooks (`INIT`/`SIPI` and
`GS.base` on `x86-64`). Harden `pmm`, `vmm`, `sched`, `ipc`, and `futex` locks
under concurrent multi-core QEMU tests ({need}`SWR_PROC_0009`).
```
