# Tasks

## Purpose

Track active, temporary engineering tasks (`task`) for `microkernel`.

## Structure

Tasks are ordered by optimal execution order (highest priority first) and link
into the normative model through `:traces:`.

## Formatting Rules

- Normative specifications and records must never link to `TSK_` items.
- Move completed or cancelled tasks verbatim into [HISTORY.md](HISTORY.md) with
  status `done` or `cancelled`.

```{needtable} All tasks
:types: task
:filter: docname == 'records/TASKS'
:columns: id; title; status; traces as "Traces to"; traces_back as "Traced by"
```

### TSK_0012 — Implement option-gated POSIX memory extensions (SHM, ML, MLR)

```{task} Implement option-gated POSIX memory extensions (SHM, ML, MLR)
:id: TSK_0012
:status: open
:traces: ADR_0003, SWR_MEM_0005

Expose `shm_open`/`shm_unlink` (`SHM`, reusing `ftruncate` and `MAP_SHARED`) and
`mlock`/`munlock`/`mlockall`/`munlockall` (`ML`/`MLR`) behind standard
`_POSIX_*` feature macros with automated tests ({need}`SWR_MEM_0005`).
```

### TSK_0016 — Build Ring 3 serial TTY server and termios line discipline

```{task} Build Ring 3 serial TTY server and termios line discipline
:id: TSK_0016
:status: open
:traces: ADR_0003, SWR_IPC_0008

Implement Ring 3 `ttyserv` (`/dev/tty`) driven by COM1 IRQ 4 pulses and
`<termios.h>`, enabling blocking and pollable `stdin` `read()` with canonical
line editing ({need}`SWR_IPC_0008`).
```

### TSK_0017 — Build Ring 3 POSIX.1-2024 XCU shell (sh)

```{task} Build Ring 3 POSIX.1-2024 XCU shell (sh)
:id: TSK_0017
:status: open
:traces: ADR_0003, SWR_LIBC_0006

Implement a Ring 3 POSIX shell (`sh`) supporting pipelines (`pipe2`, `fork`,
`execve`), file redirections (`open`, `dup2`), `$'...'` quoting, `set -o
pipefail`, and `read -d` ({need}`SWR_LIBC_0006`).
```

### TSK_0018 — Build Ring 3 AF_UNIX local sockets server and accept4

```{task} Build Ring 3 AF_UNIX local sockets server and accept4
:id: TSK_0018
:status: open
:traces: ADR_0003, SWR_IPC_0009

Implement Ring 3 `sockserv` (`/dev/sock`) and `<sys/socket.h>` (`socket`,
`socketpair`, `bind`, `listen`, `accept`, `accept4` with `SOCK_CLOEXEC` and
`SOCK_NONBLOCK`, `connect`, `send`, `recv`) ({need}`SWR_IPC_0009`).
```
