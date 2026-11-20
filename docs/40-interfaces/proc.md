# Interfaces — PROC

Domain `PROC`: System call ABI and Ring 3 thread management interfaces.

```{icd} System V AMD64 syscall ABI
:id: ICD_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0002

`libc/include/sys/microkernel.h` defines system call numbers (`0`..`26`) and the
`RAX`/`RDI`/`RSI`/`RDX`/`R10`/`R8`/`R9` register convention used by the process
subsystem ({need}`ARC_PROC_0001`) to satisfy system call dispatch
({need}`SWR_PROC_0002`).
```

```{icd} Ring 3 thread management interface
:id: ICD_PROC_0002
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0003

`libc/include/sys/thread.h` defines `thread_info_t` and the `NewThread`,
`ThreadReset`, `ThreadGetCtx`, `ThreadGetTid`, and `ThreadSpawn` wrappers for
the process subsystem ({need}`ARC_PROC_0001`) to satisfy thread creation and
inspection ({need}`SWR_PROC_0003`).
```
