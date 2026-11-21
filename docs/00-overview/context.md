# System Context

## System Overview

`microkernel` is a small x86-64 operating system targeting POSIX.1-2024 (Issue
8) Base conformance in user space. The Ring 0 kernel runs at the higher-half
virtual address `0xFFFFFFFF80000000`. It supplies preemptive 256-priority
scheduling, synchronous and scatter-gather IPC, threads, `FS.base` TLS, and
futexes. It also supplies clocks and timers, signals, W^X and copy-on-write page
tables, IOAPIC IRQ pulses, and `syscall`/`sysret` dispatch.

GRUB boots the kernel with the Multiboot2 protocol. The kernel starts in 32-bit
mode, builds 64-bit higher-half page tables, and jumps to long mode. The first
user process `sysproc` loads from a `ustar` archive (`boot.tar`) on the boot
ISO. `sysproc` hosts the system name registry so servers register service names
and clients resolve them. All other system services, such as the echo server
`ipcserv` and the CMOS RTC driver `rtcdrv`, run as isolated Ring 3 processes.

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
├── allowed_phrases.txt         <- ASD-STE100 multi-word technical terms
├── technical_terms.txt         <- ASD-STE100 technical nouns and verbs
├── kernel/                     <- Ring 0 kernel (arch, core, include, lib)
├── libc/                       <- Freestanding C17 / POSIX.1-2024 user library
├── servers/                    <- Ring 3 servers (sysproc, ipcserv, rtcdrv)
├── tests/                      <- QEMU harness, Ring 3, POSIX, and doc tests
├── config/sphinx/              <- Sphinx-Needs model, config, and tag scanner
├── tools/                      <- Traceability audit and repository STE gate
├── scripts/                    <- Container, build, format, hook, tar scripts
├── iso/                        <- GRUB Multiboot2 boot configuration
├── cmake/                      <- Clang x86-64 cross-compilation toolchain
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

- `kernel/arch/x86_64/`: CPU and platform bring-up (`boot.S`), GDT, IDT,
  interrupt stubs, LAPIC timer, serial port, and context switch.
- `kernel/core/`: Core subsystems for memory (`pmm`, `vmm`, `kheap`), scheduling
  and processes (`sched`, `proc`, `time`), IPC and IRQs (`ipc`, `channel`,
  `intr`), signals (`signal`), syscalls (`syscall`), and boot (`elf`, `ustar`,
  `multiboot`).
- `kernel/include/`, `kernel/lib/`: Kernel-internal headers and freestanding
  helpers included via `kernel/` CMake include paths.
- `libc/`: Freestanding Ring 3 C17 / POSIX.1-2024 library (`crt0`, `errno`,
  `signal`, `time`, `pthread`, `semaphore`, `mman`, `wait`, `stdio`, `stdlib`,
  `string`, and `include/sys/` syscall wrappers).
- `servers/sysproc/`: First Ring 3 process. It spawns system servers and hosts
  the service name registry.
- `servers/ipcserv/`: Ring 3 echo server registered at `/dev/echo`.
- `servers/rtcdrv/`: Privileged Ring 3 MC146818 CMOS RTC driver registered at
  `/dev/rtc`, using `CAP_SYS_INTR` port I/O and IOAPIC IRQ 8 pulses.
- `tests/`: `test_runner.py` (headless QEMU serial assertion harness), Ring 3
  test binaries (`ipccli`, `sigtest`, `proctest`, `procchild`, `drvtest`,
  `timetest`, `pthreadtest`), `posix/` (`posix.headers`,
  `posix.async_signal_safe`), and `traceability/` unit tests.
- `config/sphinx/`, `tools/`: Sphinx-Needs model (`needs_model.py`), code tag
  checker (`need_tag_check.py`), `check_needs_audit.py`, and `check_ste.py`.
- `scripts/`: `env.sh` (Podman workflow), `enter-dev.sh` (interactive shell),
  `mk_boot_tar.py` (ustar packer), `build-agent.sh` (all pre-commit gates),
  `formatter-agent.sh`, and `install-hooks.sh`.
- `cmake/clang-x86_64.cmake`: Clang cross-toolchain (`x86_64-unknown-none-elf`,
  LLD, LLVM binutils).
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
- `technical_terms.txt` and `allowed_phrases.txt`: Project ASD-STE100 term lists
  used by `tools/check_ste.py`.
