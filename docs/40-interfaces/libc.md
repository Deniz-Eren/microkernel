# Interfaces — LIBC

Domain `LIBC`: Public Ring 3 C headers grouped by POSIX.1-2024 interface area.

```{icd} Core freestanding C17 and POSIX base headers
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001, SWR_LIBC_0002, SWR_LIBC_0004

`libc/include/` gives `<stdio.h>` (`FILE`, `flockfile`, `ftrylockfile`,
`funlockfile`), `<stdlib.h>`, `<string.h>`, and `<sys/types.h>` installed into
`sysroot/usr/include/` ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0004`).
```

```{icd} POSIX.1-2024 unistd.h header and feature-test constants
:id: ICD_LIBC_0002
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002

`libc/include/unistd.h` declares `_POSIX_VERSION` (`202405L`), `_POSIX2_VERSION`
(`202405L`), descriptor constants, `environ`, and base wrappers with no
`<sys/microkernel.h>` leakage ({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0002`).
```

```{icd} POSIX.1-2024 errno.h and error constants
:id: ICD_LIBC_0003
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0003

`libc/include/errno.h` and `libc/include/sys/errno.h` declare POSIX.1-2024 `E*`
error constants and thread-local `errno` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`).
```

```{icd} POSIX.1-2024 <uchar.h> and Issue 8 Base library extensions
:id: ICD_LIBC_0004
:status: active
:posix: XSH/getentropy (BASE), XSH/strlcpy (CX), XBD/uchar.h (CX)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0005

`libc/include/uchar.h` declares `char16_t`, `char32_t`, `mbstate_t`, `mbrtoc16`,
`c16rtomb`, `mbrtoc32`, and `c32rtomb`. `<string.h>`, `<stdlib.h>`, and
`<unistd.h>` declare `strlcpy`, `strlcat`, `reallocarray`, `secure_getenv`, and
`getentropy` alongside `<stdatomic.h>` and `<stdalign.h>`
({need}`ARC_LIBC_0001`, {need}`SWR_LIBC_0005`).
```

```{icd} POSIX.1-2024 buffered stdio, env, execvp, and core C17 headers
:id: ICD_LIBC_0005
:status: active
:posix: XBD/ctype.h (CX), XBD/limits.h (CX), XSH/fopen (CX), XSH/exec (BASE)
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0002, SWR_LIBC_0007

`libc/include/` adds `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`,
and `<limits.h>` with POSIX limits. `<stdio.h>`, `<stdlib.h>`, `<string.h>`, and
`<unistd.h>` declare buffered `FILE` operations, `setenv`, `unsetenv`, `execv`,
`execvp`, `strdup`, `strstr`, and `strerror` ({need}`ARC_LIBC_0001`,
{need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0007`).
```
