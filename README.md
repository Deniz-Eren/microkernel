# Microkernel

Microkernel is a small operating system for x86-64 computers that targets
**POSIX.1-2024 (IEEE Std 1003.1-2024, Issue 8) Base** compliance in user space.
The Ring 0 kernel stays small and supplies its services to user processes.
Servers and applications run as isolated user processes in Ring 3 and send
messages to each other.

## Commands

```bash
./scripts/env.sh build
./scripts/env.sh test --timeout 120 \
    --expect '\[OK\] PMM/VMM/APIC initialized' \
    --expect 'name resolved /dev/echo' \
    --expect 'errno thread-local and preserved across success' \
    --expect 'signal handler entered'
./scripts/env.sh ctest --test-dir build --output-on-failure
```

For the full procedures, see the [cmake skill](docs/skills/cmake/SKILL.md). For
the build, test, and lint in one step, run `scripts/build-agent.sh`.

## Development Environment

```bash
./scripts/enter-dev.sh
```

The build runs in a Podman container. The image `microkernel-dev` contains the
Clang compiler, CMake, and QEMU. Your host must have Podman. The first run of an
`env.sh` command makes the image. For more information, see the [dev-environment
skill](docs/skills/dev-environment/SKILL.md).

## System services

The kernel supplies these services:

- preemptive scheduling of threads with 256 priority levels
- synchronous messages between processes, with channels, connections, replies,
  and pulses
- threads, and signals with user handlers
- memory management with page tables and the W^X policy
- fast system calls with the `syscall`/`sysret` instructions

W^X means: the system maps a page as writable or as code, but not the two
together.

The process `sysproc` is the first user process. It contains the name registry
of the system. Servers register a name with `sysproc`. Clients find the servers
with this name.

## User-space functionality (current support)

Developers can build Ring 3 C17 servers and applications packed into `boot.tar`.
The headers below show all user-space functions available at this time:

- **POSIX core (`<unistd.h>`):** `_POSIX_VERSION` and `_POSIX2_VERSION`
  (`202405L`), `fork` (COW), `execve` (bootfs `argv`/`envp` and `O_CLOEXEC`),
  `getpid`, `getppid`, `pipe`, `pipe2`, `dup`, `dup2`, `dup3`, `close`, `write`,
  `read`, `_exit`, `sleep`, `environ`, `STDIN_FILENO`, `STDOUT_FILENO`,
  `STDERR_FILENO`
- **POSIX descriptors and polling (`<fcntl.h>`, `<poll.h>`):** `fcntl`
  (`F_DUPFD`, `F_DUPFD_CLOEXEC`, `F_GETFD`, `F_SETFD`, `F_GETFL`, `F_SETFL`),
  `O_CLOEXEC`, `O_NONBLOCK`, `O_SYNC`, `O_DSYNC`, `poll`, and `ppoll` (finite
  timeout and atomic `sigmask`) backed by Ring 3 `pipeserv` (`/dev/pipe`)
- **POSIX asynchronous I/O (`<aio.h>`):** `aio_read`, `aio_write`, `aio_fsync`,
  `aio_cancel`, `lio_listio` (`LIO_WAIT`/`LIO_NOWAIT`, `SIGEV_SIGNAL`), and
  async-signal-safe `aio_error`, `aio_return`, and `aio_suspend` backed by a
  Ring 3 worker thread over `SYS_FUTEX`
- **Thread-local errors (`<errno.h>`):** O(1) `FS.base` TLS `errno`
  (`__errno_location`, with lock-free fallback) and POSIX.1-2024 `E*` constants
- **POSIX signals (`<signal.h>`):** async-signal-safe `sigaction`,
  `sigprocmask`, `sigpending`, `pthread_sigmask`, `kill`, `pthread_kill`,
  `raise`, `signal`, `sigemptyset`, `sigfillset`, `sigaddset`, `sigdelset`,
  `sigismember`, `SIGKILL`/`SIGSTOP` `EINVAL`, and mask reload across
  `SYS_SIGRETURN`
- **POSIX memory mapping (`<sys/mman.h>`):** W^X-enforced anonymous `mmap`,
  `mprotect`, `msync`, and `munmap` (`PROT_*`, `MAP_PRIVATE`, `MAP_ANONYMOUS`,
  `MAP_FIXED`, `MS_*`)
- **POSIX process wait (`<sys/wait.h>`):** async-signal-safe `wait`, `waitpid`
  (`WNOHANG`), and exit/signal status macros (`WIFEXITED`, `WEXITSTATUS`,
  `WIFSIGNALED`, `WTERMSIG`)
- **POSIX clocks and timers (`<time.h>`):** `CLOCK_MONOTONIC`, `CLOCK_REALTIME`,
  `TIMER_ABSTIME`, `clock_getres`, `clock_gettime`, `clock_settime`,
  `clock_nanosleep`, `nanosleep`, `timer_create`, `timer_settime`,
  `timer_gettime`, `timer_getoverrun`, and `timer_delete`
