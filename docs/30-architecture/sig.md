# Architecture — SIG

Domain `SIG`: Architecture items for Ring 3 signals.

```{arc} Ring 3 signal subsystem
:id: ARC_SIG_0001
:status: active
:satisfies: SYS_SIG_0001
:traces: ADR_0002

The signal subsystem keeps process signal handlers and queued thread signal
masks in the kernel. It sends signal frames at kernel-to-user boundaries for
asynchronous signals ({need}`SYS_SIG_0001`).
```

## Asynchronous Signals and `SYS_SIGRETURN`

The kernel sends unmasked queued signals at syscall or interrupt exit
boundaries. It writes a `sigframe_t` to the user stack and sets the exit address
to `__sigreturn_trampoline`. When a user signal handler stops, the trampoline
calls `SYS_SIGRETURN` to load the saved context and signal mask.

```mermaid
sequenceDiagram
    participant App as Ring 3 Thread
    participant K as Kernel Exit Boundary (Ring 0)
    participant Tramp as libc __sigreturn_trampoline

    Note over K: kill, pthread_kill, or timer queues signal
    App->>K: Syscall exit or timer interrupt
    K->>K: Examine (pending_signals & ~blocked_signals)
    K->>K: Write trapframe_t and previous mask to sigframe_t on user stack
    K->>K: Apply sa_mask, set RIP = sa_handler, push trampoline address
    K-->>App: iretq or sysret to user sa_handler(sig)
    App->>Tramp: Handler does ret instruction
    Tramp->>K: syscall SYS_SIGRETURN (with sigframe_t pointer)
    K->>K: Examine user sigframe_t and load trapframe_t and previous mask
    K-->>App: Continue interrupted Ring 3 instruction
```
