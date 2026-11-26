# Microkernel Vision

## Mission

`microkernel` is a small, testable 64-bit microkernel operating system. At this
time, it targets `x86-64`, with future `arm64` planned. The Ring 0 kernel stays
small and gives only scheduling, synchronous IPC, threads, signals, and W^X
memory management. All higher-level servers and executables operate as isolated
Ring 3 processes that send messages to each other.

## Principles

- **Small kernel:** Ring 0 gives only boot initialization, W^X memory
  management, preemptive scheduling, synchronous IPC, IRQ pulses, and signals.
  All other services operate in Ring 3.
- **Always correct:** A milestone or task is completed only when its automatic
  QEMU and CTest test suite gives exit code 0 with no build warnings.
- **Memory safety:** Page tables make sure that processes obey W^X (a page is
  writable or executable, not the two together). The kernel examines page
  permissions before each copy through user pointers.
- **One source of truth:** Each tool procedure stays in one skill file in
  `docs/skills/`. Each traceable item stays one time in the Sphinx-Needs model.
- **Traceable:** Each requirement connects to an accepted business decision
  (`BDR_`) through architecture, code, and tests. Each architecture or workflow
  alternative has a decision record (`ADR_`, `BDR_`, or `WDR_`). Each
  AI-assisted commit has an `Assisted-by:` footer.

## Target Result

`microkernel` gives a Ring 0 and Ring 3 microkernel platform that succeeds on
automatic tests. Its user-space headers, `libc`, and system services target
compatibility with interfaces from POSIX.1-2024 (IEEE Std 1003.1-2024 /
ISO/IEC/IEEE 9945:2026):

- kernel boot, physical and virtual W^X memory management, and LAPIC timers
- isolated Ring 3 user processes with `syscall`/`sysret`, synchronous IPC,
  threads, signals, and `FS.base` TLS
- user-space process startup, copy-on-write memory (`VMCloneCow`), and `mmap`
- hardware interrupt pulses and `CAP_SYS_INTR` port I/O for Ring 3 drivers
- POSIX.1-2024 Base libraries, descriptors, and utilities that succeed in one
  automatic QEMU and CTest suite with exit code 0
