# Architecture — PROC

Domain `PROC`: Structural architecture elements for processes, threads,
scheduling, and system call dispatch.

```{arc} Process, thread, scheduler, and syscall subsystem
:id: ARC_PROC_0001
:status: active
:satisfies: SYS_PROC_0001
:traces: ADR_0002

The process subsystem encapsulates PCB and TCB lifecycle management, preemptive
256-priority scheduling, and `syscall`/`sysret` dispatch to satisfy Ring 3
execution ({need}`SYS_PROC_0001`).
```
