# Components — PROC

Domain `PROC`: white-box specification for scheduling, process/thread lifecycle,
system call dispatch, and `waitpid` wrappers.

```{comp} Priority scheduler and context switcher
:id: COMP_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0001

`kernel/core/sched.c` and `kernel/arch/x86-64/switch.S` keep 256 priority ready
queues, count slice ticks on LAPIC timer interrupts, and switch kernel stacks
and FPU state ({need}`ARC_PROC_0001`).
```

```{comp} Process manager and syscall dispatcher
:id: COMP_PROC_0002
:status: active
:refines: ARC_PROC_0001, ICD_PROC_0001, ICD_PROC_0002, ICD_PROC_0003
:satisfies: SWR_PROC_0002, SWR_PROC_0003, SWR_PROC_0004, SWR_PROC_0007

`kernel/core/proc.c` and `kernel/core/syscall.c` manage PCB/TCB lifecycle,
`sys_proc_create`, `sys_proc_fork`, `sys_proc_execve`, `sys_proc_wait`,
`sys_fd_op`, and `syscall_handler` dispatch ({need}`ICD_PROC_0001`,
{need}`ICD_PROC_0002`, {need}`ICD_PROC_0003`, {need}`ARC_PROC_0001`).
```

```{comp} Async-signal-safe libc waitpid wrappers
:id: COMP_PROC_0003
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/waitpid (BASE)
:refines: ARC_PROC_0001, ICD_PROC_0003, ICD_PROC_0004
:satisfies: SWR_PROC_0007

`libc/src/wait.c` gives async-signal-safe `wait` and `waitpid` over `ProcWait`
with no heap allocation or locks ({need}`ICD_PROC_0003`, {need}`ICD_PROC_0004`,
{need}`SWR_PROC_0007`).
```

```{comp} Kernel monotonic/realtime clocks, timers, and libc <time.h> wrappers
:id: COMP_PROC_0004
:status: active
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:refines: ARC_PROC_0001, ICD_PROC_0005
:satisfies: SWR_PROC_0006

`kernel/core/time.c` tracks 1 ms LAPIC ticks for `CLOCK_MONOTONIC` and
`CLOCK_REALTIME`, wakes sleepers, and fires POSIX timers. `libc/src/time.c`
gives async-signal-safe clock and timer wrappers ({need}`ICD_PROC_0005`,
{need}`SWR_PROC_0006`).
```

```{comp} POSIX.1-2024 pthreads, FS.base TLS, and futex synchronization
:id: COMP_PROC_0005
:status: active
:posix: XBD/pthread.h (BASE), XSH/2.9
:refines: ARC_PROC_0001, ICD_PROC_0006
:satisfies: SWR_PROC_0005, SWR_LIBC_0004

`libc/src/pthread.c` builds `pthread_*` threads, `FS.base` TLS, TSD keys,
deferred cancellation, cleanup stacks, mutexes, condvars, barriers, spinlocks,
and rwlocks on `SYS_FUTEX` and `SYS_THREAD_SET_FSBASE` ({need}`ICD_PROC_0006`,
{need}`SWR_PROC_0005`, {need}`SWR_LIBC_0004`).
```

```{comp} POSIX.1-2024 unnamed semaphore library
:id: COMP_PROC_0006
:status: active
:posix: XBD/semaphore.h (BASE), XSH/2.9
:refines: ARC_PROC_0001, ICD_PROC_0007
:satisfies: SWR_PROC_0005

`libc/src/semaphore.c` builds `sem_init`, `sem_destroy`, `sem_wait`,
`sem_trywait`, `sem_timedwait`, `sem_clockwait`, async-signal-safe `sem_post`,
and `sem_getvalue` on `SYS_FUTEX` ({need}`ICD_PROC_0007`,
{need}`SWR_PROC_0005`).
```
