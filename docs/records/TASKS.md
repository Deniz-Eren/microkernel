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

### TSK_0047 — Add 5 network Base headers and socket/resolver libc APIs

```{task} Add 5 network Base headers and socket/resolver libc APIs
:id: TSK_0047
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_IPC_0001, SYS_LIBC_0001

Add `sendmsg`, `recvmsg`, and `sockatmark` over Ring 3 socket servers. Supply
`<netinet/in.h>`, `<netinet/tcp.h>`, `<arpa/inet.h>`, `<netdb.h>`, and
`<net/if.h>` (`htons`/`htonl`/`ntohs`/`ntohl`, `inet_*`, `if_*`, `getaddrinfo`,
`getnameinfo`, `gai_strerror`, `get*ent`) to reach 70 Base headers
({need}`BDR_0004`, {need}`SYS_IPC_0001`, {need}`SYS_LIBC_0001`).
```

### TSK_0050 — Add POSIX.1-2024 XCU sort, field, and checksum text utilities

```{task} Add POSIX.1-2024 XCU sort, field, and checksum text utilities
:id: TSK_0050
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add text pipeline utilities `sort`, `uniq`, `cut`, `tr`, `tail`, `tee`, `paste`,
`join`, and `cksum` with QEMU runtime tests ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0046 — Add POSIX.1-2024 XCU diff, patch, and archive utilities

```{task} Add POSIX.1-2024 XCU diff, patch, and archive utilities
:id: TSK_0046
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add comparison, patching, and archive utilities `diff`, `cmp`, `comm`, `patch`,
and `pax` over `<tar.h>` and `<cpio.h>` with QEMU tests ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0051 — Add POSIX.1-2024 XCU split, format, and inspection utilities

```{task} Add POSIX.1-2024 XCU split, format, and inspection utilities
:id: TSK_0051
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add text-formatting and inspection utilities `split`, `csplit`, `fold`,
`expand`, `unexpand`, `strings`, `tsort`, `file`, and `pr` with QEMU tests
({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
```

### TSK_0044 — Add POSIX.1-2024 XCU awk and ed utilities

```{task} Add POSIX.1-2024 XCU awk and ed utilities
:id: TSK_0044
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add the pattern-scanning language `awk` and line editor `ed` over `<regex.h>`
and `<math.h>` with QEMU runtime tests ({need}`BDR_0004`,
{need}`SYS_LIBC_0001`).
```

### TSK_0052 — Add POSIX.1-2024 XCU bc and m4 language utilities

```{task} Add POSIX.1-2024 XCU bc and m4 language utilities
:id: TSK_0052
:status: open
:traces: BDR_0003, BDR_0004, ADR_0003, SYS_LIBC_0001

Add the arbitrary-precision calculator `bc` and macro processor `m4` with QEMU
runtime tests ({need}`BDR_0004`, {need}`SYS_LIBC_0001`).
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
