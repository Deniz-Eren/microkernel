# Product Vision

## Mission

`microkernel` is a minimal, verifiable x86-64 microkernel operating system. The
Ring 0 kernel stays small and supplies only scheduling, synchronous IPC,
threads, signals, and W^X memory management. All higher-level servers and
applications run as isolated Ring 3 user processes that send messages to each
other.

## Principles

- **Small kernel:** Ring 0 provides only boot setup, W^X memory management,
  preemptive scheduling, synchronous IPC, IRQ pulse routing, and signal
  delivery. All other services run in Ring 3.
- **Always correct:** A milestone or task is complete only when its automated
  QEMU and CTest verification suite returns exit code 0 with zero build
  warnings.
- **Memory safety:** Page tables enforce W^X (a page is writable or executable,
  never both). The kernel validates page permissions before each copy through
  user pointers.
- **Single source of truth:** Each tool procedure lives in one skill file under
  `docs/skills/`, and each traceable item lives once in the Sphinx-Needs model.
- **Traceable:** Every requirement traces from an accepted business decision
  (`BDR_`) through architecture, code, and tests. Every architectural or
  workflow choice has a decision record (`ADR_`, `BDR_`, or `WDR_`), and every
  AI-assisted commit carries an `Assisted-by:` footer.

## Target Result

`microkernel` delivers a verified Ring 0 and Ring 3 microkernel platform. Its
user-space headers, `libc`, and system services work toward compatibility with
interfaces from POSIX.1-2024 (IEEE Std 1003.1-2024 / ISO/IEC/IEEE 9945:2026):

- kernel boot, physical and virtual W^X memory management, and LAPIC timing
- isolated Ring 3 user processes with `syscall`/`sysret`, synchronous IPC,
  threads, signals, and `FS.base` TLS
- user-space process creation, copy-on-write memory (`VMCloneCow`), and `mmap`
- hardware interrupt pulses and capability-gated port I/O for Ring 3 drivers
- POSIX.1-2024 Base libraries, descriptors, and utilities verified in one
  automated QEMU and CTest run with exit code 0
