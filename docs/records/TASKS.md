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

### TSK_0022 — Add TTY job control, ISIG (Ctrl-C), process groups, and utilities

```{task} Add TTY job control, ISIG (Ctrl-C), process groups, and utilities
:id: TSK_0022
:status: open
:traces: ADR_0003, ADR_0018, ADR_0021, SWR_PROC_0008

Add `ISIG` (`VINTR` to `SIGINT`) in `ttyserv`, process groups (`setpgid`,
`getpgid`, `setsid`, `tcgetpgrp`, `tcsetpgrp`), and `waitpid(-1)`. Add Ring 3
utilities (`mkdir`, `rm`, `cp`, `mv`, `uname`, `env`, `wc`, `head`) for the
interactive shell ({need}`SWR_PROC_0008`).
```

### TSK_0023 — Add high-leverage libc Base headers and standard C utilities

```{task} Add high-leverage libc Base headers and standard C utilities
:id: TSK_0023
:status: open
:traces: ADR_0003, WDR_0002, SWR_LIBC_0008

Register compiler freestanding headers in Gate 2 and add `<sys/utsname.h>`,
`<endian.h>`, `<sched.h>`, `<setjmp.h>`, `<sys/times.h>`, and `<fnmatch.h>`. Add
`atexit`, `qsort`, `bsearch`, `strtol`, `strtoul`, `getopt`, `alarm`, `rename`,
and `realpath` ({need}`SWR_LIBC_0008`).
```

### TSK_0024 — Build Ring 3 virtio-blk driver and writable disk mount in vfsserv

```{task} Build Ring 3 virtio-blk driver and writable disk mount in vfsserv
:id: TSK_0024
:status: open
:traces: ADR_0003, ADR_0008, ADR_0016, SWR_IPC_0010

Add a `CAP_SYS_INTR` Ring 3 PCI `virtio-blk` block driver using contiguous DMA
frames and IOAPIC IRQ pulses. Mount a writable disk file system in `vfsserv` so
files stay across QEMU restarts ({need}`SWR_IPC_0010`).
```

### TSK_0025 — Boot secondary CPUs via ACPI MADT and harden SMP locks

```{task} Boot secondary CPUs via ACPI MADT and harden SMP locks
:id: TSK_0025
:status: open
:traces: ADR_0003, ADR_0022, SWR_PROC_0009

Start secondary CPUs under `-smp 2` with LAPIC `INIT`/`SIPI` from ACPI `MADT`
and move per-CPU state to `GS.base`. Harden `pmm`, `vmm`, `sched`, `ipc`, and
`futex` locks under concurrent multi-core QEMU tests ({need}`SWR_PROC_0009`).
```
