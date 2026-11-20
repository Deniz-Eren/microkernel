# Architecture — LIBC

Domain `LIBC`: Structural architecture elements for the freestanding Ring 3 C
library.

```{arc} Freestanding Ring 3 C runtime and standard library
:id: ARC_LIBC_0001
:status: active
:satisfies: SYS_LIBC_0001
:traces: ADR_0002

The `libc` subsystem installs public headers into the sysroot and provides
`crt0.o` and `libc.a` for all Ring 3 servers and test programs to satisfy C
runtime requirements ({need}`SYS_LIBC_0001`).
```
