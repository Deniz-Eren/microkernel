# Interfaces — LIBC

Domain `LIBC`: Public Ring 3 C headers and sysroot layout.

```{icd} Public Ring 3 C headers and sysroot contract
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001, SWR_LIBC_0002, SWR_LIBC_0003

`libc/include/` defines standalone POSIX.1-2024 headers (`<errno.h>`,
`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`, `<sys/types.h>`) and
separate microkernel ABI headers (`<sys/microkernel.h>`, `<sys/bootinfo.h>`,
`<sys/ipc.h>`, `<sys/thread.h>`, `<sys/signal.h>`) installed into
`sysroot/usr/include/` for the C library subsystem ({need}`ARC_LIBC_0001`) to
satisfy runtime ({need}`SWR_LIBC_0001`), header hygiene ({need}`SWR_LIBC_0002`),
and thread-local `errno` ({need}`SWR_LIBC_0003`) requirements.
```
