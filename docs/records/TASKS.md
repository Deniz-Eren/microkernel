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

### TSK_0002 — Implement Milestone 5 process and memory syscalls

```{task} Implement Milestone 5 process and memory syscalls
:id: TSK_0002
:status: open
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004

Implement `ProcCreate`, `VMMap`, `VMUnmap`, `VMCloneCow`, child exit-code
reporting, and automated QEMU verification tests to lay the foundation for POSIX
`mmap` and `waitpid` ({need}`SWR_MEM_0004`, {need}`SWR_PROC_0004`).
```

### TSK_0003 — Align signal ABI, masks, and SIGKILL/SIGSTOP rules

```{task} Align signal ABI, masks, and SIGKILL/SIGSTOP rules
:id: TSK_0003
:status: open
:traces: ADR_0003, SWR_SIG_0003, SWR_SIG_0004

Add `<signal.h>` with standard `sigaction` and `kill` signatures, reject
`SIGKILL`/`SIGSTOP` overrides with `-EINVAL` in `sys_sigaction`, save/restore
`blocked_signals` in `sig_frame_t`, and audit async-signal-safety
({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```
