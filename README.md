# Microkernel

Microkernel is a small operating system for x86-64 computers. The kernel is small and supplies its services to user processes. Servers and applications run as user processes in Ring 3. The processes send messages to each other.

## System services

The kernel supplies these services:

- preemptive scheduling of threads with 256 priority levels
- synchronous messages between processes, with channels, connections, replies, and pulses
- threads, and signals with user handlers
- memory management with page tables and the W^X policy
- fast system calls with the `syscall`/`sysret` instructions

W^X means: the system maps a page as writable or as code, but not the two together.

The process `sysproc` is the first user process. It contains the name registry of the system. Servers register a name with `sysproc`. Clients find the servers with this name.

## Source tree

- `kernel/` — the kernel: boot code, x86-64 code, and the core subsystems
- `libc/` — a small C library for user processes
- `servers/` — the system servers (`sysproc`, `ipcserv`)
- `apps/` — the user applications (`ipccli`, `sigtest`)
- `scripts/` — helper scripts for the build and the tests
- `iso/` — the boot configuration for the GRUB boot loader
- `cmake/` — the Clang toolchain for CMake
- `Containerfile` — the definition of the build container image

The kernel starts at a high virtual address (`0xFFFFFFFF80000000`). The first user process comes from a `ustar` archive on the boot ISO. The kernel loads each program from this archive into a new process.

## How the build runs

The build runs in a container. The container image `microkernel-dev` contains the Clang compiler, CMake, and QEMU. Your host must have Podman.

1. Install Podman.
2. Run `./scripts/env.sh configure`. The first run makes the container image. This step also configures the build directory.
3. Run `./scripts/env.sh build`. This step makes the kernel, the library, the user programs, and the boot ISO.

The build has four stages. Each stage gives its output to the next stage:

1. The public headers go into `sysroot/usr/include`.
2. The C library and `crt0` go into `sysroot/usr/lib`.
3. The linker connects the user programs to this library. A script puts the programs into `boot.tar`.
4. The linker connects the kernel, and a tool makes the boot ISO.

## How the tests work

The test harness starts QEMU without a display. The kernel and the user processes write their status to the serial port. The harness reads the serial output and looks for text patterns. You give the patterns with the `--expect` option. You can give this option many times.

Run the full test with this command:

```
./scripts/env.sh test --timeout 60 \
    --expect '\[OK\] PMM/VMM/APIC initialized' \
    --expect 'name resolved /dev/echo' \
    --expect 'signal handler entered'
```

If the serial output contains all patterns before the timeout, the harness stops QEMU and exits with code 0. A missing pattern or a kernel panic causes exit code 1.

The expectations include all milestones:

- the kernel boots, and the memory manager starts
- the first user process starts in Ring 3, and the first system calls work
- a client finds a server with the name registry, and they send messages to each other
- a process makes a new thread, and a signal handler runs, and then the process continues

For interactive work, run `./scripts/env.sh shell`. This command gives you a shell in the build container.

## Code rules

The kernel and the library use the C17 standard, with warnings as errors. The kernel code runs without a C runtime. The kernel maps its code read-only. Data pages are not code pages. The same policy applies to user programs. The kernel does not write through user pointers directly. It moves the data through the physical memory map. It examines the page permissions before each copy.

## Status

See `TASKS.md` for the milestone plan. The build completed milestones 1 to 4. The next milestone adds process creation and memory system calls.
