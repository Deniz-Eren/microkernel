# Components — PROC

Domain `PROC`: white-box specification for scheduling, process/thread lifecycle,
system call dispatch, and `waitpid` wrappers.

```{comp} Priority scheduler and context switcher
:id: COMP_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0001

`kernel/core/sched.c` and `kernel/arch/x86_64/switch.S` keep 256 priority ready
queues, count slice ticks on LAPIC timer interrupts, and switch kernel stacks
and FPU state ({need}`ARC_PROC_0001`).
```

```{comp} Process manager and syscall dispatcher
:id: COMP_PROC_0002
:status: active
:refines: ARC_PROC_0001, ICD_PROC_0001, ICD_PROC_0002, ICD_PROC_0003
:satisfies: SWR_PROC_0002, SWR_PROC_0003, SWR_PROC_0007

`kernel/core/proc.c` and `kernel/core/syscall.c` manage PCB/TCB lifecycle,
`sys_proc_create`, `sys_proc_wait`, two-phase thread creation, and
`syscall_handler` dispatch ({need}`ICD_PROC_0001`, {need}`ICD_PROC_0002`,
{need}`ICD_PROC_0003`, {need}`ARC_PROC_0001`).
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
