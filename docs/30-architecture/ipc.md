# Architecture — IPC

Domain `IPC`: Structural architecture elements for synchronous message passing
and service discovery.

```{arc} Synchronous channel IPC and name service subsystem
:id: ARC_IPC_0001
:status: active
:satisfies: SYS_IPC_0001
:traces: ADR_0002

The IPC subsystem pairs kernel-managed receive channels, client connections, and
priority-inheriting message queues with the Ring 3 `sysproc` name registry to
satisfy inter-process communication ({need}`SYS_IPC_0001`).
```

## Service Discovery and Synchronous Message Passing

Servers register endpoints under `/dev/*` with `sysproc` (PID 1, channel 1).
Clients resolve a service name into a `(pid, chanid)` pair, open a connection,
and exchange synchronous request/reply messages through Ring 0.

```mermaid
sequenceDiagram
    participant Srv as Ring 3 Server (e.g. vfsserv)
    participant NS as sysproc (PID 1, Chan 1)
    participant K as Kernel IPC Engine (Ring 0)
    participant Cli as Ring 3 Client

    Srv->>K: CreateChannel()
    K-->>Srv: chanid
    Srv->>NS: SendMsg(NAME_MSG_ATTACH, "/dev/vfs", chanid)
    NS-->>Srv: ReplyMsg(OK)
    Srv->>K: ReceiveMsg(chanid) [blocks in RECEIVE_BLOCKED]

    Cli->>NS: SendMsg(NAME_MSG_QUERY, "/dev/vfs")
    NS-->>Cli: ReplyMsg(srv_pid, chanid)
    Cli->>K: OpenConnection(srv_pid, chanid)
    K-->>Cli: coid

    Cli->>K: SendMsg(coid, req, rep_buf)
    Note over K: Client enters REPLY_BLOCKED, Server gets PI boost
    K-->>Srv: Wake ReceiveMsg -> msgid + req payload
    opt Large scatter-gather transfer
        Srv->>K: ReadMsgV(msgid, iov) / WriteMsgV(msgid, iov)
    end
    Srv->>K: ReplyMsg(msgid, rep)
    Note over K: Copy reply via HHDM and restore Server priority
    K-->>Cli: Unblock SendMsg with reply bytes
```

## Hardware IRQ-to-Pulse Delivery for Ring 3 Drivers

Device drivers run in Ring 3 without moving device logic into the kernel. The
kernel routes IOAPIC hardware interrupts into non-blocking 8-byte pulses
(`PULSE_CODE_IRQ`) for processes holding `CAP_SYS_INTR`.

```mermaid
sequenceDiagram
    participant HW as Hardware (COM1 IRQ 4 / RTC IRQ 8)
    participant K as Kernel IOAPIC Router (Ring 0)
    participant Drv as Ring 3 Driver (ttyserv / rtcdrv)

    Drv->>K: IOPLSet(3) [checked against CAP_SYS_INTR]
    Drv->>K: AttachIntrEvent(irq, chanid, PULSE_CODE_IRQ)
    Drv->>K: IntrUnmask(irq)
    Drv->>K: ReceiveMsg(chanid)

    HW->>K: Assert Hardware IRQ
    K->>K: Mask IOAPIC pin, enqueue pre-allocated pulse
    K-->>Drv: ReceiveMsg returns msgid == 0 (pulse)
    Drv->>HW: Read/write device registers via Ring 3 port I/O
    Drv->>K: IntrUnmask(irq) [re-arm for next interrupt]
```

## Multi-Server Descriptor Routing and `ppoll()`

The kernel stores only a small per-process descriptor table (`FD_KIND_*` plus
`(srv_pid, srv_chanid, srv_handle)`). `libc` routes descriptor calls to the
right Ring 3 server and coordinates multi-server `ppoll()`. The kernel sends
`PULSE_CODE_FD_CLOSE` automatically on `O_CLOEXEC` `execve` or process exit.

```mermaid
flowchart TD
    APP["User Program (open, read, write, accept4, ppoll, select, close)"]
    LIBC["libc Descriptor Router (fd.c, vfs.c, tty.c, socket.c, select.c)"]
    KFD["Kernel SYS_FD_OP Table (FD_KIND_*)"]

    subgraph SRVS["Ring 3 Descriptor Servers & Kernel Backends"]
        direction TB
        PIPE["pipeserv (/dev/pipe) - FD_KIND_SERVER"]
        VFS["vfsserv (/dev/vfs) - FD_KIND_VFS"]
        TTY["ttyserv (/dev/tty) - FD_KIND_TTY"]
        SOCK["sockserv (/dev/sock) - FD_KIND_SOCK"]
        SHM["Kernel SHM / Console - FD_KIND_SHM / CONSOLE"]
    end

    APP --> LIBC
    LIBC -->|"Lookup / Alloc FD"| KFD
    LIBC -->|"SendMsg RPC"| PIPE
    LIBC -->|"SendMsg RPC"| VFS
    LIBC -->|"SendMsg RPC"| TTY
    LIBC -->|"SendMsg RPC"| SOCK
    LIBC -->|"Direct Syscall"| SHM
    KFD -.->|"PULSE_CODE_FD_CLOSE"| PIPE
    KFD -.->|"PULSE_CODE_FD_CLOSE"| VFS
    KFD -.->|"PULSE_CODE_FD_CLOSE"| SOCK
```
