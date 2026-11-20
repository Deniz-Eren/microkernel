# Interfaces — PROC

Domain `PROC`: system call ABI, native thread/process wrappers, and POSIX.1-2024
`<sys/wait.h>`.

```{icd} System V AMD64 syscall ABI
:id: ICD_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0002

`libc/include/sys/microkernel.h` specifies system call numbers (`0`..`29`) and
the `RAX`/`RDI`/`RSI`/`RDX`/`R10`/`R8`/`R9` register convention
({need}`ARC_PROC_0001`, {need}`SWR_PROC_0002`).
```

```{icd} Ring 3 thread management interface
:id: ICD_PROC_0002
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0003

`libc/include/sys/thread.h` gives `thread_info_t` and the `NewThread`,
`ThreadReset`, `ThreadGetCtx`, `ThreadGetTid`, and `ThreadSpawn` wrappers
({need}`ARC_PROC_0001`, {need}`SWR_PROC_0003`).
```

```{icd} Ring 3 native process creation and wait interface
:id: ICD_PROC_0003
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0007

`libc/include/sys/proc.h` gives `PROC_WAIT_NOHANG`, `ProcCreate`, and `ProcWait`
wrappers to spawn bootfs children and read exit status ({need}`ARC_PROC_0001`,
{need}`SWR_PROC_0007`).
```

```{icd} POSIX.1-2024 <sys/wait.h> public interface
:id: ICD_PROC_0004
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/waitpid (BASE)
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0007

`libc/include/sys/wait.h` declares `WNOHANG`, `WUNTRACED`, `WIFEXITED`,
`WEXITSTATUS`, `WIFSIGNALED`, `WTERMSIG`, `WIFSTOPPED`, `WSTOPSIG`, `wait`, and
`waitpid` with no `<sys/microkernel.h>` leakage ({need}`ARC_PROC_0001`,
{need}`SWR_PROC_0007`).
```
