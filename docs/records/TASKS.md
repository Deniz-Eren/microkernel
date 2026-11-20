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

### TSK_0004 — Implement Milestone 6 interrupts, port I/O, and Ring 3 driver

```{task} Implement Milestone 6 interrupts, port I/O, and Ring 3 driver
:id: TSK_0004
:status: open
:traces: SYS_IPC_0001, SWR_IPC_0004

Implement `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`, and multi-buffer
`ReadMsgV`/`WriteMsgV`, and verify a Ring 3 QEMU device driver in headless QEMU
({need}`SYS_IPC_0001`, {need}`SWR_IPC_0004`).
```

### TSK_0005 — Implement POSIX.1-2024 monotonic clocks and timers

```{task} Implement POSIX.1-2024 monotonic clocks and timers
:id: TSK_0005
:status: open
:traces: ADR_0003, SWR_PROC_0006

Add kernel monotonic and realtime clock syscalls and `<time.h>`
(`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`,
`timer_settime`) for `CLOCK_MONOTONIC` and `CLOCK_REALTIME`
({need}`SWR_PROC_0006`).
```

### TSK_0006 — Implement POSIX.1-2024 pthreads, semaphores, and FS.base TLS

```{task} Implement POSIX.1-2024 pthreads, semaphores, and FS.base TLS
:id: TSK_0006
:status: open
:traces: ADR_0003, SWR_PROC_0005

Add per-thread `FS.base` TLS, `<pthread.h>` (`pthread_create`, `pthread_join`,
mutexes, `pthread_cond_*`, `pthread_cond_clockwait`), and `<semaphore.h>`
(`sem_*`) on top of kernel wait queues ({need}`SWR_PROC_0005`).
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
