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

### TSK_0001 — Align requirements and libc to POSIX.1-2024 (Issue 8)

```{task} Align requirements and libc to POSIX.1-2024 (Issue 8)
:id: TSK_0001
:status: open
:traces: BDR_0003, SYS_LIBC_0001, SWR_LIBC_0002

Review `microkernel` requirements, public headers, error numbers, signals, and
threads against IEEE Std 1003.1-2024 using the `posix-issue8-os-dev` skill
({need}`BDR_0003`, {need}`SYS_LIBC_0001`, {need}`SWR_LIBC_0002`).
```

### TSK_0002 — Implement Milestone 5 process and memory syscalls

```{task} Implement Milestone 5 process and memory syscalls
:id: TSK_0002
:status: open
:traces: SYS_PROC_0001, SYS_MEM_0001

Implement `ProcCreate`, `VMMap`, `VMUnmap`, `VMCloneCow`, child exit-code
reporting, and automated QEMU verification tests ({need}`SYS_PROC_0001`,
{need}`SYS_MEM_0001`).
```
