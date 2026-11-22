# History

## Purpose

Archive completed and cancelled planning entries (`task`, `roadmap`) moved from
[TASKS.md](TASKS.md) and [ROADMAP.md](ROADMAP.md).

## Structure

Entries are stored newest first without modifying their original IDs or links.

## Formatting Rules

- Set the status of every archived item to `done` or `cancelled`.
- Never reuse IDs from archived entries.

```{needtable} Completed tasks and roadmap items
:types: task, roadmap
:filter: docname == 'records/HISTORY' and status in ['done', 'cancelled']
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0034 — Add Base signals, waitid, _Fork, popen/system, and <threads.h>

```{task} Add Base signals, waitid, _Fork, popen/system, and <threads.h>
:id: TSK_0034
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0031, SWR_PROC_0012

Add `_Fork`, `waitid`, `execl`, `execle`, `execlp`, `fexecve`, `popen`,
`pclose`, and `system`. Add `pause`, `sigsuspend`, `sigwait`, `sigwaitinfo`,
`sigtimedwait`, `sigqueue`, `sig2str`, `str2sig`, `strsignal`, `psignal`, and
`psiginfo`. Supply C11 `<threads.h>`, remaining Base `pthread_*` and `sched_*`
helpers, named `sem_open`/`sem_close`/`sem_unlink`, and `sleep`, `time`,
`timeout`, `nohup`, and `sh` `wait` ({need}`BDR_0004`, {need}`SWR_PROC_0012`,
{need}`SYS_PROC_0001`, {need}`SYS_SIG_0001`).
```

### TSK_0033 — Add VFS links, permissions, FIFOs, and <sys/statvfs.h>

```{task} Add VFS links, permissions, FIFOs, and <sys/statvfs.h>
:id: TSK_0033
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0030, SWR_IPC_0012

Add `chmod*`, `chown*`, `faccessat`, `link*`, `symlink*`, `readlink*`, and
`mkfifo*` in `vfsserv` and `pipeserv`. Add `pread`, `pwrite`, `truncate`,
`posix_close`, `posix_getdents`, `scandir`, `alphasort`, `readdir_r`,
`<sys/statvfs.h>`, `remove`, and temp-file helpers. Add file utilities `chmod`,
`chown`, `chgrp`, `ln`, `mkfifo`, `readlink`, `realpath`, and `touch`. Also add
`rmdir`, `df`, `du`, `dd`, `basename`, `dirname`, and `pathchk`
({need}`BDR_0004`, {need}`SWR_IPC_0012`).
```

### TSK_0032 — Add credentials, <pwd.h>, <grp.h>, and <sys/resource.h>

```{task} Add credentials, <pwd.h>, <grp.h>, and <sys/resource.h>
:id: TSK_0032
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0029, SWR_PROC_0011

Add process user, group, supplementary group, `nice`, and `rlimit` fields to
`pcb_t` with kernel syscalls. Supply `<pwd.h>`, `<grp.h>`, `<sys/resource.h>`
(`getrlimit`, `setrlimit`), `getuid`/`setuid`/`geteuid`/`seteuid`,
`getgid`/`setgid`/`getegid`/`setegid`, `getgroups`, and `getlogin`/`getlogin_r`.
Add `id`, `logname`, `nice`, `renice`, and `sh` built-ins `umask` and `ulimit`
({need}`BDR_0004`, {need}`SWR_PROC_0011`).
```

### TSK_0031 — Add POSIX.1-2024 ps, sysconf/confstr/pathconf, and getconf

```{task} Add POSIX.1-2024 ps, sysconf/confstr/pathconf, and getconf
:id: TSK_0031
:status: done
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0028, SWR_PROC_0010

Add kernel `pcb_t` command metadata and read-only `FD_OP_PROC_LIST` snapshot
support for Ring 3 `ps`. Add Base `<unistd.h>` `sysconf`, `confstr`, `pathconf`,
`fpathconf`, `gethostname`, `ctermid`, `ttyname`, `ttyname_r`, `tcgetsid`,
`tcgetwinsize`, and `tcsetwinsize` with no non-POSIX `_SC_*` names. Add `ps`,
`getconf`, `tty`, and `stty`, and update Gate 2 to count standard header macros
({need}`BDR_0004`, {need}`SWR_PROC_0010`).
```

### TSK_0025 — Boot secondary CPUs via ACPI MADT and harden SMP locks

```{task} Boot secondary CPUs via ACPI MADT and harden SMP locks
:id: TSK_0025
:status: done
:traces: ADR_0003, ADR_0022, ADR_0024, SWR_PROC_0009

