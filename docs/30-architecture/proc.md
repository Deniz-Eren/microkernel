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
