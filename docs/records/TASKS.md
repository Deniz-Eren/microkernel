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

### TSK_0009 — Implement POSIX.1-2024 Base asynchronous I/O (<aio.h>)

```{task} Implement POSIX.1-2024 Base asynchronous I/O (<aio.h>)
:id: TSK_0009
:status: open
:traces: ADR_0003, SWR_IPC_0005

Add `<aio.h>` (`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_fsync`, `aio_suspend`, `lio_listio`) backed by Ring 3 worker threads or
servers. Keep `<mqueue.h>` behind `_POSIX_MESSAGE_PASSING`
({need}`SWR_IPC_0005`).
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
