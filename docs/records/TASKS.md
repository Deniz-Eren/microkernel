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

### TSK_0059 — Cross-compile OpenSSL into the Ring 3 sysroot via CMake

```{task} Cross-compile OpenSSL into the Ring 3 sysroot via CMake
:id: TSK_0059
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Keep only CMake build configurations in the repository. Download and build
OpenSSL (`libcrypto.a`, `libssl.a`, and headers) during the CMake build without
cloning upstream source. Verify SHA-256, RSA, ECDSA, and TLS primitives in QEMU
and update `docs/THIRD-PARTY-NOTICES.md` ({need}`SYS_LIBC_0001`).
```

### TSK_0060 — Enable OpenSSL and zlib in OpenSSH Portable

```{task} Enable OpenSSL and zlib in OpenSSH Portable
:id: TSK_0060
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Update the CMake OpenSSH Portable build configuration to link against the Ring 3
sysroot OpenSSL and `zlib` libraries. Download all third-party sources during
the CMake build without cloning into the repository. Verify RSA, ECDSA, Ed25519,
and compressed SSH sessions in CTest ({need}`SYS_IPC_0001`,
{need}`SYS_LIBC_0001`).
```