Start secondary CPUs under `-smp 2` behind portable HAL hooks (`INIT`/`SIPI` and
`GS.base` on `x86-64`). Harden `pmm`, `vmm`, `sched`, `ipc`, and `futex` locks
under concurrent multi-core QEMU tests ({need}`SWR_PROC_0009`).
```

### TSK_0028 — Mount writable on-disk file system in vfsserv over /dev/blk

```{task} Mount writable on-disk file system in vfsserv over /dev/blk
:id: TSK_0028
:status: done
:traces: ADR_0003, ADR_0016, SWR_IPC_0011

Mount a writable on-disk file system (such as `ext2`) inside `vfsserv` over
`/dev/blk` so files and directories stay across QEMU restarts
({need}`SWR_IPC_0011`).
```

### TSK_0024 — Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver

```{task} Add MMIO/DMA syscalls and Ring 3 PCI virtio-blk driver
:id: TSK_0024
:status: done
:traces: ADR_0003, ADR_0008, ADR_0016, SWR_IPC_0010

Add `CAP_SYS_INTR` MMIO device mapping, PCI configuration, and DMA buffer
syscalls. Build a Ring 3 PCI `virtio-blk` driver (`/dev/blk`) using IOAPIC IRQ
pulses, and verify sector persistence across QEMU restarts
({need}`SWR_IPC_0010`).
```

### TSK_0030 — Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA support

```{task} Add POSIX.1-2024 <math.h> and <fenv.h> with AVX2/FMA support
:id: TSK_0030
:status: done
:traces: ADR_0003, ADR_0026, SWR_LIBC_0009

Add `<fenv.h>` (`fegetround`, `fesetround`, exception flags) and `<math.h>`
(`sqrt`, `fma`, rounding, classification, `exp`/`log`/`pow`, and trigonometric
functions including `sin`/`cos`/`tan`/`asin`/`acos`/`atan`) using `x86-64-v3`
hardware `FMA` with Gates 1 to 3 and QEMU verification ({need}`SWR_LIBC_0009`).
```

### TSK_0029 — Enforce x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512

```{task} Enforce x86-64-v3 boot gate, XSAVE/XRSTOR, and optional AVX-512
:id: TSK_0029
:status: done
:traces: ADR_0024, ADR_0026, SWR_BOOT_0005

Add early `CPUID` gating for `x86-64-v3` (`XSAVE`, `AVX`, `AVX2`, `FMA`,
`BMI1`/`BMI2`) that halts legacy CPUs on boot. Upgrade `switch.S` to
`XSAVE`/`XRSTOR` with optional `AVX-512` `XCR0` detection, build Ring 3 with
`-march=x86-64-v3`, and add positive and negative QEMU tests
({need}`SWR_BOOT_0005`).
```

### TSK_0022 — Add TTY job control, ISIG (Ctrl-C), process groups, and utilities

```{task} Add TTY job control, ISIG (Ctrl-C), process groups, and utilities
:id: TSK_0022
:status: done
:traces: ADR_0003, ADR_0018, ADR_0021, SWR_PROC_0008

Add `ISIG` (`VINTR` to `SIGINT`) in `ttyserv`, process groups (`setpgid`,
`getpgid`, `setsid`, `tcgetpgrp`, `tcsetpgrp`), `waitpid(0)`/`-pgid`, and
`kill(-pgid)`. Add Ring 3 utilities (`mkdir`, `rm`, `cp`, `mv`, `uname`, `env`,
`wc`, `head`) for the interactive shell ({need}`SWR_PROC_0008`).
```

### TSK_0023 — Add high-leverage libc Base headers and standard C utilities

```{task} Add high-leverage libc Base headers and standard C utilities
:id: TSK_0023
:status: done
:traces: ADR_0003, WDR_0002, SWR_LIBC_0008

