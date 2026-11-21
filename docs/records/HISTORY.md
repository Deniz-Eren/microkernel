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

### TSK_0006 — Implement POSIX.1-2024 pthreads, semaphores, and FS.base TLS

```{task} Implement POSIX.1-2024 pthreads, semaphores, and FS.base TLS
:id: TSK_0006
:status: done
:traces: ADR_0003, SWR_PROC_0005

Add per-thread `FS.base` TLS, `<pthread.h>` (`pthread_create`, `pthread_join`,
mutexes, `pthread_cond_*`, `pthread_cond_clockwait`), and `<semaphore.h>`
(`sem_*`) on top of kernel wait queues ({need}`SWR_PROC_0005`).
```

### TSK_0005 — Implement POSIX.1-2024 monotonic clocks and timers

```{task} Implement POSIX.1-2024 monotonic clocks and timers
:id: TSK_0005
:status: done
:traces: ADR_0003, SWR_PROC_0006

Add kernel monotonic and realtime clock syscalls and `<time.h>`
(`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`,
`timer_settime`) for `CLOCK_MONOTONIC` and `CLOCK_REALTIME`
({need}`SWR_PROC_0006`).
```

### RDM_0002 — Milestone 6: Interrupts and user-space drivers

```{roadmap} Milestone 6: Interrupts and user-space drivers
:id: RDM_0002
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004, SWR_IPC_0005

Deliver hardware interrupt events as IPC messages (`AttachIntrEvent`), support
`IOPLSet` and multi-buffer `ReadMsgV`/`WriteMsgV`, and run a Ring 3 QEMU device
driver ({need}`SYS_IPC_0001`).
```

### TSK_0004 — Implement Milestone 6 interrupts, port I/O, and Ring 3 driver

```{task} Implement Milestone 6 interrupts, port I/O, and Ring 3 driver
:id: TSK_0004
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004

Implement `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`, and multi-buffer
`ReadMsgV`/`WriteMsgV`, and verify a Ring 3 QEMU device driver in headless QEMU
({need}`SYS_IPC_0001`, {need}`SWR_IPC_0004`).
```

### RDM_0001 — Milestone 5: Processes and memory system calls

```{roadmap} Milestone 5: Processes and memory system calls
:id: RDM_0001
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004

Add user-space process spawning (`ProcCreate`), virtual memory mapping (`VMMap`,
`VMUnmap`), copy-on-write cloning (`VMCloneCow`), and parent exit-code
collection ({need}`SYS_PROC_0001`, {need}`SYS_MEM_0001`).
```

### TSK_0002 — Implement Milestone 5 process and memory syscalls

```{task} Implement Milestone 5 process and memory syscalls
:id: TSK_0002
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004

Implement `ProcCreate`, `VMMap`, `VMUnmap`, `VMCloneCow`, child exit-code
reporting, and automated QEMU verification tests to lay the foundation for POSIX
`mmap` and `waitpid` ({need}`SWR_MEM_0004`, {need}`SWR_PROC_0004`).
```

### TSK_0003 — Align signal ABI, masks, and SIGKILL/SIGSTOP rules

```{task} Align signal ABI, masks, and SIGKILL/SIGSTOP rules
:id: TSK_0003
:status: done
:traces: ADR_0003, SWR_SIG_0003, SWR_SIG_0004

Add `<signal.h>` with POSIX `sigaction` and `kill`, and reject `SIGKILL` and
`SIGSTOP` in `sys_sigaction`. Keep `blocked_signals` in `sig_frame_t` and audit
async-signal-safety ({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```

### TSK_0001 — Align libc headers and thread-local errno to POSIX.1-2024

```{task} Align libc headers and thread-local errno to POSIX.1-2024
:id: TSK_0001
:status: done
:traces: BDR_0003, ADR_0003, SWR_LIBC_0002, SWR_LIBC_0003, SWR_DOCS_0003

Remove `<sys/microkernel.h>` leakage from `<unistd.h>` and set `_POSIX_VERSION`
and `_POSIX2_VERSION` to `202405L`. Add `<errno.h>` with thread-local `errno`
and the standalone header gate ({need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`,
{need}`SWR_DOCS_0003`).
```
