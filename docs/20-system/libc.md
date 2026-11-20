# System and Software Requirements — LIBC

Domain `LIBC`: Freestanding Ring 3 C17 runtime, public system headers, syscall
wrappers, and POSIX.1-2024 (Issue 8) Base alignment.

```{sys} Provide a freestanding C17 library aligned toward POSIX.1-2024
:id: SYS_LIBC_0001
:status: active

The system shall provide a freestanding Ring 3 C17 library and public header set
that supports user servers and conforms to POSIX.1-2024 (Issue 8) Base rules.
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

```{swr} Enforce Issue 8 standalone headers, feature macros, and namespaces
:id: SWR_LIBC_0002
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

Each provided POSIX.1-2024 header shall compile standalone under `-std=c17
-D_POSIX_C_SOURCE=202405L -Wall -Wextra -Werror -pedantic` and
`-D_XOPEN_SOURCE=800` ([XBD 13][xbd-13], [XBD 14][xbd-14], [XSH 2.2][xsh-02]).
`<unistd.h>` ([unistd.h][hdr-unistd]) shall define `_POSIX_VERSION` and
`_POSIX2_VERSION` as `202405L`, omit non-standard `<sys/microkernel.h>` symbols,
reserve internal names under `__` or `_[A-Z]`, and gate option groups behind
their `_POSIX_*` macros ({need}`SYS_LIBC_0001`).
```

```{swr} Provide thread-local errno isolated from raw syscalls
:id: SWR_LIBC_0003
:status: active
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

`<errno.h>` ([errno.h][hdr-errno]) shall define thread-local `errno` per [XSH
2.3][xsh-02]. Libc wrappers shall isolate raw syscall negative error codes from
`errno`, assign `errno` only on failure paths, never set `errno` to `0` or clear
it on success, and honor mandatory ("shall fail") error precedence
({need}`SYS_LIBC_0001`).
```

```{swr} Implement mandatory POSIX.1-2024 thread cancellation points
:id: SWR_LIBC_0004
:status: draft
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

Libc wrappers for mandatory cancellation points (`read`, `write`, `open`,
`close`, `nanosleep`, `clock_nanosleep`, `ppoll`, `pthread_cond_wait`,
`pthread_cond_clockwait`, `sem_wait`, `waitpid`, and the remaining Issue 8 list
in [XSH 2.9][xsh-02]) shall check deferred cancellation at safe points without
holding locks or leaking resources ({need}`SYS_LIBC_0001`).
```

```{swr} Provide new POSIX.1-2024 Base functions and C17/C11 headers
:id: SWR_LIBC_0005
:status: draft
:derives: SYS_LIBC_0001
:traces: BDR_0003, ADR_0003

Under `_POSIX_C_SOURCE=202405L`, `libc` shall expose Issue 8 Base additions
`strlcpy`, `strlcat`, `reallocarray`, `getentropy` ([getentropy][fn-getent]),
and `secure_getenv`, together with `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` ({need}`SYS_LIBC_0001`).
```

[xbd-13]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap13.html
[xbd-14]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap14.html
[xsh-02]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/V2_chap02.html
[hdr-unistd]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/unistd.h.html
[hdr-errno]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/errno.h.html
[fn-getent]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/getentropy.html
