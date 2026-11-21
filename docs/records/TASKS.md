# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered with the highest priority first and link into the normative
model through `:traces:`.

## Formatting Rules

- Normative specifications and records must never link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0007 — Implement POSIX.1-2024 fork, execve, and pipe2/dup3/ppoll

```{task} Implement POSIX.1-2024 fork, execve, and pipe2/dup3/ppoll
:id: TSK_0007
:status: open
:traces: ADR_0003, SWR_PROC_0004, SWR_IPC_0004

Build `fork` on full-address-space `VMCloneCow`, `execve` from `boot.tar`,
per-process file descriptors, and `pipe2`/`dup3`/`ppoll` with QEMU and CTest
verification ({need}`SWR_PROC_0004`, {need}`SWR_IPC_0004`).
```
