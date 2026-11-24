# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must not link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0061 — Implement kernel PT_INTERP, SysV auxv, and Ring 3 ld.so

```{task} Implement kernel PT_INTERP, SysV auxv, and Ring 3 ld.so
:id: TSK_0061
:status: open
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_BOOT_0001, SYS_PROC_0001

Extend `kernel/core/elf.c` and `kernel/core/proc.c` to load `ET_DYN` and
`PT_INTERP` (`/boot/ld.so`) with zero new syscalls and emit SysV `auxv`
(`AT_PHDR`, `AT_PHENT`, `AT_PHNUM`, `AT_BASE`, `AT_ENTRY`). Build freestanding
Ring 3 `ld.so` to load `DT_NEEDED` libraries over `/dev/vfs`, apply `R_X86_64_*`
relocations, enforce W^X, run `DT_INIT_ARRAY`, and jump to `AT_ENTRY`. Verify
with a dynamic linker test in QEMU ({need}`SYS_BOOT_0001`,
{need}`SYS_PROC_0001`).
```

### TSK_0062 — Build shared libc.so and unify dlopen/dlsym with ld.so

```{task} Build shared libc.so and unify dlopen/dlsym with ld.so
:id: TSK_0062
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001, SYS_MEM_0001

Build `-fPIC` shared `libc.so` alongside static `libc.a` into the Ring 3 sysroot
and `boot.tar`. Update `crt0.S` and `__libc_start_main` handoff, add a dynamic
executable linker script with `FILEHDR PHDRS`, and delegate `<dlfcn.h>` runtime
calls to `ld.so` when loaded dynamically. Verify `stdio`, `malloc`, pthreads,
TLS `errno`, signals, `fork`/`execve`, and `dlopen` in QEMU
({need}`SYS_LIBC_0001`, {need}`SYS_MEM_0001`).
```

### TSK_0063 — Build shared libz.so.1 via CMake and link zlibtest dynamically

```{task} Build shared libz.so.1 via CMake and link zlibtest dynamically
:id: TSK_0063
:status: open
:traces: BDR_0003, BDR_0004, ADR_0055, SYS_LIBC_0001

Update `cmake/zlib.cmake` to build shared `libz.so.1` (with `libz.so` symlink)
alongside `libz.a` during the CMake build without storing upstream source in
git. Install `libz.so.1` into the Ring 3 sysroot and `boot.tar`. Link `zlibtest`
dynamically against `libz.so.1` and `libc.so`, and verify in QEMU
({need}`SYS_LIBC_0001`).
```

### TSK_0064 — Build shared libcrypto.so.3 and libssl.so.3 via CMake

```{task} Build shared libcrypto.so.3 and libssl.so.3 via CMake
:id: TSK_0064
:status: open
:traces: BDR_0003, BDR_0004, ADR_0056, SYS_LIBC_0001

Update `cmake/openssl_target.conf`, `cmake/build_openssl.sh`, and
`cmake/openssl.cmake` to build `-fPIC` shared `libcrypto.so.3` and `libssl.so.3`
alongside static archives during the CMake build without storing upstream source
in git. Install shared libraries into the sysroot and `boot.tar`, link
`openssltest` dynamically, and verify cryptography and TLS 1.3 in QEMU
({need}`SYS_LIBC_0001`).
```

### TSK_0065 — Dynamically link OpenSSH Portable against shared libs

```{task} Dynamically link OpenSSH Portable against shared libs
:id: TSK_0065
:status: open
:traces: BDR_0003, BDR_0004, ADR_0054, ADR_0055, ADR_0056, SYS_IPC_0001

Update `cmake/openssh.cmake` to link `sshd`, `sshd-session`, and `ssh-keygen`
dynamically with `--dynamic-linker=/boot/ld.so` against `libcrypto.so.3`,
`libz.so.1`, and `libc.so`. Verify `PT_INTERP` and `DT_NEEDED` headers via
`llvm-readelf` and verify RSA, ECDSA, Ed25519, compressed (`zlib@openssh.com`),
password, and PTY sessions in CTest `ipc.sshd_e2e` ({need}`SYS_IPC_0001`).
```

### TSK_0066 — Dynamically link Ring 3 bin/ shell and utilities against libc.so

```{task} Dynamically link Ring 3 bin/ shell and utilities against libc.so
:id: TSK_0066
:status: open
:traces: BDR_0003, BDR_0004, ADR_0002, SYS_LIBC_0001, SYS_PROC_0001

Convert `bin/` (`sh`, XCU utilities, `nc`, `ping`, `sshd-setup`) to dynamic
executables linked against `ld.so` and `libc.so` while keeping core Ring 3 boot
servers statically linked. Verify `boot.tar` size reduction and pass the full
QEMU and CTest suites ({need}`SYS_LIBC_0001`, {need}`SYS_PROC_0001`).
```
