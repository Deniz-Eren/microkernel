# System Context

## System Overview

`microkernel` is a small 64-bit operating system working toward compatibility
with interfaces from POSIX.1-2024 (IEEE Std 1003.1-2024 / ISO/IEC/IEEE
9945:2026) in user space. The Ring 0 kernel runs at the higher-half virtual
address `0xFFFFFFFF80000000`. It supplies preemptive 256-priority scheduling,
synchronous and scatter-gather IPC, threads, `FS.base` TLS, and futexes. It also
supplies clocks and timers, signals, W^X and copy-on-write page tables, IOAPIC
IRQ pulses, and `syscall`/`sysret` dispatch.

GRUB boots the kernel with the Multiboot2 protocol. The kernel starts in 32-bit
mode, builds 64-bit higher-half page tables, and jumps to long mode. The first
user process `sysproc` loads from a `ustar` archive (`boot.tar`) on the boot
ISO. `sysproc` hosts the system name registry so servers register service names
and clients resolve them. All other system services and device drivers run as
isolated Ring 3 processes.

## System Topology

The diagram below shows how user applications, Ring 3 system servers, privileged
Ring 3 device drivers, the Ring 0 kernel, and platform hardware connect.

```mermaid
flowchart TD
    subgraph RING3_APP["Ring 3: Userland Shell, Utilities & Test Binaries"]
        direction TB
        SH["bin/ Command Shell (sh) & Core Utilities (echo, cat, ls, test)"]
        APPS["User & Test Binaries (linked with crt0.o + libc.a)"]
        LIBC["Ring 3 libc (stdio, pthreads, VFS, sockets, termios)"]
        SH --> LIBC
        APPS --> LIBC
    end

    subgraph RING3_SRV["Ring 3: Isolated System Servers and Drivers"]
        direction TB
        subgraph CORE_SRV["Core System & IPC Servers"]
            direction TB
            SYSPROC["sysproc (PID 1, Chan 1: /dev/* name registry)"]
            VFS["vfsserv (/dev/vfs: bootfs + tmpfs)"]
            PIPE["pipeserv (/dev/pipe: pipes + ppoll)"]
            SOCK["sockserv (/dev/sock: AF_UNIX + accept4)"]
            ECHO["ipcserv (/dev/echo: reference service)"]
            SYSPROC ~~~ VFS
            VFS ~~~ PIPE
            PIPE ~~~ SOCK
            SOCK ~~~ ECHO
        end
        subgraph DRV_SRV["CAP_SYS_INTR Hardware Drivers (IOPL=3)"]
            direction TB
            TTY["ttyserv (/dev/tty: COM1 IRQ 4 + termios)"]
            RTC["rtcdrv (/dev/rtc: CMOS RTC IRQ 8)"]
            TTY ~~~ RTC
        end
    end

    subgraph RING0["Ring 0: Minimal Microkernel (0xFFFFFFFF80000000)"]
        direction TB
        SYSCALL["syscall / sysret Dispatch & FD Table"]
        IPC["Synchronous IPC Channels, MsgV & Pulses"]
        SCHED["256-Priority Scheduler, Threads & Futexes"]
        MM["PMM / VMM (W^X, COW, SHM, HHDM, SMAP)"]
        SIGTIME["Signals & Monotonic / Realtime Clocks"]
        IRQ["IOAPIC IRQ-to-Pulse Router & CAP_SYS_INTR"]
        SYSCALL --> IPC
        SYSCALL --> SCHED
        IPC ~~~ MM
        SCHED ~~~ SIGTIME
        MM ~~~ IRQ
    end

    subgraph HW["Hardware / QEMU Platform"]
        direction TB
        CPU["64-Bit CPU (MMU, Timer) & Bootloader"]
        IOAPIC["IOAPIC Interrupt Controller"]
        DEV["COM1 16550A UART (IRQ 4) & CMOS RTC (IRQ 8)"]
        DEV -->|"Hardware IRQs"| IOAPIC
    end

    LIBC -->|"syscall / SendMsg"| SYSCALL
    IPC <-->|"Channels & Replies"| CORE_SRV
    IPC <-->|"Channels & Replies"| DRV_SRV
    IOAPIC --> IRQ
    IRQ -->|"PULSE_CODE_IRQ"| DRV_SRV
    DRV_SRV -.->|"Port I/O (IOPL=3)"| DEV
    CPU --> SCHED
```

