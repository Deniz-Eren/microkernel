# Interfaces — IPC

Domain `IPC`: synchronous message passing, scatter-gather IPC, interrupt events,
and name-service contracts.

```{icd} Channel IPC, scatter-gather MsgV, and name-service protocol
:id: ICD_IPC_0001
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0003, SWR_IPC_0006

`libc/include/sys/ipc.h` gives `CreateChannel`, `DestroyChannel`,
`OpenConnection`, `CloseConnection`, `SendMsg`, `ReceiveMsg`, `ReplyMsg`,
`ReadMsgV`, `WriteMsgV`, `SendPulse`, `ipc_iov_t`, and the `sysproc` name
protocol ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0001`, {need}`SWR_IPC_0002`,
{need}`SWR_IPC_0003`, {need}`SWR_IPC_0006`).
```

```{icd} Interrupt event, IOPL, and Ring 3 port I/O interface
:id: ICD_IPC_0002
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0006

`libc/include/sys/intr.h` and `kernel/include/kernel/intr.h` specify
`CAP_SYS_INTR`, `PULSE_CODE_IRQ`, `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`,
and Ring 3 port I/O helpers ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0006`).
```
