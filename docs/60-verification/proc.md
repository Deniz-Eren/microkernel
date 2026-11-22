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

```{ver} Verify ProcCreate, child exit-code waitpid, and <sys/wait.h>
:id: VER_PROC_0002
:status: active
:tags: integration
:verifies: SWR_PROC_0007

The headless QEMU harness `tests/test_runner.py` runs `proctest` and
`procchild`. It checks `ProcCreate`, `WNOHANG`, and blocking `waitpid` exit code
42. CTest `posix.headers` and `posix.async_signal_safe` check `<sys/wait.h>` and
`waitpid` safety ({need}`SWR_PROC_0007`).
```

```{ver} Verify POSIX.1-2024 monotonic clocks, nanosleep, and timers
:id: VER_PROC_0003
:status: active
:tags: integration
:verifies: SWR_PROC_0006

The headless QEMU harness `tests/test_runner.py` runs `timetest`. It checks
`CLOCK_MONOTONIC`, `CLOCK_REALTIME`, `clock_nanosleep`, and periodic
`timer_create`/`timer_settime` signals. CTest `posix.headers` and
`posix.async_signal_safe` check `<time.h>` ({need}`SWR_PROC_0006`).
```

```{ver} Verify POSIX.1-2024 pthreads, semaphores, and FS.base TLS
:id: VER_PROC_0004
:status: active
:tags: integration
:verifies: SWR_PROC_0005

The headless QEMU harness `tests/test_runner.py` runs `pthreadtest`. It checks
`FS.base` TLS, mutexes, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks,
and `sem_*`. CTest `posix.headers` and `posix.async_signal_safe` check headers
and `sem_post` ({need}`SWR_PROC_0005`).
```

```{ver} Verify POSIX.1-2024 fork COW isolation and execve argv/envp/O_CLOEXEC
:id: VER_PROC_0005
:status: active
:tags: integration
:verifies: SWR_PROC_0004

The headless QEMU harness `tests/test_runner.py` runs `forkpipetest` and
`exechelper`. It checks `fork` COW isolation, `execve` `argv`/`envp`, and
`O_CLOEXEC` cleanup. CTest `posix.headers` and `posix.async_signal_safe` check
`<unistd.h>` and `fork`/`execve` safety ({need}`SWR_PROC_0004`).
```

```{ver} Verify TTY ISIG job control, process groups, and core utilities in QEMU
:id: VER_PROC_0006
:status: active
:tags: integration
:verifies: SWR_PROC_0008

The headless QEMU harness `tests/test_runner.py` runs `jobtest`
(`tests/jobtest/test_job.c`). It checks `setpgid`, `getpgid`, `setsid`,
`waitpid(0)`/`-pgid`, `kill(-pgid)`, `tcgetpgrp`/`tcsetpgrp`, `ISIG` `Ctrl-C`
`SIGINT`, and `mkdir`, `rm`, `cp`, `mv`, `uname`, `env`, `wc`, and `head`. CTest
Gates 1 to 3 verify headers and async-signal-safety ({need}`SWR_PROC_0008`).
```
