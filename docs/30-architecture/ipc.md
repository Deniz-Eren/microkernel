# Architecture — IPC

Domain `IPC`: Architecture items for synchronous IPC messages and service
lookup.

```{arc} Synchronous channel IPC and name service subsystem
:id: ARC_IPC_0001
:status: active
:satisfies: SYS_IPC_0001
:traces: ADR_0002

The IPC subsystem connects kernel receive channels, client connections, and
priority-inheritance message queues with the Ring 3 `sysproc` name registry
({need}`SYS_IPC_0001`).
```

## Service Registry and Synchronous IPC Messages

Servers record endpoints at `/dev/*` in `sysproc` (PID 1, channel 1). Clients
find a `(pid, chanid)` pair from a service name, open a connection, and send
synchronous IPC messages through Ring 0.

```mermaid
sequenceDiagram
    participant Srv as Ring 3 Server (for example vfsserv)
    participant NS as sysproc (PID 1, Channel 1)
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
    Note over K: Client enters REPLY_BLOCKED and Server gets PI boost
    K-->>Srv: Wake ReceiveMsg with msgid and payload
    opt Large scatter-gather transfer
        Srv->>K: ReadMsgV(msgid, iov) / WriteMsgV(msgid, iov)
    end
    Srv->>K: ReplyMsg(msgid, rep)
    Note over K: Write reply through HHDM and set back Server priority
    K-->>Cli: Wake SendMsg with reply bytes
```

## Hardware IRQ Pulses for Ring 3 Drivers

Device drivers operate in Ring 3 and do not put device logic in the kernel. The
kernel sends IOAPIC hardware interrupts as asynchronous 8-byte pulses
(`PULSE_CODE_IRQ`) to processes that have `CAP_SYS_INTR`.

```mermaid
sequenceDiagram
    participant HW as Hardware (COM1 IRQ 4 / RTC IRQ 8)
    participant K as Kernel IOAPIC Layer (Ring 0)
    participant Drv as Ring 3 Driver (ttyserv / rtcdrv)

    Drv->>K: IOPLSet(3) [CAP_SYS_INTR check]
    Drv->>K: AttachIntrEvent(irq, chanid, PULSE_CODE_IRQ)
    Drv->>K: IntrUnmask(irq)
    Drv->>K: ReceiveMsg(chanid)

    HW->>K: Send Hardware IRQ
    K->>K: Block IOAPIC pin and queue static pulse
    K-->>Drv: ReceiveMsg gives msgid == 0 (pulse)
    Drv->>HW: Read or write device registers with Ring 3 port I/O
    Drv->>K: IntrUnmask(irq) [prepare for next interrupt]
```

## Multi-Server Descriptor Selection and `ppoll()`

The kernel keeps only a small descriptor table for each process (`FD_KIND_*` and
`(srv_pid, srv_chanid, srv_handle)`). `libc` sends descriptor calls to the
correct Ring 3 server and controls multi-server `ppoll()`. The kernel
automatically sends `PULSE_CODE_FD_CLOSE` on `O_CLOEXEC` `execve` or process
exit.

```mermaid
flowchart TD
    APP["User Executable (open, read, write, accept4, ppoll, select, close)"]
    LIBC["libc Descriptor Layer (fd.c, vfs.c, tty.c, socket.c, select.c)"]
    KFD["Kernel SYS_FD_OP Table (FD_KIND_*)"]

    subgraph SRVS["Ring 3 Descriptor Servers and Kernel Services"]
        direction TB
        PIPE["pipeserv (/dev/pipe) - FD_KIND_SERVER"]
        VFS["vfsserv (/dev/vfs) - FD_KIND_VFS"]
        TTY["ttyserv (/dev/tty) - FD_KIND_TTY"]
        SOCK["sockserv (/dev/sock) - FD_KIND_SOCK"]
        SHM["Kernel SHM and Console - FD_KIND_SHM / CONSOLE"]
    end

    APP --> LIBC
    LIBC -->|"Find or allocate FD"| KFD
    LIBC -->|"SendMsg IPC"| PIPE
    LIBC -->|"SendMsg IPC"| VFS
    LIBC -->|"SendMsg IPC"| TTY
    LIBC -->|"SendMsg IPC"| SOCK
    LIBC -->|"Kernel Syscall"| SHM
    KFD -.->|"PULSE_CODE_FD_CLOSE"| PIPE
    KFD -.->|"PULSE_CODE_FD_CLOSE"| VFS
    KFD -.->|"PULSE_CODE_FD_CLOSE"| SOCK
```
