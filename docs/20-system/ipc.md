# System and Software Requirements — IPC

Domain `IPC`: Synchronous message-passing channels, client connections, priority
inheritance, pulses, user-space name resolution, and POSIX.1-2024 descriptor IPC
and asynchronous I/O.

```{sys} Provide synchronous IPC and service discovery
:id: SYS_IPC_0001
:status: active

The system shall provide synchronous channel-based message passing, asynchronous
pulses, and user-space service name discovery between Ring 3 processes.
```

```{swr} Manage IPC channels and client connections
:id: SWR_IPC_0001
:status: active
:derives: SYS_IPC_0001

The kernel shall create and destroy per-process receive channels and attach or
detach client connections targeting a `(pid, chanid)` endpoint
({need}`SYS_IPC_0001`).
```

```{swr} Transfer synchronous messages with priority inheritance and pulses
:id: SWR_IPC_0002
:status: active
:derives: SYS_IPC_0001

The IPC subsystem shall block callers of `SendMsg` until `ReceiveMsg` and
`ReplyMsg` complete, boost receiver effective priority while servicing a
message, and deliver non-blocking 8-byte pulses via `SendPulse`
({need}`SYS_IPC_0001`).
```

```{swr} Resolve named service endpoints in sysproc
:id: SWR_IPC_0003
:status: active
:derives: SYS_IPC_0001

The `sysproc` name registry shall accept `NAME_MSG_ATTACH` registrations from
servers and resolve `NAME_MSG_QUERY` lookups to `(pid, chanid)` pairs
({need}`SYS_IPC_0001`).
```

```{swr} Provide POSIX.1-2024 Base descriptor IPC and ppoll over Ring 3 servers
:id: SWR_IPC_0004
:status: draft
:posix: XSH/pipe2 (BASE), XSH/dup3 (BASE), XSH/ppoll (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003

Ring 3 system servers communicating over kernel channels shall back mandatory
Issue 8 Base interfaces `pipe2` ([pipe2][fn-pipe2]), `dup3`, and `ppoll`
([ppoll][fn-ppoll], [poll.h][hdr-poll]), enforcing atomic `O_CLOEXEC` creation
and signal-mask restoration ({need}`SYS_IPC_0001`).
```

```{swr} Provide POSIX.1-2024 Base AIO and gate optional message queues
:id: SWR_IPC_0005
:status: draft
:posix: XBD/aio.h (BASE), XBD/mqueue.h (MSG)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003

`<aio.h>` ([aio.h][hdr-aio]) shall expose mandatory Issue 8 Base asynchronous
I/O (`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_fsync`, `aio_suspend`, `lio_listio`) backed by Ring 3 IPC servers, while
`<mqueue.h>` ([mqueue.h][hdr-mq], `MSG`) stays gated behind
`_POSIX_MESSAGE_PASSING` ({need}`SYS_IPC_0001`).
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
