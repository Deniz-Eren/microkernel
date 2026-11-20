# Verification — LIBC

Domain `LIBC`: Verification items covering the freestanding Ring 3 C17 library
and runtime.

```{ver} Verify freestanding libc build and Ring 3 runtime execution
:id: VER_LIBC_0001
:status: active
:tags: integration
:verifies: SYS_LIBC_0001, SWR_LIBC_0001

Verified by the four-stage Clang `-ffreestanding -Wall -Wextra -Werror` build
and headless QEMU execution of `sysproc`, `ipcserv`, `ipccli`, and `sigtest`
through `tests/test_runner.py` ({need}`SYS_LIBC_0001`, {need}`SWR_LIBC_0001`).
```