Register compiler freestanding headers in Gate 2 and add `<sys/utsname.h>`,
`<endian.h>`, `<sched.h>`, `<setjmp.h>`, `<sys/times.h>`, and `<fnmatch.h>`. Add
`atexit`, `qsort`, `bsearch`, `strtol`, `strtoul`, `getopt`, `alarm`, `rename`,
and `realpath` ({need}`SWR_LIBC_0008`).
```

### TSK_0027 — Add opt-in automated LLVM code coverage pipeline

```{task} Add opt-in automated LLVM code coverage pipeline
:id: TSK_0027
:status: done
:traces: ADR_0025, SWR_DOCS_0005

Add `-DMICROKERNEL_COVERAGE=ON` LLVM instrumentation, linker `__llvm_prf_*`
anchors, and a freestanding Raw Profile v8 emitter. Stream base64 records over
`arch_console_write()` via `SYS_COV_WRITE` and `test_runner.py --cov-out`. Add
`tools/coverage/generate_report.py` and `./scripts/env.sh coverage` for HTML,
LCOV, and text reports in `build-cov/` ({need}`SWR_DOCS_0005`).
```

### TSK_0026 — Isolate x86-64 platform code behind x86-64 HAL folders

```{task} Isolate x86-64 platform code behind x86-64 HAL folders
:id: TSK_0026
:status: done
:traces: ADR_0024, SWR_BOOT_0004

Move `x86-64` kernel, `libc`, and UART/RTC platform code into `x86-64` folders
behind a portable HAL. Parameterize CMake, QEMU, and audit scripts on
`-DMICROKERNEL_ARCH=x86-64` while keeping `x86-64` as the sole active target
({need}`SWR_BOOT_0004`).
```

### TSK_0020 — Add pselect and cross-subsystem leak hardening

```{task} Add pselect and cross-subsystem leak hardening
:id: TSK_0020
:status: done
:traces: ADR_0003, WDR_0002, SWR_DOCS_0004

Add `<sys/select.h>` (`select`, `pselect`) over multi-server `ppoll()` with Gate
1 to Gate 3 checks. Add kernel frame count and server handle count checks.
Verify zero leaks across `sh`, VFS, pipes, TTY, `AF_UNIX` sockets, `SHM`,
signals, and pthreads ({need}`SWR_DOCS_0004`).
```

### TSK_0021 — Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers

```{task} Add ACPI MADT/SPCR discovery and parameterized Ring 3 drivers
:id: TSK_0021
:status: done
:traces: ADR_0003, SWR_BOOT_0003, SWR_IPC_0008

Parse ACPI `MADT` IOAPIC bases and Interrupt Source Overrides (ISOs), plus
`SPCR` when present (falling back to COM1 `0x3F8`/IRQ 4). Pass discovered `argv`
(`-p`/`-i`) to `ttyserv` and `rtcdrv`, and route user `stdout`/`stderr` TX
(`OPOST`/`ONLCR`) through `ttyserv` with safe early-boot fallback
({need}`SWR_BOOT_0003`).
```

### TSK_0017 — Build Ring 3 command shell (sh) and core utilities

```{task} Build Ring 3 command shell (sh) and core utilities
:id: TSK_0017
:status: done
:traces: ADR_0003, SWR_LIBC_0006

Add Ring 3 command shell (`sh`) and minimal core utilities (`echo`, `cat`, `ls`,
`true`, `false`, `test`) targeting POSIX.1-2024 XCU. Support pipelines, file
redirections, `$'...'` quoting, `set -o pipefail`, and `read -d`
({need}`SWR_LIBC_0006`).
```

### TSK_0018 — Build Ring 3 AF_UNIX local sockets server and accept4

```{task} Build Ring 3 AF_UNIX local sockets server and accept4
:id: TSK_0018
:status: done
:traces: ADR_0003, SWR_IPC_0009

Implement Ring 3 `sockserv` (`/dev/sock`), `<sys/socket.h>`, and `<sys/un.h>`
(`socket`, `socketpair`, `bind`, `listen`, `accept`, `accept4` with
`SOCK_CLOEXEC` and `SOCK_NONBLOCK`, `connect`, `send`, `recv`) reusing
multi-server `ppoll()` ({need}`SWR_IPC_0009`).
```

### TSK_0019 — Add POSIX buffered stdio, env, execvp, and core C17 headers

```{task} Add POSIX buffered stdio, env, execvp, and core C17 headers
:id: TSK_0019
:status: done
:traces: ADR_0003, SWR_LIBC_0007