- **POSIX threads and semaphores (`<pthread.h>`, `<semaphore.h>`):**
  `pthread_create`, `pthread_join`, `pthread_detach`, `pthread_exit`,
  `pthread_once`, TSD keys, mutexes (`NORMAL`/`ERRORCHECK`/`RECURSIVE`),
  `pthread_cond_*`, `pthread_cond_clockwait`, barriers, spinlocks, rwlocks, and
  `sem_*` (with async-signal-safe `sem_post`)
- **Formatted I/O (`<stdio.h>`):** `printf`, `snprintf`, `puts`, `putchar`
- **Heap and exit (`<stdlib.h>`):** `malloc`, `free`, `calloc`, `exit`, `abort`,
  `strtol`, `atoi`
- **Memory and strings (`<string.h>`):** `memset`, `memcpy`, `memmove`,
  `memcmp`, `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strchr`
- **Freestanding C17 (compiler headers):** `<stdint.h>`, `<stddef.h>`,
  `<stdbool.h>`, `<stdarg.h>`, `<limits.h>`
- **Synchronous IPC (`<sys/ipc.h>`):** `CreateChannel`, `DestroyChannel`,
  `OpenConnection`, `CloseConnection`, `SendMsg`, `ReceiveMsg`, `ReplyMsg`,
  `ReadMsgV`, `WriteMsgV`, `SendPulse`, and `sysproc` name registry
  (`NAME_MSG_ATTACH`, `NAME_MSG_QUERY`)
- **Hardware IRQs and port I/O (`<sys/intr.h>`):** `AttachIntrEvent` (IOAPIC
  IRQ-to-IPC pulses), `IntrUnmask`, `IOPLSet` (`CAP_SYS_INTR` guarded), and Ring
  3 `port_in*`/`port_out*` helpers
- **Native processes and VM (`<sys/proc.h>`, `<sys/vm.h>`):** `ProcCreate` (from
  `boot.tar`), `ProcWait`, `VMMap`, `VMUnmap`, `VMProtect`, and `VMCloneCow`
  (copy-on-write page sharing between parent and child)
- **Native threads (`<sys/thread.h>`):** `NewThread`, `ThreadReset`,
  `ThreadSpawn`, `ThreadGetCtx`, `ThreadGetTid` (256 priority levels with IPC
  priority inheritance)

What you can build today:

- isolated Ring 3 client and server processes that find each other by name
  through `sysproc` and send synchronous or scatter-gather messages and pulses
- privileged Ring 3 hardware device drivers (for example `rtcdrv` at `/dev/rtc`)
  using port I/O and IOAPIC IRQ-to-IPC pulse delivery
- parent and child Ring 3 processes spawned with `ProcCreate` that share
  copy-on-write memory (`VMCloneCow`), manage private pages (`mmap`/`munmap`),
  and report exit codes with `waitpid`
- multithreaded Ring 3 applications with isolated `errno` for each thread,
  priority scheduling, and POSIX signal handlers and masks
- freestanding C17 console tools and protocol services packed into `boot.tar`

Not available at this time (next tasks): `pthread` cancellation points,
`<stdio.h>` stream locking, POSIX shell, file systems, and sockets. See
[docs/records/TASKS.md](docs/records/TASKS.md) and
[docs/records/ROADMAP.md](docs/records/ROADMAP.md).

## POSIX.1-2024 compliance and bidirectional traceability

Microkernel targets **POSIX.1-2024 (Issue 8) Base** (`_POSIX_VERSION = 202405L`,
`_POSIX2_VERSION = 202405L`) in user space. The Ring 0 kernel stays small while
`libc` and Ring 3 system servers supply standard POSIX interfaces through kernel
channels and syscalls:

- standalone C17 headers with clean namespaces (`-D_POSIX_C_SOURCE=202405L` and
  `-D_XOPEN_SOURCE=800`), thread-local `errno`, and async-signal-safe
  `<signal.h>`
- planned mandatory Issue 8 Base groups: `pthread_*`, `sem_*`, monotonic clocks
  and timers, `mmap`/`mprotect`/`msync`/`munmap`, `pipe2`/`dup3`/`ppoll`, and
  `aio_*`
- optional POSIX groups (`SHM`, `SPN`, `ML`, `MLR`, `MSG`) behind their
  `_POSIX_*` feature macros

The project keeps **bidirectional requirements traceability** with Sphinx,
MyST-Parser, and Sphinx-Needs (`docs/`). All active requirements link in both
directions across three layers:

1. **Standards and requirements (`SYS_`, `SWR_`):** written in ASD-STE100 Issue
   9 with `:posix:` links to IEEE Std 1003.1-2024.
2. **Design and source code (`ARC_`, `ICD_`, `COMP_`):** all public headers have
   an `ICD_` `need::` tag and all source modules have a `COMP_` `need::` tag.
3. **Automated verification (`VER_`):** all test files have a `VER_` tag and run
   in CI (`posix.headers`, `posix.async_signal_safe`, `docs.html`, `docs.audit`,
   and headless QEMU runs).

