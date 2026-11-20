# Interfaces — LIBC

Domain `LIBC`: Public Ring 3 C headers and sysroot layout.

```{icd} Public Ring 3 C headers and sysroot contract
:id: ICD_LIBC_0001
:status: active
:refines: ARC_LIBC_0001
:satisfies: SWR_LIBC_0001

`libc/include/` defines `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<unistd.h>`,
`<sys/errno.h>`, `<sys/types.h>`, and microkernel ABI headers installed into
`sysroot/usr/include/` for the C library subsystem ({need}`ARC_LIBC_0001`) to
satisfy freestanding user runtime support ({need}`SWR_LIBC_0001`).
```
