# System and Software Requirements — PROC

Domain `PROC`: Process and thread lifecycle, preemptive priority scheduling,
fast system call dispatch, and POSIX.1-2024 concurrency, clocks, and timers.

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

```{swr} Provide POSIX.1-2024 Base process lifecycle and optional spawn
:id: SWR_PROC_0004
:status: draft
:posix: XBD/sys/wait.h (BASE), XSH/fork (BASE), XBD/spawn.h (SPN)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

The process subsystem shall support POSIX.1-2024 Base process creation, image
execution, exit-status reaping (`waitpid` in [sys/wait.h][hdr-wait]), and
`_Exit` atop `ProcCreate` and `VMCloneCow`, and gate `posix_spawn*`
([spawn.h][hdr-spawn], `SPN`) behind `_POSIX_SPAWN` ({need}`SYS_PROC_0001`).
```

```{swr} Provide POSIX.1-2024 Base pthreads and semaphores with memory ordering
:id: SWR_PROC_0005
:status: draft
:posix: XBD/pthread.h (BASE), XBD/semaphore.h (BASE), XSH/2.9
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<pthread.h>` ([pthread.h][hdr-pth]) and `<semaphore.h>`
([semaphore.h][hdr-sem]) shall expose the mandatory Issue 8 Base thread suite
(threads, mutexes, condition variables including `pthread_cond_clockwait`,
barriers, spinlocks, rwlocks) and `sem_*` semaphores with specified
acquire/release memory ordering per [XSH 2.9][xsh-02] ({need}`SYS_PROC_0001`).
```

```{swr} Provide POSIX.1-2024 Base monotonic clocks and interval timers
:id: SWR_PROC_0006
:status: draft
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<time.h>` ([time.h][hdr-time]) shall expose mandatory Issue 8 Base clock and
timer interfaces `clock_gettime`, `clock_settime`, `clock_nanosleep`,
`timer_create`, and `timer_settime` supporting `CLOCK_MONOTONIC` and
`CLOCK_REALTIME` without an option gate ({need}`SYS_PROC_0001`).
```

[hdr-wait]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_wait.h.html
[hdr-spawn]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/spawn.h.html
[hdr-pth]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/pthread.h.html
[hdr-sem]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/semaphore.h.html
[hdr-time]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/time.h.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
