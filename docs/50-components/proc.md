# Components — PROC

Domain `PROC`: White-box specifications for scheduling, thread lifecycle, and
system call dispatch.

```{comp} Priority scheduler and context switcher
:id: COMP_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0001

`kernel/core/sched.c` and `kernel/arch/x86_64/switch.S` maintain 256 priority
ready queues, update slice ticks on LAPIC timer interrupts, and switch kernel
stacks and FPU state for the process subsystem ({need}`ARC_PROC_0001`).
```

```{comp} Process manager and syscall dispatcher
:id: COMP_PROC_0002
:status: active
:refines: ARC_PROC_0001, ICD_PROC_0001, ICD_PROC_0002
:satisfies: SWR_PROC_0002, SWR_PROC_0003

`kernel/core/proc.c` and `kernel/core/syscall.c` implement PCB/TCB lifecycle,
two-phase embryo thread creation (`sys_thread_create`, `sys_thread_reset`,
`sys_thread_get_ctx`), and `syscall_handler` dispatch ({need}`ICD_PROC_0001`,
{need}`ICD_PROC_0002`) for the process subsystem ({need}`ARC_PROC_0001`).
```
