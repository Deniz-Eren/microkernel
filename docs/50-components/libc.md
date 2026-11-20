# Components — LIBC

Domain `LIBC`: White-box specifications for the Ring 3 C runtime and
thread-local `errno` storage.

```{comp} Freestanding crt0, core C helpers, and syscall wrappers
:id: COMP_LIBC_0001
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0001, ICD_LIBC_0002
:satisfies: SWR_LIBC_0001, SWR_LIBC_0003

`libc/crt/crt0.S` and `libc/src/` (`start.c`, `stdio.c`, `stdlib.c`, `string.c`,
`unistd.c`) implement Ring 3 entry, `__sigreturn_trampoline`,
`printf`/`snprintf`, heap allocation, string utilities, and `errno`-preserving
syscall wrappers ({need}`ICD_LIBC_0001`, {need}`ICD_LIBC_0002`) for the C
library subsystem ({need}`ARC_LIBC_0001`).
```

```{comp} Lock-free TID-indexed thread-local errno storage
:id: COMP_LIBC_0002
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0003
:satisfies: SWR_LIBC_0003

`libc/src/errno.c` implements `__errno_location()` over a fixed 64-slot atomic
table indexed by the calling thread's kernel TID, providing lock-free and
async-signal-safe per-thread `errno` ({need}`ICD_LIBC_0003`) for the C library
subsystem ({need}`ARC_LIBC_0001`).
```
