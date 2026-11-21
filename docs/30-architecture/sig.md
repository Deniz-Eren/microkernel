# Architecture — SIG

Domain `SIG`: Structural architecture elements for Ring 3 signal delivery.

```{arc} Ring 3 signal delivery and restoration subsystem
:id: ARC_SIG_0001
:status: active
:satisfies: SYS_SIG_0001
:traces: ADR_0002

The signal subsystem tracks per-process handlers and pending thread signal masks
in the kernel and delivers frames at return-to-user boundaries to satisfy
asynchronous signal handling ({need}`SYS_SIG_0001`).
```

## Asynchronous Signal Delivery and `SYS_SIGRETURN` Restoration

The kernel delivers unmasked pending signals at syscall or interrupt return
boundaries. It writes a `sigframe_t` onto the user stack and points the return
address to `__sigreturn_trampoline`. User handlers then return cleanly to
restore the exact interrupted context and signal mask.

```mermaid
sequenceDiagram
    participant App as Ring 3 Thread
    participant K as Kernel Return Boundary (Ring 0)
    participant Tramp as libc __sigreturn_trampoline

    Note over K: kill / pthread_kill / timer queues pending signal
    App->>K: Syscall return or timer interrupt
    K->>K: Check (pending_signals & ~blocked_signals)
    K->>K: Save trapframe_t + old mask into sigframe_t on user stack
    K->>K: Apply sa_mask, set RIP = sa_handler, push trampoline return IP
    K-->>App: iretq / sysret into user sa_handler(sig)
    App->>Tramp: Handler executes ret
    Tramp->>K: syscall SYS_SIGRETURN (with sigframe_t pointer)
    K->>K: Validate user sigframe_t, restore trapframe_t + old sigmask
    K-->>App: Resume exact interrupted Ring 3 instruction
```
