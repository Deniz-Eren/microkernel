# System and Software Requirements — SIG

The `SIG` domain includes signal handlers, signal delivery, Ring 3 signal
frames, context continuation, process stop, and POSIX.1-2024 signal rules.

```{sys} Send asynchronous signals to Ring 3 threads
:id: SYS_SIG_0001
:status: active

The system must start recorded Ring 3 signal handlers and continue the preempted
context correctly. It must stop a process when a signal has no handler.
```

```{swr} Record signal handlers and wake IPC threads with EINTR
:id: SWR_SIG_0001
:status: active
:derives: SYS_SIG_0001

The kernel must record Ring 3 signal handlers with `sigaction` and send signals
with `kill`. It must wake IPC-blocked threads with `-EINTR`
({need}`SYS_SIG_0001`).
```

```{swr} Push Ring 3 signal frames and continue with SYS_SIGRETURN
:id: SWR_SIG_0002
:status: active
:derives: SYS_SIG_0001

The kernel must push a `sig_frame_t` onto the user stack to enter a handler and
load the kept trapframe on `SYS_SIGRETURN`. It must stop the process when a
signal has no handler ({need}`SYS_SIG_0001`).
```

```{swr} Align signal API, masks, and SIGKILL/SIGSTOP rules with Issue 8
:id: SWR_SIG_0003
:status: active
:posix: XBD/signal.h (CX), XSH/sigaction (CX), XSH/kill (CX)
:derives: SYS_SIG_0001
:traces: BDR_0003, ADR_0003

`<signal.h>` ([signal.h][hdr-sig]) must give POSIX.1-2024 `sigaction`
([sigaction][fn-sigact]), `kill` ([kill][fn-kill]), `pthread_kill`, and
`pthread_sigmask`/`sigprocmask`. `sys_sigaction` must give `-EINVAL` for
`SIGKILL` or `SIGSTOP`, and `sig_frame_t` must keep signal masks across
`SYS_SIGRETURN` ({need}`SYS_SIG_0001`).
```

```{swr} Keep async-signal-safe functions free of non-reentrant calls
:id: SWR_SIG_0004
:status: active
:posix: XSH/2.4.3 (BASE)
:derives: SYS_SIG_0001
:traces: BDR_0003, ADR_0003

All POSIX.1-2024 async-signal-safe functions ([XSH 2.4][xsh-02]) must operate
without `malloc`, `free`, `FILE` mutexes, or shared static state. Signal
handlers and multithreaded `fork` child processes must use only
async-signal-safe functions before `execve` or `_Exit` ({need}`SYS_SIG_0001`).
```

[hdr-sig]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/signal.h.html
[fn-sigact]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html
[fn-kill]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/kill.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
