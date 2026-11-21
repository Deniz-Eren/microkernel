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

### TSK_0020 — Add pselect and end-to-end cross-subsystem POSIX hardening

```{task} Add pselect and end-to-end cross-subsystem POSIX hardening
:id: TSK_0020
:status: open
:traces: ADR_0003, WDR_0002, SWR_DOCS_0004

Add `<sys/select.h>` (`select`, `pselect`) over multi-server `ppoll()`. Verify
zero PMM frame, descriptor, and server handle leaks across `sh`, VFS, pipes,
TTY, `AF_UNIX` sockets, `SHM`, signals, and pthreads ({need}`SWR_DOCS_0004`).
```
