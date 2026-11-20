# Interfaces — LIBC

Domain `LIBC`: Public Ring 3 C headers grouped by POSIX.1-2024 interface area.

```{icd} Core freestanding C17 and POSIX base headers
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001, SWR_LIBC_0002

`libc/include/` defines `<stdio.h>`, `<stdlib.h>`, `<string.h>`, and
`<sys/types.h>` installed into `sysroot/usr/include/` for the C library
subsystem ({need}`ARC_LIBC_0001`) to satisfy freestanding runtime support
({need}`SWR_LIBC_0001`) and standalone header hygiene ({need}`SWR_LIBC_0002`).
```

```{icd} POSIX.1-2024 unistd.h header and feature-test constants
:id: ICD_LIBC_0002
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002

`libc/include/unistd.h` defines `_POSIX_VERSION` (`202405L`), `_POSIX2_VERSION`
(`202405L`), standard descriptor constants, `environ`, and base wrappers without
leaking `<sys/microkernel.h>` for the C library subsystem
({need}`ARC_LIBC_0001`) to satisfy header hygiene ({need}`SWR_LIBC_0002`).
```

```{icd} POSIX.1-2024 errno.h and error constants
:id: ICD_LIBC_0003
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0003

`libc/include/errno.h` and `libc/include/sys/errno.h` define the POSIX.1-2024
`E*` error constants and thread-local `errno` lvalue for the C library subsystem
({need}`ARC_LIBC_0001`) to satisfy standalone headers ({need}`SWR_LIBC_0002`)
and thread-local `errno` ({need}`SWR_LIBC_0003`).
```
