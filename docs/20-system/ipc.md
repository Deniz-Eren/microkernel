# System and Software Requirements — IPC

Domain `IPC`: Synchronous message-passing channels, client connections, priority
inheritance, pulses, and user-space name resolution.

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
