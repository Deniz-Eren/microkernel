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

### TSK_0010 — Add pthread cancellation points and thread-safe <stdio.h> locking

```{task} Add pthread cancellation points and thread-safe <stdio.h> locking
:id: TSK_0010
:status: open
:traces: ADR_0003, SWR_LIBC_0004

Add `pthread_cancel`, `pthread_setcancelstate`, `pthread_setcanceltype`,
`pthread_testcancel`, and `<stdio.h>` stream locking (`flockfile`,
`funlockfile`) with QEMU and CTest verification ({need}`SWR_LIBC_0004`).
```

### TSK_0011 — Implement POSIX.1-2024 XCU shell utility baseline

```{task} Implement POSIX.1-2024 XCU shell utility baseline
:id: TSK_0011
:status: open
:traces: ADR_0003, SWR_LIBC_0005

Build a Ring 3 POSIX shell supporting `$'...'` quoting, `set -o pipefail`, and
`read -d` with QEMU verification ({need}`SWR_LIBC_0005`).
```