## Directory Structure

```text
/
├── AGENTS.md                   <- Governance for AI agents (supreme authority)
├── README.md                   <- Entry point and current user-space support
├── CHANGELOG.md                <- Release and unreleased change log
├── LICENSE.md                  <- MIT License (immutable)
├── .agent-scaffolding.yml      <- Agent governance configuration
├── .docs-traceability-setup.yml<- Sphinx-Needs model and check configuration
├── .editorconfig               <- Line length, encoding, and header rules
├── .clang-format               <- C formatter configuration (80 columns)
├── .clang-tidy                 <- Static analysis configuration
├── Containerfile               <- Podman build container image definition
├── CMakeLists.txt              <- Four-stage build and CTest integration
├── allowed_phrases.txt         <- Project multi-word technical phrases
├── technical_terms.txt         <- Project technical nouns and verbs
├── kernel/                     <- Ring 0 kernel (arch, core, include, lib)
├── libc/                       <- Freestanding C17 / POSIX.1-2024 user library
├── servers/                    <- Ring 3 system servers and device drivers
├── bin/                        <- Ring 3 command shell (sh) and core utilities
├── tests/                      <- QEMU harness, Ring 3, POSIX, and doc tests
├── config/sphinx/              <- Sphinx-Needs model, config, and tag scanner
├── tools/                      <- Traceability audit, POSIX audit, STE gate
├── scripts/                    <- Container, build, format, hook, tar scripts
├── iso/                        <- GRUB Multiboot2 boot configuration
├── cmake/                      <- Clang cross-compilation toolchain
├── build/, sysroot/            <- Generated out-of-source build outputs
└── docs/
    ├── index.md                <- Sphinx-Needs documentation root
    ├── 00-overview/            <- Context, glossary, and WDRs
    ├── 10-product/             <- Product vision and BDRs
    ├── 20-system/              <- System and software requirements (SYS_, SWR_)
    ├── 30-architecture/        <- Architecture elements (ARC_) and ADRs
    ├── 40-interfaces/          <- Interface Control Documents (ICD_)
    ├── 50-components/          <- White-box component specifications (COMP_)
    ├── 60-verification/        <- Verification specs (VER_) and matrices
    ├── records/                <- Conventions, tasks, roadmap, and history
    ├── THIRD-PARTY-NOTICES.md  <- Legal third-party attributions
    ├── agents/                 <- Build and format subagent definitions
    └── skills/                 <- Agent tool and workflow skills
```

## Component Descriptions

- `kernel/arch/x86-64/`: CPU and platform bring-up (`boot.S`), HAL (`arch.c`,
  `arch_trapframe.h`), GDT, IDT, interrupt stubs, LAPIC timer, serial port,
  linker script, and context switch.
- `kernel/core/`: Core subsystems for memory (`pmm`, `vmm`, `shm`, `kheap`),
  scheduling and processes (`sched`, `proc`, `time`, `entropy`), IPC and IRQs
  (`ipc`, `channel`, `intr`), signals (`signal`), syscalls (`syscall`), and boot
  (`elf`, `ustar`, `multiboot`).
- `kernel/include/`, `kernel/lib/`: Kernel-internal headers and freestanding
  helpers included via `kernel/` CMake include paths.
- `libc/`: Freestanding Ring 3 C17 and POSIX.1-2024 library (`crt0`, `errno`,
  `signal`, `time`, `pthread`, `semaphore`, `fd`, `vfs`, `tty`, `socket`, `aio`,
  `mman`, `malloc`, `wait`, `uchar`, `ctype`, `stdatomic`, `stdio`, `stdlib`,
  `string`, `unistd`, and `include/sys/` syscall wrappers).
- `servers/sysproc/`: First Ring 3 process (PID 1, channel 1). It hosts the
  service name registry for all Ring 3 servers.
