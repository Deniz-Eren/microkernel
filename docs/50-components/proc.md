# Components — PROC

The `PROC` domain includes internal specifications for scheduling, process and
thread control, syscall dispatch, and `waitpid` wrappers.

```{comp} Priority scheduler and context switch module
:id: COMP_PROC_0001
:status: active
:refines: ARC_PROC_0001
:satisfies: SWR_PROC_0001

`kernel/core/sched.c` and `kernel/arch/x86-64/switch.S` keep 256 priority ready
queues, count slice ticks on LAPIC timer interrupts, and change kernel stacks
and FPU state ({need}`ARC_PROC_0001`).
```

```{comp} Process manager and syscall dispatcher
:id: COMP_PROC_0002
:status: active
:refines: ARC_PROC_0001, ICD_PROC_0001, ICD_PROC_0002, ICD_PROC_0003
:satisfies: SWR_PROC_0002, SWR_PROC_0003, SWR_PROC_0004, SWR_PROC_0007

`kernel/core/proc.c` and `kernel/core/syscall.c` control PCB and TCB states,
`sys_proc_create`, `sys_proc_fork`, `sys_proc_execve`, `sys_proc_wait`,
`sys_fd_op`, and `syscall_handler` dispatch ({need}`ICD_PROC_0001`,
{need}`ICD_PROC_0002`, {need}`ICD_PROC_0003`, {need}`ARC_PROC_0001`).
```

```{comp} Async-signal-safe libc waitpid wrappers
:id: COMP_PROC_0003
:status: active
:posix: XBD/sys/wait.h (BASE), XSH/waitpid (BASE)
:refines: ARC_PROC_0001, ICD_PROC_0003, ICD_PROC_0004
:satisfies: SWR_PROC_0007

`libc/src/wait.c` gives async-signal-safe `wait` and `waitpid` on `ProcWait`
with no heap allocation or mutexes ({need}`ICD_PROC_0003`,
{need}`ICD_PROC_0004`, {need}`SWR_PROC_0007`).
```

```{comp} Kernel monotonic/realtime clocks, timers, and libc <time.h> wrappers
:id: COMP_PROC_0004
:status: active
:posix: XBD/time.h (CX), XSH/clock_gettime (CX), XSH/timer_create (CX)
:refines: ARC_PROC_0001, ICD_PROC_0005
:satisfies: SWR_PROC_0006

`kernel/core/time.c` counts 1 ms LAPIC ticks for `CLOCK_MONOTONIC` and
`CLOCK_REALTIME`, wakes blocked threads, and sends POSIX timer signals.
`libc/src/time.c` gives async-signal-safe clock and timer wrappers
({need}`ICD_PROC_0005`, {need}`SWR_PROC_0006`).
```

```{comp} POSIX.1-2024 pthreads, FS.base TLS, and futex synchronization
:id: COMP_PROC_0005
:status: active
:posix: XBD/pthread.h (BASE), XSH/2.9
:refines: ARC_PROC_0001, ICD_PROC_0006
:satisfies: SWR_PROC_0005, SWR_LIBC_0004

`libc/src/pthread.c` gives `pthread_*` threads, `FS.base` TLS, TSD keys,
deferred cancellation, exit handler stacks, mutexes, condvars, barriers,
spinlocks, and rwlocks on `SYS_FUTEX` and `SYS_THREAD_SET_FSBASE`
({need}`ICD_PROC_0006`, {need}`SWR_PROC_0005`, {need}`SWR_LIBC_0004`).
```

```{comp} POSIX.1-2024 unnamed semaphore library
:id: COMP_PROC_0006
:status: active
:posix: XBD/semaphore.h (BASE), XSH/2.9
:refines: ARC_PROC_0001, ICD_PROC_0007
:satisfies: SWR_PROC_0005

`libc/src/semaphore.c` gives `sem_init`, `sem_destroy`, `sem_wait`,
`sem_trywait`, `sem_timedwait`, `sem_clockwait`, async-signal-safe `sem_post`,
and `sem_getvalue` on `SYS_FUTEX` ({need}`ICD_PROC_0007`,
{need}`SWR_PROC_0005`).
```

