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

```{icd} Kernel FD table ABI and Ring 3 /dev/pipe protocol
:id: ICD_IPC_0003
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/sys/fd.h` defines `SYS_FD_OP` descriptor operations,
`PULSE_CODE_FD_CLOSE`, and the `/dev/pipe` Ring 3 IPC message structures
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <fcntl.h> public interface
:id: ICD_IPC_0004
:status: active
:posix: XBD/fcntl.h (BASE), XSH/fcntl (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/fcntl.h` declares `O_CLOEXEC`, `O_NONBLOCK`, `FD_CLOEXEC`,
`F_DUPFD`, `F_DUPFD_CLOEXEC`, `F_GETFD`, `F_SETFD`, `F_GETFL`, `F_SETFL`, and
`fcntl` with no `<sys/microkernel.h>` leakage ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <poll.h> public interface
:id: ICD_IPC_0005
:status: active
:posix: XBD/poll.h (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/poll.h` declares `nfds_t`, `struct pollfd`, `POLLIN`, `POLLOUT`,
`POLLHUP`, `POLLERR`, `POLLNVAL`, `poll`, and `ppoll` with no
`<sys/microkernel.h>` leakage ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <aio.h> public interface
:id: ICD_IPC_0006
:status: active
:posix: XBD/aio.h (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0005

`libc/include/aio.h` declares `struct aiocb`, `AIO_*`, `LIO_*`, `aio_read`,
`aio_write`, `aio_fsync`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_suspend`, and `lio_listio` with no `<sys/microkernel.h>` leakage
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0005`).
```
