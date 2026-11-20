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
