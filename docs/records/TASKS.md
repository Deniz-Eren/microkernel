# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered with the highest priority first and link into the normative
model through `:traces:`.

## Formatting Rules

- Normative specifications and records must never link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0011 — Implement POSIX.1-2024 Base functions and C11/C17 headers

```{task} Implement POSIX.1-2024 Base functions and C11/C17 headers
:id: TSK_0011
:status: open
:traces: ADR_0003, SWR_LIBC_0005

Add `strlcpy`, `strlcat`, `reallocarray`, `getentropy`, and `secure_getenv`
under `-D_POSIX_C_SOURCE=202405L`, plus `<stdatomic.h>`, `<stdalign.h>`, and
`<uchar.h>` with QEMU and CTest verification ({need}`SWR_LIBC_0005`).
```

### TSK_0012 — Implement option-gated POSIX memory extensions (SHM, ML, MLR)

```{task} Implement option-gated POSIX memory extensions (SHM, ML, MLR)
:id: TSK_0012
:status: open
:traces: ADR_0003, SWR_MEM_0005

Expose `shm_open`/`shm_unlink` (`SHM`) and `mlock`/`munlock`/`mlockall`
(`ML`/`MLR`) behind standard `_POSIX_*` feature macros with automated tests
({need}`SWR_MEM_0005`).
```

### TSK_0013 — Enforce full issue8-manifest.tsv Tier-2 POSIX symbol gate

```{task} Enforce full issue8-manifest.tsv Tier-2 POSIX symbol gate
:id: TSK_0013
:status: open
:traces: WDR_0002, SWR_DOCS_0003

Wire automated `issue8-manifest.tsv` symbol and header verification into CTest
alongside the existing standalone header and async-signal-safety gates
({need}`SWR_DOCS_0003`).
```