Add `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>`
POSIX limits. Implement buffered `FILE` I/O (`fopen`, `fdopen`, `fclose`,
`fread`, `fwrite`, `fgets`, `fseek`, `fileno`), `setenv`, `unsetenv`, `execv`,
`execvp`, `strdup`, `strstr`, and `strerror` ({need}`SWR_LIBC_0007`).
```

### TSK_0016 — Build Ring 3 serial TTY server, termios, and multi-server ppoll

```{task} Build Ring 3 serial TTY server, termios, and multi-server ppoll
:id: TSK_0016
:status: done
:traces: ADR_0003, SWR_IPC_0008

Implement Ring 3 `ttyserv` (`/dev/tty`) with COM1 IRQ 4 pulses, `<termios.h>`,
`isatty`, and blocking `stdin` `read()`. Add multi-server `ppoll()` routing
across pipes, VFS, and TTY descriptors ({need}`SWR_IPC_0008`).
```

### TSK_0012 — Implement option-gated POSIX memory extensions (SHM, ML, MLR)

```{task} Implement option-gated POSIX memory extensions (SHM, ML, MLR)
:id: TSK_0012
:status: done
:traces: ADR_0003, SWR_MEM_0005

Add kernel shared-frame `MAP_SHARED` backing for `shm_open`/`shm_unlink`
(`SHM`). Implement `mlock`, `munlock`, `mlockall`, and `munlockall` (`ML`/`MLR`)
behind standard `_POSIX_*` macros with QEMU and Gate 2 tests
({need}`SWR_MEM_0005`).
```

### TSK_0015 — Build Ring 3 VFS server with bootfs, tmpfs, stat, and dirent

```{task} Build Ring 3 VFS server with bootfs, tmpfs, stat, and dirent
:id: TSK_0015
:status: done
:traces: ADR_0003, SWR_IPC_0007

Implement Ring 3 `vfsserv` (`/dev/vfs`) serving read-only `bootfs` and writable
`tmpfs`. Add `<sys/stat.h>`, `<dirent.h>`, `open`/`openat`, `lseek`,
`ftruncate`, `stat`/`fstat`, `mkdir`, `unlink`, `chdir`, and `getcwd`
({need}`SWR_IPC_0007`).
```

### TSK_0014 — Implement mmap-backed thread-safe malloc and free allocator

```{task} Implement mmap-backed thread-safe malloc and free allocator
:id: TSK_0014
:status: done
:traces: ADR_0003, SWR_MEM_0006

Replace the static 1 MiB bump arena in `libc/src/stdlib.c` with a thread-safe
`mmap`/`munmap` free-list allocator. Reclaim memory on `free`, `realloc`, and
`reallocarray` ({need}`SWR_MEM_0006`).
```

### TSK_0013 — Enforce full 4-gate POSIX.1-2024 compatibility harness

```{task} Enforce full 4-gate POSIX.1-2024 compatibility harness
:id: TSK_0013
:status: done
:traces: WDR_0002, SWR_DOCS_0003

Wire automated `issue8-manifest.tsv` symbol diff reporting and cancellation
call-graph checks into CTest alongside `posix.headers` and
`posix.async_signal_safe` ({need}`SWR_DOCS_0003`).
```

### TSK_0011 — Add POSIX.1-2024 Base target functions and C11/C17 headers

```{task} Add POSIX.1-2024 Base target functions and C11/C17 headers
:id: TSK_0011
:status: done
:traces: ADR_0003, ADR_0014, SWR_LIBC_0005

Add `strlcpy`, `strlcat`, `reallocarray`, `getentropy`, and `secure_getenv`
under `-D_POSIX_C_SOURCE=202405L`, plus `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` with QEMU and CTest verification ({need}`SWR_LIBC_0005`).
```

### TSK_0010 — Add pthread cancellation points and thread-safe <stdio.h> locking

```{task} Add pthread cancellation points and thread-safe <stdio.h> locking
:id: TSK_0010
:status: done
:traces: ADR_0003, ADR_0013, SWR_LIBC_0004

