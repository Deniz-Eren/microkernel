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

### RDM_0003 — POSIX.1-2024 (Issue 8) Base conformance

```{roadmap} POSIX.1-2024 (Issue 8) Base conformance
---
id: RDM_0003
status: open
traces: >
  BDR_0003, ADR_0003, SYS_LIBC_0001, SYS_PROC_0001, SYS_SIG_0001,
  SYS_MEM_0001, SYS_IPC_0001
---

Reach IEEE Std 1003.1-2024 Base conformance across headers, `errno`, signals,
`pthread_*`, `sem_*`, clocks/timers, `mmap`, `pipe2`/`dup3`/`ppoll`, and
`aio_*`. Cover future XCU shell utilities (`$'...'`, `set -o pipefail`, `read
-d`) with automated gates ({need}`BDR_0003`, {need}`ADR_0003`).
```
