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

### TSK_0048 — Add XCU encoding, logging, terminal, and user utilities

```{task} Add XCU encoding, logging, terminal, and user utilities
:id: TSK_0048
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add `uuencode`, `uudecode`, `logger`, `tput`, `tabs`, `mesg`, `write`, and
`newgrp` with QEMU runtime tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0053 — Add XCU scheduling, spooling, and mail utilities

```{task} Add XCU scheduling, spooling, and mail utilities
:id: TSK_0053
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add `at`, `batch`, `crontab`, `lp`, and `mailx` to complete the remaining XCU
Base utility set ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```
