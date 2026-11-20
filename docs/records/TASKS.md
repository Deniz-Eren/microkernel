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

### TSK_0001 — Align libc headers and thread-local errno to POSIX.1-2024

```{task} Align libc headers and thread-local errno to POSIX.1-2024
:id: TSK_0001
:status: open
:traces: BDR_0003, ADR_0003, SWR_LIBC_0002, SWR_LIBC_0003, SWR_DOCS_0003

Remove `<sys/microkernel.h>` leakage from `<unistd.h>`, define `_POSIX_VERSION`
and `_POSIX2_VERSION` as `202405L`, add `<errno.h>` with thread-local `errno`,
and add the standalone header compilation gate ({need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0003`, {need}`SWR_DOCS_0003`).
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
