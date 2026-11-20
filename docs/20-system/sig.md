# System and Software Requirements — SIG

Domain `SIG`: Signal registration, injection, Ring 3 handler delivery, context
restoration, and default termination.

```{sys} Deliver asynchronous signals to Ring 3 threads
:id: SYS_SIG_0001
:status: active

The system shall deliver registered Ring 3 signal handlers with transparent
context restoration and terminate processes on unhandled signals.
```

```{swr} Register handlers and inject signals with EINTR wakeup
:id: SWR_SIG_0001
:status: active
:derives: SYS_SIG_0001

The kernel shall register per-signal Ring 3 handlers via `sigaction`, inject
signals via `kill`, and wake threads blocked in IPC with `-EINTR`
({need}`SYS_SIG_0001`).
```

```{swr} Deliver Ring 3 signal frames and restore via SYS_SIGRETURN
:id: SWR_SIG_0002
:status: active
:derives: SYS_SIG_0001

The kernel shall push a `sig_frame_t` onto the user stack to enter a registered
handler, restore the saved trapframe on `SYS_SIGRETURN`, and terminate the
process when no handler is registered ({need}`SYS_SIG_0001`).
```
