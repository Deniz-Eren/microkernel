# Verification — SIG

Domain `SIG`: verification items for signal registration, Ring 3 handler
dispatch, masks, `SIGKILL`/`SIGSTOP` rules, `SYS_SIGRETURN`, and
async-signal-safety.

```{ver} Verify Ring 3 signal handler delivery, sigreturn, and default action
:id: VER_SIG_0001
:status: active
:tags: integration
:verifies: SYS_SIG_0001, SWR_SIG_0001, SWR_SIG_0002

Verified by the headless QEMU harness `tests/test_runner.py` running `sigtest`,
confirming `sigaction`, caught `SIGUSR1` handler execution and `SYS_SIGRETURN`
restoration, and process termination on unhandled `SIGUSR2`
({need}`SYS_SIG_0001`, {need}`SWR_SIG_0001`, {need}`SWR_SIG_0002`).
```

```{ver} Verify SIGKILL/SIGSTOP EINVAL and sig_frame_t mask restoration
:id: VER_SIG_0002
:status: active
:tags: integration
:verifies: SWR_SIG_0003

The headless QEMU harness `tests/test_runner.py` runs `sigtest`. It verifies
that `sigaction` rejects `SIGKILL` and `SIGSTOP` with `EINVAL`. It also verifies
`sigprocmask` deferral, `sig_frame_t` mask reload across `SYS_SIGRETURN`, and
`SIG_IGN` behavior ({need}`SWR_SIG_0003`).
```

```{ver} Verify <signal.h> standalone compile and libc.a async-signal-safety
:id: VER_SIG_0003
:status: active
:tags: unit
:verifies: SWR_SIG_0003, SWR_SIG_0004

CTest `posix.headers` (`tests/posix/test_headers.py`) checks standalone
`<signal.h>` compilation and namespace hygiene. CTest `posix.async_signal_safe`
(`tests/posix/test_async_signal_safe.py`) checks the `libc.a` binary call graph
({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```
