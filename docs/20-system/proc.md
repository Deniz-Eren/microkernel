# System and Software Requirements — PROC

Domain `PROC`: Process and thread lifecycle, preemptive priority scheduling, and
fast system call dispatch.

```{sys} Run isolated Ring 3 processes and preemptive threads
:id: SYS_PROC_0001
:status: active

The system shall execute isolated Ring 3 processes, schedule threads across 256
priority levels, and dispatch system calls through the x86-64 `syscall`/`sysret`
ABI.
```

```{swr} Schedule runnable threads across 256 priority levels
:id: SWR_PROC_0001
:status: active
:derives: SYS_PROC_0001

The scheduler shall select the highest-priority runnable thread preemptively
with time-slice accounting and context switching ({need}`SYS_PROC_0001`).
```

```{swr} Dispatch Ring 3 system calls via syscall and sysret
:id: SWR_PROC_0002
:status: active
:derives: SYS_PROC_0001

The kernel shall configure the x86-64 `syscall`/`sysret` MSRs and dispatch valid
system call numbers in `RAX`, returning `-ENOSYS` for unimplemented numbers
({need}`SYS_PROC_0001`).
```

```{swr} Support two-phase Ring 3 thread creation and inspection
:id: SWR_PROC_0003
:status: active
:derives: SYS_PROC_0001

The kernel shall create unarmed embryo threads via `NewThread`, arm them with a
start context and kernel-allocated user stack via `ThreadReset`, and report
thread state via `ThreadGetCtx` ({need}`SYS_PROC_0001`).
```
