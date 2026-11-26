# Verification — SIG

Domain `SIG`: Verification items for signal handlers, Ring 3 signal dispatch,
masks, `SIGKILL` and `SIGSTOP` rules, `SYS_SIGRETURN`, and async-signal-safety.

```{ver} Test of Ring 3 signal handlers, sigreturn, and default stop
:id: VER_SIG_0001
:status: active
:tags: integration
:verifies: SYS_SIG_0001, SWR_SIG_0001, SWR_SIG_0002

The QEMU test harness `tests/test_runner.py` starts `sigtest`. This test makes
sure that `sigaction` sets a `SIGUSR1` handler and `SYS_SIGRETURN` loads the
previous thread context. It also makes sure that a `SIGUSR2` with no handler
stops the process ({need}`SYS_SIG_0001`, {need}`SWR_SIG_0001`,
{need}`SWR_SIG_0002`).
```

```{ver} Test of SIGKILL and SIGSTOP EINVAL and sig_frame_t mask reload
:id: VER_SIG_0002
:status: active
:tags: integration
:verifies: SWR_SIG_0003

The QEMU test harness `tests/test_runner.py` starts `sigtest`. This test makes
sure that `sigaction` rejects `SIGKILL` and `SIGSTOP` with `EINVAL`. It also
makes sure that `sigprocmask` blocks signals, `SYS_SIGRETURN` reloads the
`sig_frame_t` mask, and `SIG_IGN` operates correctly ({need}`SWR_SIG_0003`).
```

```{ver} Test of <signal.h> and libc.a async-signal-safety
:id: VER_SIG_0003
:status: active
:tags: unit
:verifies: SWR_SIG_0003, SWR_SIG_0004

CTest `posix.headers` (`tests/posix/test_headers.py`) compiles `<signal.h>`
independently and does a check of namespace rules. CTest
`posix.async_signal_safe` (`tests/posix/test_async_signal_safe.py`) examines the
`libc.a` call graph ({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```
