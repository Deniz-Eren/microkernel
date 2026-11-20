# Components — SIG

Domain `SIG`: white-box specification for kernel signal delivery and `libc`
signal wrappers.

```{comp} Kernel signal delivery, mask, and sigreturn handler
:id: COMP_SIG_0001
:status: active
:refines: ARC_SIG_0001, ICD_SIG_0001
:satisfies: SWR_SIG_0001, SWR_SIG_0002, SWR_SIG_0003

`kernel/core/signal.c` gives `sys_sigaction` (with `-EINVAL` for `SIGKILL` and
`SIGSTOP`), `sys_sigprocmask`, `sys_inject_sig`, `signal_deliver_check`, and
`sys_sigreturn`. It keeps and reloads `blocked_signals` in `sig_frame_t` with
`copy_to_user` and stops processes on unhandled signals ({need}`ICD_SIG_0001`,
{need}`ARC_SIG_0001`).
```

```{comp} Async-signal-safe libc signal wrappers
:id: COMP_SIG_0002
:status: active
:posix: XSH/2.4.3 (BASE), XSH/sigaction (CX), XSH/kill (CX)
:refines: ARC_SIG_0001, ICD_SIG_0002
:satisfies: SWR_SIG_0003, SWR_SIG_0004

`libc/src/signal.c` gives `sigaction`, `sigprocmask`, `pthread_sigmask`, `kill`,
`pthread_kill`, `raise`, `signal`, and `sigset_t` helpers. All wrappers use
syscalls and stack storage with no heap allocation or locks
({need}`ICD_SIG_0002`, {need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```
