# History

## Purpose

Archive completed and cancelled planning entries (`task`, `roadmap`) moved from
[TASKS.md](TASKS.md) and [ROADMAP.md](ROADMAP.md).

## Structure

Entries are stored newest first without modifying their original IDs or links.

## Formatting Rules

- Set the status of every archived item to `done` or `cancelled`.
- Never reuse IDs from archived entries.

```{needtable} Completed tasks and roadmap items
:types: task, roadmap
:filter: docname == 'records/HISTORY' and status in ['done', 'cancelled']
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0003 — Align signal ABI, masks, and SIGKILL/SIGSTOP rules

```{task} Align signal ABI, masks, and SIGKILL/SIGSTOP rules
:id: TSK_0003
:status: done
:traces: ADR_0003, SWR_SIG_0003, SWR_SIG_0004

Add `<signal.h>` with standard `sigaction` and `kill` signatures, reject
`SIGKILL`/`SIGSTOP` overrides with `-EINVAL` in `sys_sigaction`, save/restore
`blocked_signals` in `sig_frame_t`, and audit async-signal-safety
({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```

### TSK_0001 — Align libc headers and thread-local errno to POSIX.1-2024

```{task} Align libc headers and thread-local errno to POSIX.1-2024
:id: TSK_0001
:status: done
:traces: BDR_0003, ADR_0003, SWR_LIBC_0002, SWR_LIBC_0003, SWR_DOCS_0003

Remove `<sys/microkernel.h>` leakage from `<unistd.h>`, define `_POSIX_VERSION`
and `_POSIX2_VERSION` as `202405L`, add `<errno.h>` with thread-local `errno`,
and add the standalone header compilation gate ({need}`SWR_LIBC_0002`,
{need}`SWR_LIBC_0003`, {need}`SWR_DOCS_0003`).
```
