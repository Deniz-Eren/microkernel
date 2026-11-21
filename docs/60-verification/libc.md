# Verification — LIBC

Domain `LIBC`: Verification items covering the freestanding Ring 3 C17 library,
POSIX.1-2024 header hygiene, and thread-local `errno`.

```{ver} Verify freestanding libc build and Ring 3 runtime execution
:id: VER_LIBC_0001
:status: active
:tags: integration
:verifies: SYS_LIBC_0001, SWR_LIBC_0001

Verified by the four-stage Clang `-ffreestanding -Wall -Wextra -Werror` build
and headless QEMU execution of `sysproc`, `ipcserv`, `ipccli`, and `sigtest`
through `tests/test_runner.py` ({need}`SYS_LIBC_0001`, {need}`SWR_LIBC_0001`).
```

```{ver} Verify standalone POSIX.1-2024 header matrix and namespace hygiene
:id: VER_LIBC_0002
:status: active
:tags: unit
:verifies: SWR_LIBC_0002, SWR_LIBC_0003

CTest `posix.headers` runs `PosixHeaderMatrixTests` in
`tests/posix/test_headers.py`. It compiles each Issue 8 header standalone under
`-std=c17 -D_POSIX_C_SOURCE=202405L` and `-D_XOPEN_SOURCE=800`. It also checks
`_POSIX_VERSION`, `<unistd.h>` namespace isolation, and `<errno.h>` constants
({need}`SWR_LIBC_0002`, {need}`SWR_LIBC_0003`).
```

```{ver} Verify thread-local errno isolation and preservation in QEMU
:id: VER_LIBC_0003
:status: active
:tags: integration
:verifies: SWR_LIBC_0003

The headless QEMU harness `tests/test_runner.py` runs `tests/sigtest/main.c`. It
checks that `errno` starts at `0`, records `EBADF` on bad `write`, stays intact
on success, and stays isolated across threads ({need}`SWR_LIBC_0003`).
```
