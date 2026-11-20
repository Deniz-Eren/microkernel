# System and Software Requirements — LIBC

Domain `LIBC`: Freestanding Ring 3 C17 runtime, public system headers, syscall
wrappers, and POSIX.1-2024 (Issue 8) Base alignment.

```{sys} Provide a freestanding C17 library aligned toward POSIX.1-2024
:id: SYS_LIBC_0001
:status: active

The system shall provide a freestanding Ring 3 C17 library and public header set
that supports user servers and evolves toward POSIX.1-2024 (Issue 8) Base
conformance.
```

```{swr} Provide freestanding crt0, core C helpers, and syscall wrappers
:id: SWR_LIBC_0001
:status: active
:derives: SYS_LIBC_0001

The C library shall provide `crt0` process startup, signal return trampoline,
formatted console output, string and heap helpers, and Ring 3 system call
wrappers compiled with `-ffreestanding -Wall -Wextra -Werror`
({need}`SYS_LIBC_0001`).
```

```{swr} Expose POSIX.1-2024 Base headers, feature macros, and errno
:id: SWR_LIBC_0002
:status: draft
:derives: SYS_LIBC_0001

The C library shall expose standalone POSIX.1-2024 Base headers guarded by
`_POSIX_C_SOURCE=202405L`, define `_POSIX_VERSION` as `202405L`, and maintain
thread-local `errno` across syscall wrappers ({need}`SYS_LIBC_0001`).
```