- `servers/pipeserv/`: Ring 3 pipe and `ppoll` server registered at `/dev/pipe`.
- `servers/vfsserv/`: Ring 3 virtual filesystem server registered at `/dev/vfs`,
  serving read-only `bootfs` at `/boot` and writable `tmpfs` at `/` and `/tmp`.
- `servers/ttyserv/`: Privileged Ring 3 COM1 serial TTY server registered at
  `/dev/tty`, using `CAP_SYS_INTR` IRQ 4 pulses and `<termios.h>` line
  discipline.
- `servers/sockserv/`: Ring 3 `AF_UNIX` local sockets server registered at
  `/dev/sock`, supporting `SOCK_STREAM`, `SOCK_DGRAM`, `accept4`, and `ppoll`.
- `servers/ipcserv/`: Ring 3 reference echo server registered at `/dev/echo`.
- `servers/rtcdrv/`: Privileged Ring 3 MC146818 CMOS RTC driver registered at
  `/dev/rtc`, using `CAP_SYS_INTR` port I/O and IOAPIC IRQ 8 pulses.
- `bin/`: Ring 3 command shell (`sh`) and standalone core utilities (`echo`,
  `cat`, `ls`, `true`, `false`, `test`) targeting POSIX.1-2024 XCU interfaces
  and packed into `boot.tar`.
- `tests/`: `test_runner.py` (headless QEMU serial assertion harness, documented
  in `docs/60-verification/qemu-harness.md` and
  `docs/skills/qemu-debug/SKILL.md`), Ring 3 test binaries (`ipccli`, `sigtest`,
  `proctest`, `procchild`, `drvtest`, `timetest`, `pthreadtest`, `forkpipetest`,
  `exechelper`, `aiotest`, `canceltest`, `libctest`, `heaptest`, `vfstest`,
  `shmtest`, `ttytest`, `stdiotest`, `socktest`, `shtest`, `utilstest`,
  `jobtest`, `simdtest`, `mathtest`, `blktest`, `ext2test`, `smptest`,
  `stresstest`), `posix/` compatibility gates, and `traceability/` unit tests.
- `config/sphinx/`, `tools/`: Sphinx-Needs model (`needs_model.py`), code tag
  checker (`need_tag_check.py`), `check_needs_audit.py`, `audit_lib.py`, and
  `check_ste.py`.
- `scripts/`: `env.sh` (Podman workflow), `enter-dev.sh` (interactive shell),
  `mk_boot_tar.py` (ustar packer), `build-agent.sh` (all pre-commit gates),
  `formatter-agent.sh`, and `install-hooks.sh`.
- `cmake/clang-x86-64.cmake`, `cmake/arch/x86-64.cmake`: Clang cross-toolchain
  and platform build settings (`-DMICROKERNEL_ARCH=x86-64`, LLD, LLVM binutils).
- `Containerfile`: Debian container with Clang, LLD, CMake, Ninja, GRUB tools,
  xorriso, mtools, QEMU, Python 3, `clang-format`, and `clang-tidy`.

## Integration Points

- `AGENTS.md`: Supreme agent governance linking to `docs/skills/` and
  `docs/records/`.
- `.agents/skills`: Symlink to `docs/skills` for cross-platform skill discovery.
- `scripts/env.sh`: Drives `Containerfile` (`microkernel-dev`), CMake, QEMU,
  `clang-format`, `clang-tidy`, and CTest.
- `CMakeLists.txt`: Orchestrates the four-stage OS build (`libc/`, `servers/`,
  `tests/`, `kernel/`, `scripts/mk_boot_tar.py`, `iso/boot/grub/grub.cfg`) and
  connects `docs/` and CTest.
- `build/boot.tar`: Packs Ring 3 servers and test binaries for the kernel ELF
  loader.
- `build/compile_commands.json`: Exported at configure time for `clang-tidy`.
- `scripts/build-agent.sh` and `scripts/install-hooks.sh`: Run formatting,
  `check_text.py --root .`, `tools/check_ste.py --root .`, build + Sphinx `-W`,
  headless QEMU tests, `clang-tidy`, and full CTest.
- `technical_terms.txt` and `allowed_phrases.txt`: Project term lists used by
  optional `tools/check_ste.py` (`ste-writing-skill`).
