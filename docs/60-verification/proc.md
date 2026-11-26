# Verification — PROC

Domain `PROC`: Verification items for preemptive scheduling, `syscall` dispatch,
and Ring 3 threads.

```{ver} Test of Ring 3 operation, priority scheduling, and thread syscalls
:id: VER_PROC_0001
:status: active
:tags: integration
:verifies: SYS_PROC_0001, SWR_PROC_0001, SWR_PROC_0002, SWR_PROC_0003

The QEMU test harness `tests/test_runner.py` starts `sysproc`, `ipcserv`,
`ipccli`, and `sigtest`. These tests do a check of Ring 3 `syscall` dispatch,
preemptive scheduling, `NewThread`, `ThreadReset`, and `ThreadGetCtx`
({need}`SYS_PROC_0001`, {need}`SWR_PROC_0001`, {need}`SWR_PROC_0002`,
{need}`SWR_PROC_0003`).
```

```{ver} Test of ProcCreate, child exit-code waitpid, and <sys/wait.h>
:id: VER_PROC_0002
:status: active
:tags: integration
:verifies: SWR_PROC_0007

The QEMU test harness `tests/test_runner.py` starts `proctest` and `procchild`.
These tests do a check of `ProcCreate`, `WNOHANG`, and synchronous `waitpid`
exit code 42. CTest `posix.headers` and `posix.async_signal_safe` do a check of
`<sys/wait.h>` and `waitpid` safety ({need}`SWR_PROC_0007`).
```

```{ver} Test of POSIX.1-2024 monotonic clocks, nanosleep, and timers
:id: VER_PROC_0003
:status: active
:tags: integration
:verifies: SWR_PROC_0006

The QEMU test harness `tests/test_runner.py` starts `timetest`. This test does a
check of `CLOCK_MONOTONIC`, `CLOCK_REALTIME`, `clock_nanosleep`, and periodic
`timer_create` and `timer_settime` signals. CTest `posix.headers` and
`posix.async_signal_safe` do a check of `<time.h>` ({need}`SWR_PROC_0006`).
```

```{ver} Test of POSIX.1-2024 pthreads, semaphores, and FS.base TLS
:id: VER_PROC_0004
:status: active
:tags: integration
:verifies: SWR_PROC_0005

The QEMU test harness `tests/test_runner.py` starts `pthreadtest`. This test
does a check of `FS.base` TLS, mutexes, `pthread_cond_clockwait`, barriers,
spinlocks, rwlocks, and `sem_*`. CTest `posix.headers` and
`posix.async_signal_safe` do a check of headers and `sem_post`
({need}`SWR_PROC_0005`).
```

```{ver} Test of POSIX.1-2024 fork COW memory and execve argv, envp, O_CLOEXEC
:id: VER_PROC_0005
:status: active
:tags: integration
:verifies: SWR_PROC_0004

The QEMU test harness `tests/test_runner.py` starts `forkpipetest` and
`exechelper`. These tests make sure that `fork` isolates COW pages, `execve`
sends `argv` and `envp`, and `O_CLOEXEC` closes descriptors. CTest
`posix.headers` and `posix.async_signal_safe` do a check of `<unistd.h>`,
`fork`, and `execve` ({need}`SWR_PROC_0004`).
```

```{ver} Test of TTY ISIG process groups and core utilities
:id: VER_PROC_0006
:status: active
:tags: integration
:verifies: SWR_PROC_0008

The QEMU test harness `tests/test_runner.py` starts `jobtest`
(`tests/jobtest/test_job.c`). This test does a check of `setpgid`, `getpgid`,
`setsid`, `waitpid(0)`, `-pgid`, `kill(-pgid)`, `tcgetpgrp`, `tcsetpgrp`, and
`ISIG` `Ctrl-C` `SIGINT`. It also does a check of `mkdir`, `rm`, `cp`, `mv`,
`uname`, `env`, `wc`, and `head`. CTest compatibility gates 1 to 3 do a check of
headers and async-signal-safety ({need}`SWR_PROC_0008`).
```