Add `pthread_cancel`, `pthread_setcancelstate`, `pthread_setcanceltype`,
`pthread_testcancel`, and `<stdio.h>` stream locking (`flockfile`,
`funlockfile`) with QEMU and CTest verification ({need}`SWR_LIBC_0004`).
```

### TSK_0009 — Add POSIX.1-2024 Base target asynchronous I/O (<aio.h>)

```{task} Add POSIX.1-2024 Base target asynchronous I/O (<aio.h>)
:id: TSK_0009
:status: done
:traces: ADR_0003, ADR_0012, SWR_IPC_0005

Add `<aio.h>` (`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_fsync`, `aio_suspend`, `lio_listio`) backed by Ring 3 worker threads or
servers. Keep `<mqueue.h>` behind `_POSIX_MESSAGE_PASSING`
({need}`SWR_IPC_0005`).
```

### TSK_0007 — Add POSIX.1-2024 target fork, execve, and pipe2/dup3/ppoll

```{task} Add POSIX.1-2024 target fork, execve, and pipe2/dup3/ppoll
:id: TSK_0007
:status: done
:traces: ADR_0003, ADR_0011, SWR_PROC_0004, SWR_IPC_0004

Build `fork` on full-address-space `VMCloneCow`, `execve` from `boot.tar`,
per-process file descriptors, and `pipe2`/`dup3`/`ppoll` with QEMU and CTest
verification ({need}`SWR_PROC_0004`, {need}`SWR_IPC_0004`).
```

### TSK_0008 — Consolidate legacy scaffolding documents into Sphinx-Needs

```{task} Consolidate legacy scaffolding documents into Sphinx-Needs
:id: TSK_0008
:status: done
:traces: WDR_0009, ADR_0001, CNV_0005, CNV_0006

Port all unique milestones, decisions (`ADR-1` through `ADR-19`), conventions,
vision principles, and context from the five legacy docs into Sphinx-Needs.
Redirect governance and skills, retain `docs/THIRD-PARTY-NOTICES.md`, and remove
the five legacy files ({need}`WDR_0009`).
```

### TSK_0006 — Add POSIX.1-2024 target pthreads, semaphores, and FS.base TLS

```{task} Add POSIX.1-2024 target pthreads, semaphores, and FS.base TLS
:id: TSK_0006
:status: done
:traces: ADR_0003, ADR_0010, SWR_PROC_0005

Add per-thread `FS.base` TLS, `<pthread.h>` (`pthread_create`, `pthread_join`,
mutexes, `pthread_cond_*`, `pthread_cond_clockwait`), and `<semaphore.h>`
(`sem_*`) on top of kernel wait queues ({need}`SWR_PROC_0005`).
```

### TSK_0005 — Add POSIX.1-2024 target monotonic clocks and timers

```{task} Add POSIX.1-2024 target monotonic clocks and timers
:id: TSK_0005
:status: done
:traces: ADR_0003, ADR_0009, SWR_PROC_0006

Add kernel monotonic and realtime clock syscalls and `<time.h>`
(`clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_create`,
`timer_settime`) for `CLOCK_MONOTONIC` and `CLOCK_REALTIME`
({need}`SWR_PROC_0006`).
```

### RDM_0002 — Milestone 6: Interrupts and user-space drivers

```{roadmap} Milestone 6: Interrupts and user-space drivers
:id: RDM_0002
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004, SWR_IPC_0005, SWR_IPC_0006, ADR_0008

Deliver hardware interrupt events as IPC messages (`AttachIntrEvent`), support
`IOPLSet` and multi-buffer `ReadMsgV`/`WriteMsgV`, and run a Ring 3 QEMU device
driver ({need}`SYS_IPC_0001`).
```

### TSK_0004 — Implement Milestone 6 interrupts, port I/O, and Ring 3 driver

```{task} Implement Milestone 6 interrupts, port I/O, and Ring 3 driver
:id: TSK_0004
:status: done
:traces: SYS_IPC_0001, SWR_IPC_0004, SWR_IPC_0006, ADR_0008

Implement `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`, and multi-buffer
`ReadMsgV`/`WriteMsgV`, and verify a Ring 3 QEMU device driver in headless QEMU
({need}`SYS_IPC_0001`, {need}`SWR_IPC_0004`).
```

### RDM_0001 — Milestone 5: Processes and memory system calls

```{roadmap} Milestone 5: Processes and memory system calls
:id: RDM_0001
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004, ADR_0007