The Sphinx build (`-W`) stops with an error if a code tag points to an unknown
item. It also stops if an active `ICD_`, `COMP_`, or automated `VER_` item has
no code tag, or if a test file has no `VER_` tag. See
[WDR_0002](docs/00-overview/wdr/WDR_0002.md),
[BDR_0003](docs/10-product/bdr/BDR_0003.md),
[ADR_0003](docs/30-architecture/adr/ADR_0003.md), and the [traceability and
POSIX.1-2024 compliance matrix](docs/60-verification/traceability.md).

## Source tree

- `kernel/` — the kernel: boot code, x86-64 code, and the core subsystems
- `libc/` — a small C library for user processes
- `servers/` — the system servers (`sysproc`, `pipeserv`, `ipcserv`, `rtcdrv`)
- `tests/` — the test harness (`test_runner.py`) and the test programs
  (`ipccli`, `sigtest`, `proctest`, `drvtest`, `timetest`, `pthreadtest`,
  `forkpipetest`, `exechelper`, `aiotest`)
- `scripts/` — helper scripts for the environment, the build, and the agents
- `docs/` — the project documents and the agent skills
- `iso/` — the boot configuration for the GRUB boot loader
- `cmake/` — the Clang toolchain for CMake
- `Containerfile` — the definition of the build container image

The kernel starts at a high virtual address (`0xFFFFFFFF80000000`). The first
user process comes from a `ustar` archive on the boot ISO. The kernel loads each
program from this archive into a new process.

## How the build runs

The build has four stages. Each stage gives its output to the next stage:

1. The public headers go into `sysroot/usr/include`.
2. The C library and `crt0` go into `sysroot/usr/lib`.
3. The linker connects the user programs to this library. A script puts the
   programs into `boot.tar`.
4. The linker connects the kernel, and a tool makes the boot ISO.

The stage commands and the test harness usage are in the [cmake
skill](docs/skills/cmake/SKILL.md).

## How the tests work

The test harness starts QEMU without a display. The kernel and the user
processes write their status to the serial port. The harness reads the serial
output and looks for text patterns. You give the patterns with the `--expect`
option. A missing pattern or a kernel panic causes exit code 1. The expectations
include all milestones: the kernel boots, the first user process starts in Ring
3, the name registry resolves servers, messages move between processes, and a
signal handler runs.

## Code rules

The kernel and the library use the C17 standard, with warnings as errors. The
kernel code runs without a C runtime. The kernel maps its code read-only. Data
pages are not code pages. The same policy applies to user programs.

The kernel does not write through user pointers directly. It moves the data
through the physical memory map. It examines the page permissions before each
copy. The file naming, the document formatting, and the copyright header rules
are in [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md).

## Status

See [docs/records/TASKS.md](docs/records/TASKS.md) and
[docs/records/HISTORY.md](docs/records/HISTORY.md) for the milestone plan and
task records. The build completed milestones 1 to 6, POSIX clocks/timers, and
pthreads/semaphores. It also completed `fork`/`execve`/`pipe2`/`dup3`/`ppoll`
and `<aio.h>` asynchronous I/O. The next task adds `pthread` cancellation points
and `<stdio.h>` locking.

## Documentation

- [AGENTS.md](AGENTS.md): governance for AI agents (commit mode, workflow,
  pre-commit checklist)
- [docs/index.md](docs/index.md): the Sphinx-Needs requirements, architecture,
  and POSIX.1-2024 traceability model
- [docs/00-overview/context.md](docs/00-overview/context.md): system overview,
  directory structure, and integration points
- [docs/records/CONVENTIONS.md](docs/records/CONVENTIONS.md): file names, source
  text, ASD-STE100, traceability, and copyright rules
- [docs/30-architecture/adr/index.md](docs/30-architecture/adr/index.md),
  [docs/10-product/bdr/index.md](docs/10-product/bdr/index.md), and
  [docs/00-overview/wdr/index.md](docs/00-overview/wdr/index.md): architecture,
  business, and workflow decision records
- [docs/records/TASKS.md](docs/records/TASKS.md),
  [docs/records/ROADMAP.md](docs/records/ROADMAP.md), and
  [docs/records/HISTORY.md](docs/records/HISTORY.md): active tasks, roadmap, and
  completed milestones
- Skills:
  - [cmake](docs/skills/cmake/SKILL.md) — build, test, and user programs
  - [clang-format](docs/skills/clang-format/SKILL.md) — code formatting
  - [clang-tidy](docs/skills/clang-tidy/SKILL.md) — static analysis
  - [dev-environment](docs/skills/dev-environment/SKILL.md) — the container
    environment
  - [git](docs/skills/git/SKILL.md) — commits and branches
  - [state-management](docs/skills/state-management/SKILL.md) — tasks, history,
    decisions, notices
  - [keepachangelog](docs/skills/keepachangelog/SKILL.md) — the changelog format
  - [create-skill](docs/skills/create-skill/SKILL.md) — new agent skills

## License

The MIT License governs this project. See [LICENSE.md](LICENSE.md).

Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>

For third-party attributions, see
[docs/THIRD-PARTY-NOTICES.md](docs/THIRD-PARTY-NOTICES.md).