```{ver} Test of multi-core SMP start and concurrent kernel locks in QEMU
:id: VER_PROC_0007
:status: active
:tags: integration
:verifies: SWR_PROC_0009

The QEMU test harness `tests/test_runner.py` starts `smptest`
(`tests/smptest/test_smp.c`) with `-smp 2`. This test does a check of multi-core
atomic counter changes and contention on `futex`, mutexes, condvars, and
semaphores. It also does a check of parallel `PMM`, `VMM`, COW, and TLB
shootdowns, and concurrent IPC with isolated `XSAVE` and `FMA` state. CTest
`proc.smp_cores` (`tests/boot/test_smp_cores.py`) does a test of 4-core `-smp 4`
boot and operation ({need}`SWR_PROC_0009`).
```

```{ver} Test of ps, getconf, tty, stty, sysconf, and TTY dimensions in QEMU
:id: VER_PROC_0008
:status: active
:tags: integration
:verifies: SWR_PROC_0010

The QEMU test harness starts `utilstest` (`tests/utilstest/test_utils.c`) and
`shtest` (`tests/shtest/test_sh.c`). These tests do a check of `sysconf`,
`confstr`, `pathconf`, `fpathconf`, `gethostname`, `ctermid`, `ttyname*`, and
`tcgetsid`. They also do a check of `tcgetwinsize`, `tcsetwinsize`, `ProcList`,
`ps`, `getconf`, `tty`, and `stty`. CTest compatibility gates 1 to 3 do a check
of header rules, non-POSIX symbol exclusion, and async-signal-safety
({need}`SWR_PROC_0010`).
```

```{ver} Test of credentials, pwd and grp, rlimits, and id, nice, and ulimit
:id: VER_PROC_0009
:status: active
:tags: integration
:verifies: SWR_PROC_0011

The QEMU test harness starts `utilstest` (`tests/utilstest/test_utils.c`) and
`shtest` (`tests/shtest/test_sh.c`). These tests do a check of UID, GID, and
group changes, `EPERM` errors, `getlogin*`, `<pwd.h>`, and `<grp.h>`. They also
do a check of `RLIMIT_NOFILE`, `id`, `logname`, `nice`, `renice`, `umask`, and
`ulimit`. CTest compatibility gates 1 to 3 do a check of the new headers, XCU
utilities, and async-signal-safety ({need}`SWR_PROC_0011`).
```

```{ver} Test of Base signals, waitid, _Fork, popen, system, and <threads.h>
:id: VER_PROC_0010
:status: active
:tags: integration
:verifies: SWR_PROC_0012

The QEMU test harness starts `sigtest`, `proctest`, `pthreadtest`, `utilstest`,
and `shtest`. These tests do a check of `sigqueue`, `sigwait*`, `sigsuspend`,
`pause`, `sig2str`, `str2sig`, `waitid`, `_Fork`, `execl*`, `fexecve`, `popen`,
and `system`. They also do a check of `<threads.h>`, LIFO `pthread_atfork`,
robust mutex `EOWNERDEAD`, `MAP_SHARED` named semaphores, `sleep`, `time`,
`timeout`, `nohup`, and `sh` `wait`. CTest compatibility gates 1 to 3 do a check
of header rules, async-signal-safety, and cancellation points
({need}`SWR_PROC_0012`).
```

```{ver} Test of kernel PT_INTERP load and SysV auxv in QEMU
:id: VER_PROC_0011
:status: active
:tags: integration
:verifies: SWR_PROC_0013

The QEMU test harness starts `dlsoctest` (`tests/dlsoctest/test_dlsoctest.c`).
This test starts the freestanding dynamic executable `dlsoctest_dyn`
(`tests/dlsoctest/dlsoctest_dyn.c`) with `PT_INTERP=/boot/ld.so`. It does a
check of the `AT_BASE`, `AT_ENTRY`, `AT_PHDR`, `AT_PHENT`, and `AT_PHNUM` auxv
entries. It also does a check of mapped program headers for the two images and
the kernel W^X `mprotect` rule ({need}`SWR_PROC_0013`).
```
