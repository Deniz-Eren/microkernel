# Architecture — LIBC

Domain `LIBC`: Structural architecture elements for the freestanding Ring 3 C
library.

```{arc} Freestanding Ring 3 C runtime and standard library
:id: ARC_LIBC_0001
:status: active
:satisfies: SYS_LIBC_0001
:traces: ADR_0002

The `libc` subsystem installs public headers into the sysroot. It gives `crt0.o`
and `libc.a` for all Ring 3 servers and test programs ({need}`SYS_LIBC_0001`).
```

## Three-Layer POSIX.1-2024 Architecture (`ADR_0003`)

`libc` keeps Ring 0 small while isolating applications from kernel ABI details.
Public headers expose clean POSIX.1-2024 namespaces, while `libc.a` translates
between POSIX semantics, raw negative-`RAX` kernel syscalls, and Ring 3 servers.

```mermaid
flowchart TD
    subgraph L0["Ring 3 Userland Consumers"]
        SH["bin/sh & Core Utilities (echo, cat, ls, true, false, test)"]
    end

    subgraph L1["Layer 1: Standalone POSIX.1-2024 Headers"]
        direction TB
        HDR["unistd.h, stdio.h, pthread.h, sys/socket.h, termios.h, limits.h"]
        OPT["Option Gates (_POSIX_SHARED_MEMORY_OBJECTS, _POSIX_MEMLOCK)"]
        HDR --> OPT
    end

    subgraph L2["Layer 2: Freestanding Ring 3 libc.a + crt0.o"]
        direction TB
        TLS["FS.base TLS: thread-local errno & pthread state"]
        STDIO["Buffered FILE streams, recursive flockfile, mmap heap"]
        CANCEL["Deferred cancellation & async-signal-safe wrappers"]
        ROUTER["Descriptor router (fd.c, vfs.c, tty.c, socket.c, aio.c)"]
        TLS --> CANCEL
        STDIO --> ROUTER
    end

    subgraph L3["Layer 3: Execution Backends"]
        direction TB
        RAW["Ring 0 Raw Syscalls (negative errno in RAX)"]
        SRVS["Ring 3 Servers (sysproc, vfsserv, pipeserv, ttyserv, sockserv)"]
    end

    SH --> HDR
    OPT --> TLS
    OPT --> STDIO
    CANCEL --> RAW
    ROUTER -->|"SendMsg / ReceiveMsg / ReplyMsg"| SRVS
    SRVS -->|"Kernel Channels & Pulses"| RAW
```
