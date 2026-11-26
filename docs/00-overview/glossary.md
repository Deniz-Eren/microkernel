# Glossary

| Term | Description |
| --- | --- |
| Need item | One uniquely identified item in Sphinx-Needs. |
| Domain | An uppercase `[A-Z0-9]+` subsystem identifier in item IDs. |
| Forward link | A relation recorded one time on the lower or derived item. |
| Ring 0 | x86-64 kernel privilege level (CPL 0) with full hardware access. |
| Ring 3 | x86-64 user privilege level (CPL 3) for isolated processes. |
| Syscall | Ring 3 call into Ring 0 services with `syscall` and `sysret`. |
| PCB / TCB | Kernel Process Control Block and Thread Control Block records. |
| Channel | A kernel receive endpoint owned by one Ring 3 process (`chanid`). |
| Connection | A client link (`connid`) to a `(pid, chanid)` endpoint. |
| IPC | Synchronous message and pulse sends between Ring 3 processes. |
| Pulse | A fixed 8-byte asynchronous signal sent through a connection. |
| PMM / VMM | Physical 4 KiB frame allocator and virtual page-table manager. |
| PML4 | Top-level x86-64 four-level page table for one address space. |
| HHDM | Higher-Half Map (`0xFFFF800000000000`) of physical RAM. |
| W^X | Write XOR Code: no page is writable and executable together. |
| SMEP / SMAP | CPU guards that prevent Ring 0 code or user read of Ring 3. |
| COW | Copy-on-Write: shared read-only pages copied on a write `#PF`. |
| Bootfs | `boot.tar` ustar archive of Ring 3 executables that GRUB loads. |
| Sysroot | `sysroot/usr/` tree that keeps public headers, `libc.a`, `crt0.o`. |
| Async-signal-safe | Function safe in a signal handler (no heap or mutexes). |
| POSIX.1-2024 | IEEE Std 1003.1-2024 / ISO/IEC/IEEE 9945:2026 target. |

POSIX® is a registered trademark of the Institute of Electrical and Electronic
Engineers, Inc.
