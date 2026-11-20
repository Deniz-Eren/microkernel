# Components — IPC

Domain `IPC`: White-box specifications for kernel message queues and user
servers.

```{comp} Kernel channel queues and message staging engine
:id: COMP_IPC_0001
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0001, SWR_IPC_0002

`kernel/core/channel.c` and `kernel/core/ipc.c` manage per-process channels,
connection tables, priority-ordered message/receiver/pulse queues, priority
inheritance, and HHDM-staged payload copies ({need}`ICD_IPC_0001`) for the IPC
subsystem ({need}`ARC_IPC_0001`).
```

```{comp} User-space sysproc name registry and ipcserv echo server
:id: COMP_IPC_0002
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0003

`servers/sysproc/main.c` serves `NAME_MSG_ATTACH` and `NAME_MSG_QUERY` requests
on PID 1 channel 1, and `servers/ipcserv/main.c` registers `/dev/echo` and
serves concurrent requests ({need}`ICD_IPC_0001`) for the IPC subsystem
({need}`ARC_IPC_0001`).
```
