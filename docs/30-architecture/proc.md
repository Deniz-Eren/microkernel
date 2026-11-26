# Architecture — PROC

Domain `PROC`: Architecture items for processes, threads, the scheduler, and
syscalls.

```{arc} Process, thread, scheduler, and syscall subsystem
:id: ARC_PROC_0001
:status: active
:satisfies: SYS_PROC_0001
:traces: ADR_0002

The process subsystem controls PCB and TCB states, preemptive 256-priority
scheduling, and `syscall`/`sysret` dispatch for Ring 3 operation
({need}`SYS_PROC_0001`).
```

## Thread States and Scheduler Model

The scheduler keeps 256 priority queues (`0` to `255`) with time-slice
preemption on LAPIC timer ticks and temporary priority inheritance during
synchronous IPC.

```mermaid
stateDiagram-v2
    [*] --> READY : ThreadSpawn / fork / proc_spawn
    READY --> RUNNING : Highest-priority selection
    RUNNING --> READY : LAPIC tick, yield, or higher priority

    RUNNING --> SEND_BLOCKED : SendMsg (server busy)
    SEND_BLOCKED --> REPLY_BLOCKED : Server ReceiveMsg gets message
    RUNNING --> REPLY_BLOCKED : SendMsg (server ready, PI boost)
    REPLY_BLOCKED --> READY : Server ReplyMsg or signal EINTR
    SEND_BLOCKED --> READY : Signal EINTR

    RUNNING --> RECEIVE_BLOCKED : ReceiveMsg (channel empty)
    RECEIVE_BLOCKED --> READY : Client SendMsg, SendPulse, or signal EINTR

    RUNNING --> SLEEP_BLOCKED : clock_nanosleep
    SLEEP_BLOCKED --> READY : Time limit reached or signal EINTR

    RUNNING --> FUTEX_BLOCKED : SYS_FUTEX(WAIT)
    FUTEX_BLOCKED --> READY : SYS_FUTEX(WAKE), timeout, or signal EINTR

    RUNNING --> WAIT_BLOCKED : waitpid (child active)
    WAIT_BLOCKED --> READY : Child exit, signal, or signal EINTR

    RUNNING --> DEAD : _exit, pthread_exit, or stop signal
    DEAD --> [*]
```

## SMP Multi-Core Scheduling and Spinlock Sequence

Each active CPU has a cache-line-aligned `cpu_local_t` block (`GS.base` on
`x86-64`) and a private `idle_tcb`. All cores dispatch threads from the global
256-level ready queues with `g_sched_lock` and wake other cores with Reschedule
IPIs (`0xF0`). Synchronous TLB Shootdown IPIs (`0xF1`) flush TLB entries across
cores during address-space unmap and COW operations.

```mermaid
flowchart TD
    L1["1. g_proc_lock (PCB table, FDs, cwd, umask, waitpid, signals)"]
    L2["2. g_ipc_lock / g_time_lock / g_shm_lock / g_intr_lock"]
    L3["3. g_kheap_lock (kernel heap free blocks)"]
    L4["4. g_vmm_lock (PML4/PTE changes and COW)"]
    L5["5. g_pmm_lock (frame bitmap and refcounts)"]
    L6["Leaf: g_sched_lock (ready queues, switch_to, futex)"]
    L7["Leaf: g_console_lock (printk, debug output, coverage)"]

    L1 --> L2
    L1 --> L3
    L2 --> L3
    L3 --> L4
    L4 --> L5
    L1 --> L6
    L2 --> L6
```
