# System and Software Requirements — PROC

The `PROC` domain includes process and thread control, preemptive scheduling,
fast syscall dispatch, and POSIX.1-2024 threads, clocks, and timers.

```{sys} Operate isolated Ring 3 processes and preemptive threads
:id: SYS_PROC_0001
:status: active

The system must operate isolated Ring 3 processes and schedule threads across
256 priority levels. It must dispatch syscalls through the `x86-64`
`syscall`/`sysret` ABI.
```

```{swr} Schedule threads across 256 priority levels
:id: SWR_PROC_0001
:status: active
:derives: SYS_PROC_0001

The scheduler must select the highest-priority thread in the ready queues
preemptively. It must count time slices and do each context switch correctly
({need}`SYS_PROC_0001`).
```

```{swr} Dispatch Ring 3 syscalls with syscall and sysret
:id: SWR_PROC_0002
:status: active
:derives: SYS_PROC_0001

The kernel must set the `x86-64` `syscall`/`sysret` MSRs and dispatch correct
syscall numbers in `RAX`. It must give `-ENOSYS` for unknown syscall numbers
({need}`SYS_PROC_0001`).
```

```{swr} Give two-step Ring 3 thread creation and state read
:id: SWR_PROC_0003
:status: active
:derives: SYS_PROC_0001

The kernel must make unstarted threads with `NewThread` and prepare them with
`ThreadReset` on a user stack that the kernel allocates. `ThreadGetCtx` must
give the thread state ({need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 Base process operations and optional spawn
:id: SWR_PROC_0004
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/fork (BASE), XBD/spawn.h (SPN)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003, ADR_0011

The process subsystem must give POSIX.1-2024 Base process creation, executable
start, `waitpid` ([sys/wait.h][hdr-wait]), and `_Exit` on `ProcCreate` and
`VMCloneCow`. It must put `posix_spawn*` ([spawn.h][hdr-spawn], `SPN`) behind
`_POSIX_SPAWN` ({need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 Base pthreads and semaphores
:id: SWR_PROC_0005
:status: active
:posix: XBD/pthread.h (BASE), XBD/semaphore.h (BASE), XSH/2.9
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<pthread.h>` ([pthread.h][hdr-pth]) and `<semaphore.h>`
([semaphore.h][hdr-sem]) must give the Issue 8 Base thread suite (mutexes,
`pthread_cond_*`, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks) and
`sem_*` semaphores. Thread synchronization must obey [XSH 2.9][xsh-02] memory
rules ({need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 Base monotonic clocks and timers
:id: SWR_PROC_0006
:status: active
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

`<time.h>` ([time.h][hdr-time]) must give mandatory Issue 8 Base functions
`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`, and
`timer_settime`. These functions must operate on `CLOCK_MONOTONIC` and
`CLOCK_REALTIME` with no option macro guard ({need}`SYS_PROC_0001`).
```

```{swr} Start bootfs child processes and give exit status with waitpid
:id: SWR_PROC_0007
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/waitpid (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

The kernel must start `bootfs` child processes with `ProcCreate` and give child
exit status with `ProcWait`. `<sys/wait.h>` ([sys/wait.h][hdr-wait]) must give
async-signal-safe `wait` and `waitpid` ([waitpid][fn-waitpid])
({need}`SYS_PROC_0001`).
```

```{swr} Give process groups, TTY signal control, and primary utilities
:id: SWR_PROC_0008
:status: active
:posix: XSH/setpgid (BASE), XSH/tcsetpgrp (BASE), XBD/termios.h (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, ADR_0003

The kernel and `ttyserv` must give process groups (`setpgid`, `getpgid`,
`getpgrp`, `setsid`, `getsid`, `tcgetpgrp`, `tcsetpgrp`), `ISIG` (`SIGINT` on
`Ctrl-C`), `waitpid(0)`/`waitpid(-pgid)`, and `kill(-pgid)`. Interactive `sh`
must control foreground process groups, and Ring 3 must give `mkdir`, `rm`,
`cp`, `mv`, `uname`, `env`, `wc`, and `head` ({need}`SYS_PROC_0001`).
```

```{swr} Boot secondary CPUs through ACPI MADT and operate SMP scheduling
:id: SWR_PROC_0009
:status: active
:derives: SYS_PROC_0001
:traces: ADR_0003, ADR_0022, ADR_0024, ADR_0026, ADR_0027

The kernel must start secondary CPUs behind standard HAL functions
(`INIT`/`SIPI` and `GS.base` on `x86-64`, `PSCI` and `TPIDR_EL1` for new
`arm64`). Kernel spinlocks for `pmm`, `vmm`, `sched`, `ipc`, and `futex` must
operate safely across all CPU cores ({need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 ps, sysconf, TTY dimensions, and primary utilities
:id: SWR_PROC_0010
:status: active
:posix: XCU/ps (BASE), XCU/getconf (BASE), XSH/sysconf (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0028

The kernel must keep process command metadata and give a read-only process table
for `ps` ([ps][xcu-ps]). `libc` and `ttyserv` must give Base `sysconf`
([sysconf][fn-sysconf]), `confstr`, `pathconf`, `fpathconf`, `gethostname`,
`ctermid`, `ttyname`, `ttyname_r`, `tcgetsid`, `tcgetwinsize`, and
`tcsetwinsize` with no non-POSIX names. Ring 3 must give `ps`, `getconf`, `tty`,
and `stty` ({need}`SYS_PROC_0001`).
```

```{swr} Give POSIX.1-2024 credentials, pwd/grp, rlimits, and nice/ulimit
:id: SWR_PROC_0011
:status: active
:posix: XBD/pwd.h (BASE), XBD/grp.h (BASE), XBD/sys/resource.h (BASE)
:derives: SYS_PROC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0029

The kernel must record `real`, `effective`, and `saved` UIDs and GIDs,
supplementary groups, `nice`, and `rlimit` limits on `pcb_t`. `libc` must give
Base `<pwd.h>` ([pwd.h][hdr-pwd]), `<grp.h>` ([grp.h][hdr-grp]),
`<sys/resource.h>` ([sys/resource.h][hdr-rlim]), UID/GID/group calls, and
`getlogin`/`getlogin_r`. Ring 3 must give `id`, `logname`, `nice`, `renice`,
`umask`, and `ulimit` ({need}`SYS_PROC_0001`).
```

```{swr} Give Base signals, waitid, _Fork, popen/system, and <threads.h>
:id: SWR_PROC_0012
:status: active
:posix: XBD/threads.h (CX), XSH/waitid (BASE), XSH/sem_open (BASE)
:derives: SYS_PROC_0001, SYS_SIG_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0031

The kernel and `libc` must give Base `_Fork` ([_Fork][fn-ufork]), `waitid`
([waitid][fn-waitid]), `execl*`, `fexecve`, `popen`, `pclose`, and `system`.
They must also give `pause`, `sigsuspend`, `sigwait*`, `sigqueue`, `sig2str`,
`str2sig`, `strsignal`, `psignal`, and `psiginfo`. The library must give
`<threads.h>` ([threads.h][hdr-thr]), remaining Base `pthread_*` and `sched_*`
helpers, and Ring 3 `MAP_SHARED` named `sem_open`/`sem_close`/`sem_unlink`. Ring
3 must give `sleep`, `time`, `timeout`, `nohup`, and `sh` `wait`
({need}`SYS_PROC_0001`, {need}`SYS_SIG_0001`).
```

```{swr} Load ET_DYN executables with PT_INTERP and SysV auxv
:id: SWR_PROC_0013
:status: active
:derives: SYS_PROC_0001, SYS_BOOT_0001
:traces: ADR_0002, ADR_0057

The kernel must load `ET_DYN` user executables at a fixed virtual address and
map the `PT_INTERP` dynamic linker (`/boot/ld.so`) from `bootfs` without new
syscalls. It must write the SysV `auxv` entries `AT_PHDR`, `AT_PHENT`,
`AT_PHNUM`, `AT_BASE`, and `AT_ENTRY` on the initial stack for dynamic and
static executables ({need}`SYS_PROC_0001`, {need}`SYS_BOOT_0001`).
```

[hdr-wait]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_wait.h.html
[fn-waitpid]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/waitpid.html
[fn-waitid]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/waitid.html
[fn-ufork]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/_Fork.html
[hdr-thr]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/threads.h.html
[fn-sysconf]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/sysconf.html
[xcu-ps]:
  https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ps.html
[hdr-pwd]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/pwd.h.html
[hdr-grp]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/grp.h.html
[hdr-rlim]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_resource.h.html
[hdr-spawn]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/spawn.h.html
[hdr-pth]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/pthread.h.html
[hdr-sem]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/semaphore.h.html
[hdr-time]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/time.h.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
