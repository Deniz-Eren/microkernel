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

```{comp} Ring 3 POSIX pipe and ppoll server
:id: COMP_IPC_0005
:status: active
:posix: XSH/pipe2 (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0005
:satisfies: SWR_IPC_0004

`servers/pipeserv/main.c` registers `/dev/pipe` in Ring 3, keeps 4 KiB pipe
buffers, serves deferred blocking `read`/`write`/`ppoll` requests, and processes
`PULSE_CODE_FD_CLOSE` close notifications ({need}`ICD_IPC_0003`,
{need}`SWR_IPC_0004`).
```

```{comp} Async-signal-safe libc descriptor, pipe2, dup3, and ppoll wrappers
:id: COMP_IPC_0006
:status: active
:posix: XSH/pipe2 (BASE), XSH/dup3 (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0003, ICD_IPC_0004, ICD_IPC_0005
:satisfies: SWR_IPC_0004

`libc/src/fd.c` builds `pipe`, `pipe2`, `dup`, `dup2`, `dup3`, `fcntl`, `close`,
`poll`, and `ppoll` over `SYS_FD_OP` and `/dev/pipe` IPC with atomic `O_CLOEXEC`
and signal mask rules ({need}`ICD_IPC_0004`, {need}`ICD_IPC_0005`,
{need}`SWR_IPC_0004`).
```

```{comp} Ring 3 POSIX.1-2024 asynchronous I/O worker engine
:id: COMP_IPC_0007
:status: active
:posix: XBD/aio.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0006
:satisfies: SWR_IPC_0005

`libc/src/aio.c` runs `aio_read`, `aio_write`, `aio_fsync`, `aio_cancel`, and
`lio_listio` on a Ring 3 worker thread over `SYS_FUTEX` and descriptor IPC. It
keeps `aio_error`, `aio_return`, and `aio_suspend` async-signal-safe
({need}`ICD_IPC_0006`, {need}`SWR_IPC_0005`).
```

```{comp} Ring 3 VFS server for bootfs and tmpfs
:id: COMP_IPC_0008
:status: active
:posix: XSH/open (BASE), XBD/sys/stat.h (BASE), XBD/dirent.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0007
:satisfies: SWR_IPC_0007

`servers/vfsserv/main.c` registers `/dev/vfs`, serves read-only `boot.tar`
entries at `/boot`, and manages writable `tmpfs` files, directories, open file
offsets, and `PULSE_CODE_FD_CLOSE` handle cleanup ({need}`ICD_IPC_0007`,
{need}`SWR_IPC_0007`).
```

```{comp} POSIX.1-2024 libc VFS, stat, dirent, and cwd wrappers
:id: COMP_IPC_0009
:status: active
:posix: XSH/open (BASE), XBD/sys/stat.h (BASE), XBD/dirent.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0004, ICD_IPC_0007, ICD_IPC_0008, ICD_IPC_0009
:satisfies: SWR_IPC_0007

`libc/src/vfs.c` implements `open`, `openat`, `creat`, `lseek`, `ftruncate`,
`stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `unlink`, `rmdir`, `chdir`,
`fchdir`, `getcwd`, `umask`, `access`, and `<dirent.h>` over `/dev/vfs` IPC
({need}`ICD_IPC_0007`, {need}`ICD_IPC_0008`, {need}`ICD_IPC_0009`,
{need}`SWR_IPC_0007`).
```
