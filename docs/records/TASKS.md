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

### TSK_0017 — Build Ring 3 POSIX.1-2024 XCU shell (sh) and core utilities

```{task} Build Ring 3 POSIX.1-2024 XCU shell (sh) and core utilities
:id: TSK_0017
:status: open
:traces: ADR_0003, SWR_LIBC_0006

Implement Ring 3 POSIX `sh` and minimal core utilities (`echo`, `cat`, `ls`,
`true`, `false`, `test`) supporting pipelines, file redirections, `$'...'`
quoting, `set -o pipefail`, and `read -d` ({need}`SWR_LIBC_0006`).
```

### TSK_0021 — Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers

```{task} Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers
:id: TSK_0021
:status: open
:traces: ADR_0003, SWR_BOOT_0003, SWR_IPC_0008

Parse ACPI `MADT` IOAPIC bases and Interrupt Source Overrides (ISOs) plus `SPCR`
serial configuration. Parameterize `ttyserv` (`-p`/`-i`) and `rtcdrv` instead of
hardcoded ISA addresses, and route user `stdout`/`stderr` TX (`OPOST`/`ONLCR`)
through `ttyserv` ({need}`SWR_BOOT_0003`).
```

### TSK_0020 — Add pselect and end-to-end cross-subsystem POSIX hardening

```{task} Add pselect and end-to-end cross-subsystem POSIX hardening
:id: TSK_0020
:status: open
:traces: ADR_0003, WDR_0002, SWR_DOCS_0004

Add `<sys/select.h>` (`select`, `pselect`) over multi-server `ppoll()`. Run an
integrated QEMU stress and leak suite across `sh`, VFS, pipes, TTY, `AF_UNIX`
sockets, `SHM`, signals, and pthreads ({need}`SWR_DOCS_0004`).
```
