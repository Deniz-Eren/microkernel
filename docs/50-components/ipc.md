# Components — IPC

Domain `IPC`: white-box specification for kernel message queues, IOAPIC IRQ
dispatch, and Ring 3 servers.

```{comp} Kernel channel queues, MsgV engine, and IRQ pulse delivery
:id: COMP_IPC_0001
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0006

`kernel/core/channel.c` and `kernel/core/ipc.c` manage channels, connection
tables, priority-ordered queues, priority inheritance, `ipc_msg_readv`,
`ipc_msg_writev`, and static IRQ pulse delivery ({need}`ICD_IPC_0001`,
{need}`ARC_IPC_0001`).
```

```{comp} User-space sysproc name registry and ipcserv echo server
:id: COMP_IPC_0002
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0003

`servers/sysproc/main.c` serves `NAME_MSG_ATTACH` and `NAME_MSG_QUERY` on PID 1
channel 1. `servers/ipcserv/main.c` registers `/dev/echo` and serves concurrent
requests ({need}`ICD_IPC_0001`, {need}`ARC_IPC_0001`).
```

```{comp} IOAPIC IRQ router and Ring 3 IOPL capability gate
:id: COMP_IPC_0003
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0002
:satisfies: SWR_IPC_0006

`kernel/core/intr.c` maps the IOAPIC, guards `AttachIntrEvent`, `IntrUnmask`,
and `IOPLSet` with `CAP_SYS_INTR`, masks active IRQ pins until unmask, and sends
pre-allocated pulses ({need}`ICD_IPC_0002`, {need}`SWR_IPC_0006`).
```

```{comp} User-space MC146818 CMOS RTC driver
:id: COMP_IPC_0004
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002
:satisfies: SWR_IPC_0006

`servers/rtcdrv/main.c` runs in Ring 3 with `IOPLSet(3)`, handles hardware IRQ 8
pulses through `AttachIntrEvent`/`IntrUnmask`, registers `/dev/rtc`, and serves
clients with `ReadMsgV` and `WriteMsgV` ({need}`ICD_IPC_0001`,
{need}`ICD_IPC_0002`, {need}`SWR_IPC_0006`).
```
