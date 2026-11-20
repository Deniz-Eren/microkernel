# System and Software Requirements — SIG

Domain `SIG`: Signal registration, injection, Ring 3 handler delivery, context
restoration, default termination, and POSIX.1-2024 signal semantics.

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

```{swr} Conform signal interfaces, masks, and SIGKILL/SIGSTOP rules to Issue 8
:id: SWR_SIG_0003
:status: draft
:derives: SYS_SIG_0001
:traces: BDR_0003, ADR_0003

`<signal.h>` ([signal.h][hdr-sig]) shall expose POSIX.1-2024 `sigaction`
([sigaction][fn-sigact]), process-directed `kill` ([kill][fn-kill]),
thread-directed `pthread_kill`, and `pthread_sigmask`/`sigprocmask`.
`sys_sigaction` shall fail with `-EINVAL` when catching or ignoring `SIGKILL` or
`SIGSTOP`, and `sig_frame_t` shall save and restore per-thread signal masks
across `SYS_SIGRETURN` ({need}`SYS_SIG_0001`).
```

```{swr} Enforce async-signal-safety across libc and signal handlers
:id: SWR_SIG_0004
:status: draft
:derives: SYS_SIG_0001
:traces: BDR_0003, ADR_0003

Every POSIX.1-2024 async-signal-safe function ([XSH 2.4][xsh-02]) shall execute
without calling `malloc`, `free`, `FILE` stream locks, or unsynchronized mutable
static state. Multithreaded signal handlers and `fork` children before `execve`
or `_Exit` shall invoke only async-signal-safe functions ({need}`SYS_SIG_0001`).
```

[hdr-sig]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/signal.h.html
[fn-sigact]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html
[fn-kill]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/kill.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