```{comp} Process-group control, TTY ISIG signals, and Ring 3 utilities
:id: COMP_PROC_0007
:status: active
:posix: XSH/setpgid (BASE), XSH/tcsetpgrp (BASE), XBD/termios.h (BASE)
:refines: ARC_PROC_0001, ICD_PROC_0008
:satisfies: SWR_PROC_0008

`kernel/core/proc.c`, `kernel/core/signal.c`, `servers/ttyserv/main.c`,
`libc/src/` (`unistd.c`, `tty.c`), and `bin/sh/main.c` give process groups,
sessions, `waitpid(0)`/`-pgid`, `kill(-pgid)`, `ISIG` `SIGINT`/`SIGQUIT`, and
foreground process-group control. `bin/utils/` gives `mkdir`, `rm`, `cp`, `mv`,
`uname`, `env`, `wc`, and `head` ({need}`ICD_PROC_0008`, {need}`SWR_PROC_0008`).
```

```{comp} SMP secondary CPU start, CPU state, and kernel spinlocks
:id: COMP_PROC_0008
:status: active
:refines: ARC_PROC_0001, ICD_BOOT_0002
:satisfies: SWR_PROC_0009

`kernel/core/smp.c`, `kernel/include/kernel/spinlock.h`, and
`kernel/arch/x86-64/ap_trampoline.S` start secondary CPUs through ACPI `MADT`
and keep `GS.base` state and wait threads for each CPU. SMP-safe `sched.c`,
`pmm.c`, `kheap.c`, `vmm.c`, `ipc.c`, `proc.c`, `time.c`, `signal.c`, `intr.c`,
and `shm.c` lock shared kernel state across cores ({need}`ARC_PROC_0001`,
{need}`ICD_BOOT_0002`, {need}`SWR_PROC_0009`).
```

```{comp} Process table, sysconf, TTY dimensions, and XCU utilities
:id: COMP_PROC_0009
:status: active
:posix: XCU/ps (BASE), XCU/getconf (BASE), XSH/sysconf (BASE)
:refines: ARC_PROC_0001, ICD_PROC_0009
:satisfies: SWR_PROC_0010

`kernel/core/proc.c` records `comm` and `args` on `pcb_t` and gives replies for
`FD_OP_PROC_LIST`. `libc/src/sysconf.c`, `libc/src/tty.c`, and
`servers/ttyserv/main.c` give Base `sysconf`, `confstr`, `pathconf`,
`fpathconf`, `gethostname`, `ctermid`, `ttyname*`, `tcgetsid`, and
`tcgetwinsize`/`tcsetwinsize`. `bin/utils/` gives `ps`, `getconf`, `tty`, and
`stty` ({need}`ICD_PROC_0009`, {need}`SWR_PROC_0010`).
```

```{comp} Kernel credentials, rlimits, pwd/grp databases, and identity utilities
:id: COMP_PROC_0010
:status: active
:posix: XBD/pwd.h (BASE), XBD/grp.h (BASE), XBD/sys/resource.h (BASE)
:refines: ARC_PROC_0001, ICD_PROC_0010
:satisfies: SWR_PROC_0011

`kernel/core/proc.c` and `kernel/core/sched.c` give `SYS_PROC_CRED`,
`SYS_PROC_RLIMIT`, and `RLIMIT_NOFILE` checks. `libc/src/cred.c` gives Base
`pwd.h`, `grp.h`, `sys/resource.h`, and `unistd.h` identity calls. `bin/utils/`
and `bin/sh/` give `id`, `logname`, `nice`, `renice`, `umask`, and `ulimit`
({need}`ICD_PROC_0010`, {need}`SWR_PROC_0011`).
```

```{comp} Kernel ET_DYN and PT_INTERP executable loader
:id: COMP_PROC_0011
:status: active
:refines: ARC_PROC_0001, ARC_BOOT_0001
:satisfies: SWR_PROC_0013

`kernel/core/elf.c` maps `ET_DYN` images at `ELF_DYN_BASE` and `PT_INTERP`
dynamic linkers at `ELF_INTERP_BASE` with segment W^X page permissions.
`kernel/core/proc.c` finds the dynamic linker in `bootfs` and makes the initial
stack with `AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_BASE`, and `AT_ENTRY` `auxv`
entries ({need}`ARC_PROC_0001`, {need}`SWR_PROC_0013`).
```
