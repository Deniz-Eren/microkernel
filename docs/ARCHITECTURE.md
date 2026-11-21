## Purpose

Write the architecture of microkernel in this document. Write the files, their
structure, and the task of each component.

## Structure

The document has a system overview, the directory structure, component
descriptions, and the integration points.

## Formatting Rules

- Write about the current condition only. Put decisions in
  [DECISIONS.md](DECISIONS.md).
- Use code blocks for directory trees.
- Link to skill files for procedures. Do not write the procedures here.

---

## System Overview

Microkernel is a small operating system for x86-64 computers. The kernel runs in
Ring 0 at `0xFFFFFFFF80000000`. It gives preemptive 256-priority scheduling,
synchronous IPC, threads, and signals. It also gives W^X page tables,
copy-on-write memory, IOAPIC IRQ routing, and `syscall`/`sysret` dispatch.

GRUB boots the kernel with the Multiboot2 protocol. The kernel starts in 32-bit
mode, makes the 64-bit page tables, and jumps to the high half. The first user
process `sysproc` comes from a `ustar` archive (`boot.tar`) on the boot ISO. The
kernel loads each program from this archive into a new process with the ELF
loader. `sysproc` contains the name registry: servers register a name, and
clients find the servers with this name. All other services, for example the
echo server `ipcserv`, run as user processes in Ring 3.

## Directory Structure

```
/
├── AGENTS.md              ← Governance for AI agents (supreme authority)
├── README.md              ← Entry point
├── CHANGELOG.md           ← Changes between versions
├── LICENSE.md             ← The MIT License
├── .agent-scaffolding.yml ← Scaffolding configuration
├── .clang-format          ← Formatter configuration
├── .clang-tidy            ← Static analysis configuration
├── Containerfile          ← The build container image
├── CMakeLists.txt         ← The four-stage build
├── allowed_phrases.txt    ← STE term list (multi-word terms)
├── technical_terms.txt    ← STE term list (nouns and verbs)
├── kernel/                ← Ring 0 kernel
├── libc/                  ← C library for user processes
├── servers/               ← System servers (sysproc, ipcserv)
├── tests/                 ← Test harness and test programs
├── scripts/               ← Environment, build, and agent scripts
├── iso/                   ← GRUB boot configuration
├── cmake/                 ← Clang toolchain for CMake
├── build/, sysroot/       ← Generated output (not in git)
└── docs/
    ├── ARCHITECTURE.md    ← This file
    ├── DECISIONS.md       ← ADRs
    ├── CONVENTIONS.md     ← Names, formatting, copyright
    ├── TASKS.md           ← Task records
    ├── THIRD-PARTY-NOTICES.md
    ├── VISION.md          ← Mission and principles
    ├── agents/            ← Subagent definitions
    └── skills/            ← Agent skills
```

## Component Descriptions

- `kernel/arch/x86_64/`: CPU and machine start: boot code (`boot.S`), GDT, IDT,
  interrupt stubs, APIC timer, serial port, context switch.
- `kernel/core/`: Core subsystems. These cover memory (`pmm`, `vmm`, `kheap`),
  scheduling and processes (`sched`, `proc`), IPC and IRQs (`ipc`, `channel`,
  `intr`), signals (`signal`), syscalls (`syscall`), and boot (`elf`, `ustar`,
  `multiboot`).
- `kernel/include/`, `kernel/lib/`: Kernel-internal headers and small helpers.
- `libc/`: The C library for Ring 3: `printf`, memory allocation, string
  functions, thread-local `errno`, syscall wrappers (`include/sys/`), and `crt0`
  process start (`crt/`).
- `servers/sysproc/`: The first user process. It starts the servers and holds
  the name registry.
- `servers/ipcserv/`: The echo server. It registers `/dev/echo` and replies to
  messages.
- `tests/`: `test_runner.py` (headless QEMU serial assertion harness), `ipccli`
  (name lookup and message client), `sigtest` (threads, `errno`, and signals),
  `posix/` (POSIX.1-2024 header gate), and `traceability/` (audit unit tests).
- `scripts/`: `env.sh` (container workflow), `enter-dev.sh` (container shell),
  `mk_boot_tar.py` (ustar packer), `build-agent.sh` (build, test, lint), and
  `formatter-agent.sh` (format check and fix).
- `cmake/clang-x86_64.cmake`: The Clang cross-toolchain: target
  `x86_64-unknown-none-elf`, LLD linker, LLVM bin tools.
- `Containerfile`: Debian bookworm-slim with Clang, LLD, CMake, Ninja, GRUB
  tools, xorriso, mtools, QEMU, Python 3, clang-format, and clang-tidy.

## Integration Points

- `AGENTS.md`: Links to `docs/skills/git` and `docs/skills/state-management`.
  All agent platforms use it.
- `.agents/skills`: Symlink to `docs/skills`. Agent platforms use it for skill
  discovery.
- `scripts/env.sh`: Uses `Containerfile` (`microkernel-dev`),
  `cmake/clang-x86_64.cmake`, and `tests/test_runner.py`. It drives all build,
  test, and lint runs.
- `CMakeLists.txt`: Connects `libc/`, `servers/`, `tests/`, `kernel/`, `docs/`,
  `scripts/mk_boot_tar.py`, and `iso/boot/grub/grub.cfg`. It drives the
  four-stage build and CTest.
- `boot.tar` (build output): Packs programs from `servers/` and `tests/`. The
  kernel ELF loader reads it at boot.
- `build/compile_commands.json`: Made by configure. `clang-tidy` uses it (see
  the [clang-tidy skill](skills/clang-tidy/SKILL.md)).
- `scripts/build-agent.sh`: Runs format, `check_text.py`, `tools/check_ste.py`,
  build, QEMU tests, `run-clang-tidy`, and CTest. The [build
  agent](agents/build.md) runs it.
- `scripts/formatter-agent.sh`: Uses `.clang-format` and `env.sh`. The [format
  validator](agents/format-validator.md) runs it.
- `technical_terms.txt`, `allowed_phrases.txt`: STE term lists. Documentation
  checks use them (see [CONVENTIONS.md](CONVENTIONS.md)).
