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

`servers/rtcdrv/arch/x86-64/cmos_rtc.c` runs in Ring 3 with `IOPLSet(3)`,
handles hardware IRQ 8 pulses through `AttachIntrEvent`/`IntrUnmask`, registers
`/dev/rtc`, and serves clients with `ReadMsgV` and `WriteMsgV`
({need}`ICD_IPC_0001`, {need}`ICD_IPC_0002`, {need}`SWR_IPC_0006`).
```

```{comp} Ring 3 pipe and ppoll server
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

```{comp} Ring 3 serial TTY server and termios line discipline
:id: COMP_IPC_0010
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0010
:satisfies: SWR_IPC_0008

`servers/ttyserv/main.c` and `servers/ttyserv/arch/x86-64/uart_16550.c` run with
`CAP_SYS_INTR` and COM1 IRQ 4 pulses and register `/dev/tty`. They implement
canonical and raw `<termios.h>` line discipline and serve deferred blocking
`read` and `ppoll` requests ({need}`ICD_IPC_0010`, {need}`SWR_IPC_0008`).
```

```{comp} POSIX.1-2024 libc termios, isatty, and /dev/tty client wrappers
:id: COMP_IPC_0011
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0010, ICD_IPC_0011
:satisfies: SWR_IPC_0008

`libc/src/tty.c` implements async-signal-safe `tcgetattr`, `tcsetattr`,
`tcflush`, `tcdrain`, `tcflow`, `tcsendbreak`, `cf*speed`, `isatty`, and
`/dev/tty` `read`/`ppoll` helpers ({need}`ICD_IPC_0010`, {need}`ICD_IPC_0011`,
{need}`SWR_IPC_0008`).
```

```{comp} Ring 3 AF_UNIX local sockets server
:id: COMP_IPC_0012
:status: active
:posix: XBD/sys/socket.h (BASE), XSH/accept4 (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0012
:satisfies: SWR_IPC_0009

`servers/sockserv/main.c` registers `/dev/sock` in Ring 3 and serves `AF_UNIX`
`SOCK_STREAM` and `SOCK_DGRAM` sockets. It manages 4 KiB receive rings, accept
queues, deferred blocking `accept`/`send`/`recv`/`ppoll` requests, and
`PULSE_CODE_FD_CLOSE` cleanup ({need}`ICD_IPC_0012`, {need}`SWR_IPC_0009`).
```

```{comp} POSIX.1-2024 libc AF_UNIX socket and accept4 wrappers
:id: COMP_IPC_0013
:status: active
:posix: XBD/sys/socket.h (BASE), XBD/sys/un.h (BASE), XSH/accept4 (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0012, ICD_IPC_0013
:satisfies: SWR_IPC_0009

`libc/src/socket.c` implements async-signal-safe `socket`, `socketpair`, `bind`,
`listen`, `accept`, `accept4`, `connect`, `send`, `recv`, `sendto`, and
`recvfrom`. It also implements `shutdown`, `getsockname`, `getpeername`,
`getsockopt`, and `setsockopt` over `/dev/sock` IPC with cancellation checks
({need}`ICD_IPC_0012`, {need}`ICD_IPC_0013`, {need}`SWR_IPC_0009`).
```

```{comp} POSIX.1-2024 libc select and pselect wrappers over multi-server ppoll
:id: COMP_IPC_0014
:status: active
:posix: XBD/sys/select.h (BASE), XSH/pselect (BASE), XSH/select (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0005, ICD_IPC_0014
:satisfies: SWR_DOCS_0004

`libc/src/select.c` implements `FD_CLR`, `FD_ISSET`, `FD_SET`, `FD_ZERO`,
`select`, and `pselect` over multi-server `ppoll()` with cancellation point
checks and `POLLNVAL` to `EBADF` mapping ({need}`ICD_IPC_0005`,
{need}`ICD_IPC_0014`, {need}`SWR_DOCS_0004`).
```

