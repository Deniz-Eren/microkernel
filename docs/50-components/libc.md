# Components — LIBC

Domain `LIBC`: White-box specifications for the Ring 3 C runtime and
thread-local `errno` storage.

```{comp} Freestanding crt0, core C helpers, and syscall wrappers
:id: COMP_LIBC_0001
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0001, ICD_LIBC_0002
:satisfies: SWR_LIBC_0001, SWR_LIBC_0003, SWR_LIBC_0004

`libc/crt/crt0.S` and `libc/src/` (`start.c`, `stdio.c`, `stdlib.c`, `string.c`,
`unistd.c`) give Ring 3 startup, `__sigreturn_trampoline`, recursive `<stdio.h>`
stream locking, heap, string helpers, and `errno` wrappers
({need}`ICD_LIBC_0001`, {need}`ICD_LIBC_0002`, {need}`ARC_LIBC_0001`).
```

```{comp} Lock-free TID-indexed thread-local errno storage
:id: COMP_LIBC_0002
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0003
:satisfies: SWR_LIBC_0003

`libc/src/errno.c` puts `__errno_location()` on a 64-slot atomic table keyed by
kernel TID. It gives lock-free, async-signal-safe `errno` for each thread
({need}`ICD_LIBC_0003`, {need}`ARC_LIBC_0001`).
```

```{comp} ISO C17 and POSIX.1-2024 Unicode conversion and Base extensions
:id: COMP_LIBC_0003
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/uchar.h (CX)
:refines: ARC_LIBC_0001, ICD_LIBC_0004
:satisfies: SWR_LIBC_0005

`libc/src/uchar.c` converts UTF-8 to and from UTF-16/UTF-32 with surrogate-pair
handling and `EILSEQ` checks. `string.c`, `stdlib.c`, and `unistd.c` provide
`strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, and `getentropy` over
`SYS_GETENTROPY` ({need}`ICD_LIBC_0004`, {need}`SWR_LIBC_0005`).
```
