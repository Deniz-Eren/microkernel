# System and Software Requirements — IPC

Domain `IPC`: synchronous message passing channels, connections, priority
inheritance, pulses, name registry, and POSIX.1-2024 descriptor IPC and AIO.

```{sys} Supply synchronous IPC and service lookup
:id: SYS_IPC_0001
:status: active

The system must supply synchronous channel message passing, asynchronous pulses,
and user-space service lookup between Ring 3 processes.
```

```{swr} Manage IPC channels and client connections
:id: SWR_IPC_0001
:status: active
:derives: SYS_IPC_0001

The kernel must make and remove process receive channels. It must attach and
detach client connections to a `(pid, chanid)` endpoint ({need}`SYS_IPC_0001`).
```

```{swr} Move synchronous messages with priority inheritance and pulses
:id: SWR_IPC_0002
:status: active
:derives: SYS_IPC_0001

The IPC subsystem must block `SendMsg` callers until `ReceiveMsg` and `ReplyMsg`
complete. It must boost receiver priority during service and deliver 8-byte
pulses with `SendPulse` ({need}`SYS_IPC_0001`).
```

```{swr} Resolve named service endpoints in sysproc
:id: SWR_IPC_0003
:status: active
:derives: SYS_IPC_0001

The `sysproc` name registry must accept `NAME_MSG_ATTACH` messages from servers.
It must resolve `NAME_MSG_QUERY` lookups to `(pid, chanid)` pairs
({need}`SYS_IPC_0001`).
```

```{swr} Supply POSIX.1-2024 Base descriptor IPC and ppoll
:id: SWR_IPC_0004
:status: active
:posix: XSH/pipe2 (BASE), XSH/dup3 (BASE), XSH/ppoll (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0011

Ring 3 system servers through kernel channels must supply mandatory Issue 8 Base
functions `pipe2` ([pipe2][fn-pipe2]), `dup3`, and `ppoll` ([ppoll][fn-ppoll],
[poll.h][hdr-poll]). They must obey atomic `O_CLOEXEC` and signal mask rules
({need}`SYS_IPC_0001`).
```

```{swr} Supply POSIX.1-2024 Base AIO and option-gated message queues
:id: SWR_IPC_0005
:status: active
:posix: XBD/aio.h (BASE), XBD/mqueue.h (MSG)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0012

`<aio.h>` ([aio.h][hdr-aio]) must declare mandatory Issue 8 Base AIO functions
(`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`, `aio_fsync`,
`aio_suspend`, `lio_listio`) through Ring 3 servers. `<mqueue.h>`
([mqueue.h][hdr-mq], `MSG`) must stay behind `_POSIX_MESSAGE_PASSING`
({need}`SYS_IPC_0001`).
```

```{swr} Deliver IRQ pulses, grant Ring 3 IOPL, and run scatter-gather IPC
:id: SWR_IPC_0006
:status: active
:derives: SYS_IPC_0001

The kernel must route hardware IRQs through the IOAPIC into IPC pulses with
`AttachIntrEvent` and `IntrUnmask`. It must guard `IOPLSet` with `CAP_SYS_INTR`
and move multi-buffer payloads with `ReadMsgV` and `WriteMsgV`
({need}`SYS_IPC_0001`).
```

[fn-pipe2]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/pipe2.html
[fn-ppoll]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/ppoll.html
[hdr-poll]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/poll.h.html
[hdr-aio]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/aio.h.html
[hdr-mq]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/mqueue.h.html
