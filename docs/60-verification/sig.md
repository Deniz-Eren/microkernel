# Verification — SIG

Domain `SIG`: Verification items covering signal registration, Ring 3 handler
dispatch, `SYS_SIGRETURN`, and default termination.

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
