# Components — LIBC

Domain `LIBC`: White-box specification for the Ring 3 C runtime and library
sources.

```{comp} Freestanding crt0 and C17 library implementation
:id: COMP_LIBC_0001
:status: active
:refines: ARC_LIBC_0001, ICD_LIBC_0001
:satisfies: SWR_LIBC_0001

`libc/crt/crt0.S` and `libc/src/` (`start.c`, `stdio.c`, `stdlib.c`, `string.c`,
`unistd.c`) implement Ring 3 entry, `__sigreturn_trampoline`,
`printf`/`snprintf`, heap allocation, string utilities, and syscall wrappers
({need}`ICD_LIBC_0001`) for the C library subsystem ({need}`ARC_LIBC_0001`).
```
