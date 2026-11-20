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
