# Interfaces — SIG

The `SIG` domain includes signal numbers, user interfaces, the kernel ABI, and
the signal frame layout.

```{icd} Kernel signal ABI and sig_frame_t layout
:id: ICD_SIG_0001
:status: active
:refines: ARC_SIG_0001
:satisfies: SWR_SIG_0001, SWR_SIG_0002, SWR_SIG_0003

`libc/include/sys/signal.h` and `kernel/include/kernel/signal.h` give POSIX
signal numbers, `SIG_BLOCK`/`SIG_UNBLOCK`/`SIG_SETMASK`, `SA_*` flags, `struct
__k_sigaction`, `SYS_SIGPROCMASK`, and the 16-byte aligned `sig_frame_t` stack
frame with `saved_mask` ({need}`ARC_SIG_0001`, {need}`SWR_SIG_0001`,
{need}`SWR_SIG_0002`, {need}`SWR_SIG_0003`).
```

```{icd} POSIX.1-2024 <signal.h> public interface
:id: ICD_SIG_0002
:status: active
:posix: XBD/signal.h (CX), XSH/sigaction (CX), XSH/kill (CX)
:refines: ARC_SIG_0001
:satisfies: SWR_SIG_0003

`libc/include/signal.h` gives `sig_atomic_t`, `sigset_t`, `SIG_DFL`, `SIG_IGN`,
`SIG_ERR`, `struct sigaction`, `sigset_t` helpers, `sigaction`, `sigprocmask`,
`pthread_sigmask`, `kill`, `pthread_kill`, `raise`, and `signal`. It does not
include `<sys/microkernel.h>` ({need}`ARC_SIG_0001`, {need}`SWR_SIG_0003`).
```
