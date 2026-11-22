# System and Software Requirements — PROC

Domain `PROC`: process and thread management, preemptive scheduling, fast
syscall dispatch, and POSIX.1-2024 threads, clocks, and timers.

```{sys} Run isolated Ring 3 processes and preemptive threads
:id: SYS_PROC_0001
:status: active

The system must run isolated Ring 3 processes and schedule threads across 256
priority levels. It must dispatch syscalls through the x86-64 `syscall`/`sysret`
ABI.
```

```{swr} Schedule runnable threads across 256 priority levels
:id: SWR_PROC_0001
:status: active
:derives: SYS_PROC_0001

The scheduler must select the highest-priority runnable thread preemptively. It
must count time slices and do a context switch cleanly ({need}`SYS_PROC_0001`).
```

```{swr} Dispatch Ring 3 syscalls with syscall and sysret
:id: SWR_PROC_0002
:status: active
:derives: SYS_PROC_0001

The kernel must set the x86-64 `syscall`/`sysret` MSRs and dispatch correct
syscall numbers in `RAX`. It must give `-ENOSYS` for unknown syscall numbers
({need}`SYS_PROC_0001`).
```

```{swr} Supply two-phase Ring 3 thread creation and inspection
:id: SWR_PROC_0003
:status: active
:derives: SYS_PROC_0001

The kernel must make unarmed embryo threads with `NewThread` and arm them with
`ThreadReset` on a kernel-allocated user stack. `ThreadGetCtx` must give thread
state ({need}`SYS_PROC_0001`).
```

```{swr} Supply POSIX.1-2024 Base process operations and optional spawn
:id: SWR_PROC_0004
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/fork (BASE), XBD/spawn.h (SPN)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003, ADR_0011

The process subsystem must supply POSIX.1-2024 Base process creation, program
load, `waitpid` ([sys/wait.h][hdr-wait]), and `_Exit` on `ProcCreate` and
`VMCloneCow`. It must put `posix_spawn*` ([spawn.h][hdr-spawn], `SPN`) behind
`_POSIX_SPAWN` ({need}`SYS_PROC_0001`).
```

```{swr} Supply POSIX.1-2024 Base pthreads and semaphores
:id: SWR_PROC_0005
:status: active
:posix: XBD/pthread.h (BASE), XBD/semaphore.h (BASE), XSH/2.9
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<pthread.h>` ([pthread.h][hdr-pth]) and `<semaphore.h>`
([semaphore.h][hdr-sem]) must declare the Issue 8 Base thread suite (mutexes,
`pthread_cond_*`, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks) and
`sem_*` semaphores. Synchronization must obey [XSH 2.9][xsh-02] memory rules
({need}`SYS_PROC_0001`).
```

```{swr} Supply POSIX.1-2024 Base monotonic clocks and timers
:id: SWR_PROC_0006
:status: active
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<time.h>` ([time.h][hdr-time]) must declare mandatory Issue 8 Base functions
`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`, and
`timer_settime`. These functions must operate on `CLOCK_MONOTONIC` and
`CLOCK_REALTIME` with no option guard ({need}`SYS_PROC_0001`).
```

```{swr} Spawn bootfs child processes and report exit status with waitpid
:id: SWR_PROC_0007
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/waitpid (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

The kernel must spawn bootfs child processes with `ProcCreate` and report child
exit status with `ProcWait`. `<sys/wait.h>` ([sys/wait.h][hdr-wait]) must
declare async-signal-safe `wait` and `waitpid` ([waitpid][fn-waitpid])
({need}`SYS_PROC_0001`).
```

```{swr} Supply process groups, TTY signal control, and core utilities
:id: SWR_PROC_0008
:status: active
:posix: XSH/setpgid (BASE), XSH/tcsetpgrp (BASE), XBD/termios.h (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

The kernel and `ttyserv` must supply process groups (`setpgid`, `getpgid`,
`getpgrp`, `setsid`, `getsid`, `tcgetpgrp`, `tcsetpgrp`), `ISIG` (`SIGINT` on
`Ctrl-C`), `waitpid(0)`/`waitpid(-pgid)`, and `kill(-pgid)`. Interactive `sh`
must manage foreground process groups, and Ring 3 must supply `mkdir`, `rm`,
`cp`, `mv`, `uname`, `env`, `wc`, and `head` ({need}`SYS_PROC_0001`).
```

```{swr} Boot secondary CPUs via ACPI MADT and run SMP scheduling
:id: SWR_PROC_0009
:status: draft
:derives: SYS_PROC_0001
:traces: ADR_0003, ADR_0022, ADR_0024

The kernel must start secondary CPUs behind portable HAL hooks (`INIT`/`SIPI`
and `GS.base` on `x86-64`, `PSCI` and `TPIDR_EL1` for future `arm64`). Kernel
locks for `pmm`, `vmm`, `sched`, `ipc`, and `futex` must run safely across
multiple cores ({need}`SYS_PROC_0001`).
```

[hdr-wait]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_wait.h.html
[fn-waitpid]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/waitpid.html
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
