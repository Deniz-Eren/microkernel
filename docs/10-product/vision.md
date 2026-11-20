# Product Vision

`microkernel` is a minimal, verifiable x86-64 microkernel operating system. The
Ring 0 kernel stays small and provides only scheduling, synchronous IPC,
threads, signals, and W^X memory management. All higher-level services run as
isolated Ring 3 user processes.

User-space headers, the C library, and system interfaces evolve toward
POSIX.1-2024 (Issue 8) Base conformance. Every requirement traces from an
accepted business decision to automated verification tests with zero build
warnings.
