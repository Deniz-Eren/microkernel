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

## Thread Lifecycle and Scheduler State Machine

The scheduler maintains 256 strict priority queues (`0` to `255`) with
round-robin preemption on LAPIC timer ticks and temporary priority inheritance
during synchronous IPC.

```mermaid
stateDiagram-v2
    [*] --> READY : ThreadSpawn / fork / proc_spawn
    READY --> RUNNING : Highest-priority pick
    RUNNING --> READY : LAPIC tick / yield / preempted by higher priority

    RUNNING --> SEND_BLOCKED : SendMsg (server not waiting)
    SEND_BLOCKED --> REPLY_BLOCKED : Server ReceiveMsg picks message
    RUNNING --> REPLY_BLOCKED : SendMsg (server waiting, PI boost)
    REPLY_BLOCKED --> READY : Server ReplyMsg or signal EINTR
    SEND_BLOCKED --> READY : Signal EINTR

    RUNNING --> RECEIVE_BLOCKED : ReceiveMsg (channel empty)
    RECEIVE_BLOCKED --> READY : Client SendMsg, SendPulse, or signal EINTR

    RUNNING --> SLEEP_BLOCKED : clock_nanosleep
    SLEEP_BLOCKED --> READY : Deadline elapsed or signal EINTR

    RUNNING --> FUTEX_BLOCKED : SYS_FUTEX(WAIT)
    FUTEX_BLOCKED --> READY : SYS_FUTEX(WAKE), timeout, or signal EINTR

    RUNNING --> WAIT_BLOCKED : waitpid (child running)
    WAIT_BLOCKED --> READY : Child exit/signal or signal EINTR

    RUNNING --> DEAD : _exit / pthread_exit / fatal signal
    DEAD --> [*]
```

## SMP Multi-Core Scheduling and Lock Hierarchy

Every online CPU owns a cache-line-aligned `cpu_local_t` block (`GS.base` on
`x86-64`) and a private `idle_tcb`. All cores dispatch from the global 256-level
priority runqueue under `g_sched_lock` and coordinate wakeups with Reschedule
IPIs (`0xF0`). Synchronous TLB Shootdown IPIs (`0xF1`) synchronize address-space
unmaps and COW splits across cores.

```mermaid
flowchart TD
    L1["1. g_proc_lock (PCB list, FDs, cwd, umask, waitpid, signals)"]
    L2["2. g_ipc_lock / g_time_lock / g_shm_lock / g_intr_lock"]
    L3["3. g_kheap_lock (kernel heap free list)"]
    L4["4. g_vmm_lock (PML4/PTE updates and COW)"]
    L5["5. g_pmm_lock (frame bitmap and refcounts)"]
    L6["Leaf: g_sched_lock (runqueue, switch_to, futex)"]
    L7["Leaf: g_console_lock (printk, debug log, coverage)"]

    L1 --> L2
    L1 --> L3
    L2 --> L3
    L3 --> L4
    L4 --> L5
    L1 --> L6
    L2 --> L6
```