```{comp} CAP_SYS_INTR MMIO/DMA/PCI syscalls and Ring 3 virtio-blk driver
:id: COMP_IPC_0015
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0015
:satisfies: SWR_IPC_0010

`kernel/core/vmm.c` and `kernel/core/intr.c` implement `CAP_SYS_INTR`
`SYS_VM_MAP_MMIO`, `SYS_DMA_ALLOC`, and `SYS_PCI_CFG` syscalls.
`servers/blkdrv/main.c` runs a Ring 3 Modern VirtIO 1.0 PCI block driver
(`/dev/blk`) over MMIO BARs, contiguous DMA virtqueues, and IOAPIC IRQ pulses
({need}`ICD_IPC_0002`, {need}`ICD_IPC_0015`, {need}`SWR_IPC_0010`).
```

```{comp} Writable ext2 on-disk filesystem mounted in vfsserv over /dev/blk
:id: COMP_IPC_0016
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0007, ICD_IPC_0015
:satisfies: SWR_IPC_0011

`servers/vfsserv/ext2.c` and `servers/vfsserv/main.c` format and mount a rev-1
`ext2` filesystem on `/mnt` over `/dev/blk`. They support direct and
single-indirect data blocks, write-through `open`/`read`/`write`/`ftruncate`,
`mkdir`, `unlink`, `rmdir`, `rename`, `EXDEV`, and open-unlinked inode cleanup
({need}`ICD_IPC_0007`, {need}`ICD_IPC_0015`, {need}`SWR_IPC_0011`).
```

```{comp} VFS links, permissions, FIFOs, positional I/O, statvfs, and XCU tools
:id: COMP_IPC_0017
:status: active
:posix: XBD/sys/statvfs.h (BASE), XSH/posix_getdents (BASE), XSH/mkfifo (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0016
:satisfies: SWR_IPC_0012

`servers/vfsserv/main.c`, `libc/src/vfs.c`, and `libc/src/vfs_helpers.c`
implement permissions, ownership, symlinks, hard links, blocking FIFOs,
positional I/O, `posix_getdents`, `scandir`, `statvfs`, and temp-file helpers.
`bin/utils/` supplies `chmod`, `chown`, `chgrp`, `ln`, `mkfifo`, `readlink`,
`realpath`, `touch`, `rmdir`, `df`, `du`, `dd`, `basename`, `dirname`, and
`pathchk` ({need}`ICD_IPC_0016`, {need}`SWR_IPC_0012`).
```

```{comp} Ring 3 PCI virtio-net driver and TCP/IP/loopback server (/dev/net)
:id: COMP_IPC_0018
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0017
:satisfies: SWR_IPC_0013

`servers/netserv/virtio_net.c` drives Modern VirtIO 1.0 PCI `virtio-net` over
MMIO BARs, contiguous DMA RX/TX virtqueues, and IOAPIC IRQ pulses.
`servers/netserv/net_proto.c` and `servers/netserv/main.c` supply `/dev/net`
with loopback, Ethernet, ARP, IPv4, IPv6, ICMPv4, ICMPv6, UDP, TCP, and poll
support ({need}`ICD_IPC_0002`, {need}`ICD_IPC_0017`, {need}`SWR_IPC_0013`).
```

```{comp} Ring 3 netcat (nc), ping CLI, and host-to-guest networking support
:id: COMP_IPC_0019
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0017
:satisfies: SWR_IPC_0014

`bin/utils/nc.c` and `bin/utils/ping.c` implement TCP/UDP client and listener
relay (`-l`, `-k`, `-e`, `-u`) and ICMPv4/ICMPv6 ping over `/dev/net`.
`servers/netserv/` and `scripts/env.sh` provide localhost host-to-guest TCP
forwarding and hardened wire RX/TX handling ({need}`ICD_IPC_0017`,
{need}`SWR_IPC_0014`).
```
