# Verification — PROC

Domain `PROC`: Verification items covering preemptive scheduling, `syscall`
dispatch, and Ring 3 threads.

```{ver} Verify Ring 3 execution, priority scheduling, and thread syscalls
:id: VER_PROC_0001
:status: active
:tags: integration
:verifies: SYS_PROC_0001, SWR_PROC_0001, SWR_PROC_0002, SWR_PROC_0003

Verified by the headless QEMU harness `tests/test_runner.py` executing
`sysproc`, `ipcserv`, `ipccli`, and `sigtest`, confirming Ring 3 `syscall`
dispatch, preemptive scheduling, and `NewThread`/`ThreadReset`/`ThreadGetCtx`
({need}`SYS_PROC_0001`, {need}`SWR_PROC_0001`, {need}`SWR_PROC_0002`,
{need}`SWR_PROC_0003`).
```