Add user-space process spawning (`ProcCreate`), virtual memory mapping (`VMMap`,
`VMUnmap`), copy-on-write cloning (`VMCloneCow`), and parent exit-code
collection ({need}`SYS_PROC_0001`, {need}`SYS_MEM_0001`).
```

### TSK_0002 — Implement Milestone 5 process and memory syscalls

```{task} Implement Milestone 5 process and memory syscalls
:id: TSK_0002
:status: done
:traces: SYS_PROC_0001, SYS_MEM_0001, SWR_MEM_0004, SWR_PROC_0004, ADR_0007

Implement `ProcCreate`, `VMMap`, `VMUnmap`, `VMCloneCow`, child exit-code
reporting, and automated QEMU verification tests to lay the foundation for POSIX
`mmap` and `waitpid` ({need}`SWR_MEM_0004`, {need}`SWR_PROC_0004`).
```

### TSK_0003 — Align signal ABI, masks, and SIGKILL/SIGSTOP rules

```{task} Align signal ABI, masks, and SIGKILL/SIGSTOP rules
:id: TSK_0003
:status: done
:traces: ADR_0003, ADR_0006, SWR_SIG_0003, SWR_SIG_0004

Add `<signal.h>` with POSIX `sigaction` and `kill`, and reject `SIGKILL` and
`SIGSTOP` in `sys_sigaction`. Keep `blocked_signals` in `sig_frame_t` and audit
async-signal-safety ({need}`SWR_SIG_0003`, {need}`SWR_SIG_0004`).
```

### TSK_0001 — Align libc headers and thread-local errno to POSIX.1-2024

```{task} Align libc headers and thread-local errno to POSIX.1-2024
---
id: TSK_0001
status: done
traces: >
  BDR_0003, ADR_0003, ADR_0005, SWR_LIBC_0002, SWR_LIBC_0003, SWR_DOCS_0003
---

Remove `<sys/microkernel.h>` leakage from `<unistd.h>` and set `_POSIX_VERSION`
and `_POSIX2_VERSION` to `202405L`. Add `<errno.h>` with thread-local `errno`
and the standalone header gate ({need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`,
{need}`SWR_DOCS_0003`).
```

### RDM_0004 — Milestone 4: Threads and signals

```{roadmap} Milestone 4: Threads and signals
:id: RDM_0004
:status: done
:traces: SYS_PROC_0001, SYS_SIG_0001

Add `NewThread`, `ThreadReset`, `ThreadGetCtx`, and kernel-allocated user
stacks. Deliver Ring 3 signal frames, `kill` with `EINTR` wakeup, and default
termination, verified by `sigtest` ({need}`SYS_PROC_0001`,
{need}`SYS_SIG_0001`).
```

### RDM_0005 — Milestone 3: Messages between processes

```{roadmap} Milestone 3: Messages between processes
:id: RDM_0005
:status: done
:traces: SYS_IPC_0001

Add priority-ordered channels, `SendMsg`, `ReceiveMsg`, `ReplyMsg`, and priority
inheritance. Add connections, cross-space permission-checked copies, `sysproc`
name registry, and `ipcserv`/`ipccli` echo verification ({need}`SYS_IPC_0001`).
```

### RDM_0006 — Milestone 2: User processes

```{roadmap} Milestone 2: User processes
:id: RDM_0006
:status: done
:traces: SYS_PROC_0001, SYS_LIBC_0001

Add the four-stage build, minimal freestanding `libc`, and `syscall`/`sysret`
dispatch. Add the preemptive scheduler, ELF loader, and Ring 3 `sysproc` loaded
from `boot.tar` ({need}`SYS_PROC_0001`, {need}`SYS_LIBC_0001`).
```

### RDM_0007 — Milestone 1: Boot and memory

```{roadmap} Milestone 1: Boot and memory
:id: RDM_0007
:status: done
:traces: SYS_BOOT_0001, SYS_MEM_0001

Add the Podman Clang/CMake build container, GRUB Multiboot2 higher-half boot,
and bitmap physical memory manager. Add the W^X virtual memory manager, GDT,
IDT, PIT-backed LAPIC timer, and QEMU harness ({need}`SYS_BOOT_0001`,
{need}`SYS_MEM_0001`).
```
