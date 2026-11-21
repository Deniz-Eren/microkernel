# Interfaces — PROC

Domain `PROC`: system call ABI, native thread/process wrappers, and POSIX.1-2024
`<sys/wait.h>`.

```{icd} System V AMD64 syscall ABI
:id: ICD_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0002, SWR_PROC_0004

`libc/include/sys/microkernel.h` specifies system call numbers (`0`..`47`) and
the `RAX`/`RDI`/`RSI`/`RDX`/`R10`/`R8`/`R9` register convention
({need}`ARC_PROC_0001`, {need}`SWR_PROC_0002`, {need}`SWR_PROC_0004`).
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

```{icd} POSIX.1-2024 <time.h> monotonic clocks and timers interface
:id: ICD_PROC_0005
:status: active
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0006

`libc/include/time.h` declares `CLOCK_REALTIME`, `CLOCK_MONOTONIC`,
`TIMER_ABSTIME`, `clock_getres`, `clock_gettime`, `clock_settime`,
`clock_nanosleep`, `nanosleep`, and `timer_*` with no `<sys/microkernel.h>`
leakage ({need}`ARC_PROC_0001`, {need}`SWR_PROC_0006`).
```

```{icd} POSIX.1-2024 <pthread.h> public interface
:id: ICD_PROC_0006
:status: active
:posix: XBD/pthread.h (BASE), XSH/2.9
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0005

`libc/include/pthread.h` declares `pthread_create`, `pthread_join`, mutexes,
`pthread_cond_*`, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks, and
TSD keys with no `<sys/microkernel.h>` leakage ({need}`ARC_PROC_0001`,
{need}`SWR_PROC_0005`).
```

```{icd} POSIX.1-2024 <semaphore.h> public interface
:id: ICD_PROC_0007
:status: active
:posix: XBD/semaphore.h (BASE), XSH/2.9
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0005

`libc/include/semaphore.h` declares `sem_t`, `sem_init`, `sem_destroy`,
`sem_wait`, `sem_trywait`, `sem_timedwait`, `sem_clockwait`, `sem_post`, and
`sem_getvalue` with no `<sys/microkernel.h>` leakage ({need}`ARC_PROC_0001`,
{need}`SWR_PROC_0005`).
```
