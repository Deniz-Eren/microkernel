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

Add kernel shared-frame `MAP_SHARED` backing for `shm_open`/`shm_unlink`
(`SHM`). Implement `mlock`, `munlock`, `mlockall`, and `munlockall` (`ML`/`MLR`)
behind standard `_POSIX_*` macros with QEMU and Gate 2 tests
({need}`SWR_MEM_0005`).
```

### TSK_0016 — Build Ring 3 serial TTY server, termios, and multi-server ppoll

```{task} Build Ring 3 serial TTY server, termios, and multi-server ppoll
:id: TSK_0016
:status: open
:traces: ADR_0003, SWR_IPC_0008

Implement Ring 3 `ttyserv` (`/dev/tty`) with COM1 IRQ 4 pulses, `<termios.h>`,
`isatty`, and blocking `stdin` `read()`. Add multi-server `ppoll()` routing
across pipes, VFS, and TTY descriptors ({need}`SWR_IPC_0008`).
```

### TSK_0019 — Add POSIX buffered stdio, env, execvp, and core C17 headers

```{task} Add POSIX buffered stdio, env, execvp, and core C17 headers
:id: TSK_0019
:status: open
:traces: ADR_0003, SWR_LIBC_0007

Add `<ctype.h>`, `<assert.h>`, `<inttypes.h>`, `<strings.h>`, and `<limits.h>`
POSIX limits. Implement buffered `FILE` I/O (`fopen`, `fdopen`, `fclose`,
`fread`, `fwrite`, `fgets`, `fseek`, `fileno`), `setenv`, `unsetenv`, `execv`,
`execvp`, `strdup`, `strstr`, and `strerror` ({need}`SWR_LIBC_0007`).
```

### TSK_0018 — Build Ring 3 AF_UNIX local sockets server and accept4

```{task} Build Ring 3 AF_UNIX local sockets server and accept4
:id: TSK_0018
:status: open
:traces: ADR_0003, SWR_IPC_0009

Implement Ring 3 `sockserv` (`/dev/sock`), `<sys/socket.h>`, and `<sys/un.h>`
(`socket`, `socketpair`, `bind`, `listen`, `accept`, `accept4` with
`SOCK_CLOEXEC` and `SOCK_NONBLOCK`, `connect`, `send`, `recv`) reusing
multi-server `ppoll()` ({need}`SWR_IPC_0009`).
```

### TSK_0017 — Build Ring 3 POSIX.1-2024 XCU shell (sh) and core utilities

```{task} Build Ring 3 POSIX.1-2024 XCU shell (sh) and core utilities
:id: TSK_0017
:status: open
:traces: ADR_0003, SWR_LIBC_0006

Implement Ring 3 POSIX `sh` and minimal core utilities (`echo`, `cat`, `ls`,
`true`, `false`, `test`) supporting pipelines, file redirections, `$'...'`
quoting, `set -o pipefail`, and `read -d` ({need}`SWR_LIBC_0006`).
```

### TSK_0020 — Add pselect and end-to-end cross-subsystem POSIX hardening

```{task} Add pselect and end-to-end cross-subsystem POSIX hardening
:id: TSK_0020
:status: open
:traces: ADR_0003, WDR_0002, SWR_DOCS_0004

Add `<sys/select.h>` (`select`, `pselect`) over multi-server `ppoll()`. Run an
integrated QEMU stress and leak suite across `sh`, VFS, pipes, TTY, `AF_UNIX`
sockets, `SHM`, signals, and pthreads ({need}`SWR_DOCS_0004`).
```
