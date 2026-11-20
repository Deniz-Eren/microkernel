# Interfaces — IPC

Domain `IPC`: Synchronous message-passing and name-service protocol contracts.

```{icd} Channel IPC and name-service protocol
:id: ICD_IPC_0001
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0003

`libc/include/sys/ipc.h` defines `CreateChannel`, `DestroyChannel`,
`OpenConnection`, `CloseConnection`, `SendMsg`, `ReceiveMsg`, `ReplyMsg`,
`SendPulse`, `ipc_recv_info_t`, `ipc_pulse_t`, and the `sysproc`
`NAME_MSG_ATTACH`/`NAME_MSG_QUERY` protocol for the IPC subsystem
({need}`ARC_IPC_0001`), satisfying channel management ({need}`SWR_IPC_0001`),
message transfer ({need}`SWR_IPC_0002`), and name lookup ({need}`SWR_IPC_0003`).
```
