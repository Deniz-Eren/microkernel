# Glossary

| Term | Definition |
| --- | --- |
| Need | A single uniquely identified engineering item in Sphinx-Needs. |
| Domain | An uppercase `[A-Z0-9]+` subsystem identifier used in item IDs. |
| Forward link | A relationship recorded once on the lower or derived item. |
| Ring 0 | x86-64 kernel privilege level (CPL 0) with full hardware access. |
| Ring 3 | x86-64 user privilege level (CPL 3) for isolated processes. |
| Syscall | Ring 3 entry into Ring 0 services with `syscall` and `sysret`. |
| PCB / TCB | Kernel Process Control Block and Thread Control Block records. |
| Channel | A kernel receive endpoint owned by one Ring 3 process (`chanid`). |
| Connection | A client attachment (`connid`) to a `(pid, chanid)` endpoint. |
| IPC | Synchronous message and pulse exchange between Ring 3 processes. |
| Pulse | A fixed 8-byte non-blocking notification sent over a connection. |
| PMM / VMM | Physical 4 KiB frame allocator and virtual page-table manager. |
| PML4 | Top-level x86-64 four-level page table for one address space. |
| HHDM | Higher-Half Direct Map (`0xFFFF800000000000`) of physical RAM. |
| W^X | Write XOR Execute: no page is both writable and executable. |
| SMEP / SMAP | CPU guards blocking Ring 0 execution or direct read of Ring 3. |
| COW | Copy-on-Write: shared read-only pages copied on a write `#PF`. |
| Bootfs | `boot.tar` ustar archive of Ring 3 executables loaded by GRUB. |
| Sysroot | `sysroot/usr/` tree holding public headers, `libc.a`, `crt0.o`. |
| Async-signal-safe | Function safe in a signal handler (no heap or locks). |
| POSIX.1-2024 | IEEE Std 1003.1-2024 (Issue 8), target Base OS standard. |
