# Interfaces — IPC

The `IPC` domain includes synchronous message passing, scatter-gather IPC,
interrupt events, and name-service interfaces.

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

```{icd} Interrupt event, IOPL, MMIO, DMA, PCI, and Ring 3 port I/O interface
:id: ICD_IPC_0002
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0006, SWR_IPC_0010

`libc/include/sys/intr.h` and `kernel/include/kernel/intr.h` give
`CAP_SYS_INTR`, `PULSE_CODE_IRQ`, `AttachIntrEvent`, `IntrUnmask`, `IOPLSet`,
`VMMapMMIO`, `DMAAlloc`, `PCIConfigRead32`/`PCIConfigWrite32`, and Ring 3 port
I/O helpers ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0006`, {need}`SWR_IPC_0010`).
```

```{icd} Kernel FD table ABI and Ring 3 /dev/pipe protocol
:id: ICD_IPC_0003
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/sys/fd.h` gives `SYS_FD_OP` descriptor operations,
`PULSE_CODE_FD_CLOSE`, and the `/dev/pipe` Ring 3 IPC message structures
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <fcntl.h> public interface
:id: ICD_IPC_0004
:status: active
:posix: XBD/fcntl.h (BASE), XSH/fcntl (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/fcntl.h` gives `O_CLOEXEC`, `O_NONBLOCK`, `FD_CLOEXEC`, `F_DUPFD`,
`F_DUPFD_CLOEXEC`, `F_GETFD`, `F_SETFD`, `F_GETFL`, `F_SETFL`, and `fcntl`. It
does not include `<sys/microkernel.h>` ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <poll.h> public interface
:id: ICD_IPC_0005
:status: active
:posix: XBD/poll.h (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0004

`libc/include/poll.h` gives `nfds_t`, `struct pollfd`, `POLLIN`, `POLLOUT`,
`POLLHUP`, `POLLERR`, `POLLNVAL`, `poll`, and `ppoll`. It does not include
`<sys/microkernel.h>` ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0004`).
```

```{icd} POSIX.1-2024 <aio.h> public interface
:id: ICD_IPC_0006
:status: active
:posix: XBD/aio.h (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0005

`libc/include/aio.h` gives `struct aiocb`, `AIO_*`, `LIO_*`, `aio_read`,
`aio_write`, `aio_fsync`, `aio_error`, `aio_return`, `aio_cancel`,
`aio_suspend`, and `lio_listio`. It does not include `<sys/microkernel.h>`
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0005`).
```

```{icd} Ring 3 /dev/vfs IPC protocol
:id: ICD_IPC_0007
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0007

`libc/include/sys/vfs.h` gives `VFS_OP_*`, `vfs_stat_t`, `vfs_req_t`, and
`vfs_rep_t` message structures in `IPC_MSG_MAX` for Ring 3 `/dev/vfs` IPC
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0007`).
```

```{icd} POSIX.1-2024 <sys/stat.h> public interface
:id: ICD_IPC_0008
:status: active
:posix: XBD/sys/stat.h (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0007

`libc/include/sys/stat.h` gives `struct stat`, `S_IF*`, `S_IS*`, permission
bits, `stat`, `fstat`, `lstat`, `fstatat`, `mkdir`, `mkdirat`, and `umask`. It
does not include `<sys/microkernel.h>` ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0007`).
```

```{icd} POSIX.1-2024 <dirent.h> public interface
:id: ICD_IPC_0009
:status: active
:posix: XBD/dirent.h (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0007

`libc/include/dirent.h` gives `DIR`, `struct dirent`, `opendir`, `fdopendir`,
`readdir`, `rewinddir`, `closedir`, and `dirfd`. It does not include
`<sys/microkernel.h>` ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0007`).
```

```{icd} Ring 3 /dev/tty IPC protocol
:id: ICD_IPC_0010
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0008

`libc/include/sys/tty.h` gives `TTY_OP_*`, `tty_req_t`, and `tty_rep_t` message
structures in `IPC_MSG_MAX` for Ring 3 `/dev/tty` IPC ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0008`).
```

```{icd} POSIX.1-2024 <termios.h> public interface
:id: ICD_IPC_0011
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0008

`libc/include/termios.h` gives `struct termios`, `c_cc[]` indices, mode and baud
macros, `tcgetattr`, `tcsetattr`, `tcflush`, `tcdrain`, `tcflow`, `tcsendbreak`,
and `cf*speed`. It does not include `<sys/microkernel.h>` ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0008`).
```

```{icd} Ring 3 /dev/sock IPC protocol
:id: ICD_IPC_0012
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0009

`libc/include/sys/sock.h` gives `SOCK_OP_*`, `sock_req_t`, `sock_rep_t`, and
`sock_poll_*` structures in `IPC_MSG_MAX` for Ring 3 `/dev/sock` IPC
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0009`).
```

```{icd} POSIX.1-2024 <sys/socket.h> and <sys/un.h> public interfaces
:id: ICD_IPC_0013
:status: active
:posix: XBD/sys/socket.h (BASE), XBD/sys/un.h (BASE), XSH/accept4 (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0009

`libc/include/sys/socket.h` and `libc/include/sys/un.h` give `socklen_t`,
`sa_family_t`, `struct sockaddr`, `struct sockaddr_un`, `SOCK_CLOEXEC`, and
`SOCK_NONBLOCK`. They give `socket`, `socketpair`, `bind`, `listen`, `accept`,
`accept4`, `connect`, `send`, `recv`, `sendto`, `recvfrom`, `shutdown`, and
option helpers, and do not include `<sys/microkernel.h>` ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0009`).
```

```{icd} POSIX.1-2024 <sys/select.h> public interface
:id: ICD_IPC_0014
:status: active
:posix: XBD/sys/select.h (BASE), XSH/pselect (BASE), XSH/select (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_DOCS_0004

`libc/include/sys/select.h` gives `struct timeval`, `fd_set`, `FD_SETSIZE`,
`FD_CLR`, `FD_ISSET`, `FD_SET`, `FD_ZERO`, `select`, and `pselect`. It does not
include `<sys/microkernel.h>` ({need}`ARC_IPC_0001`, {need}`SWR_DOCS_0004`).
```

```{icd} Ring 3 block device IPC protocol (/dev/blk)
:id: ICD_IPC_0015
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0010

`libc/include/sys/blk.h` gives `BLK_SECTOR_SIZE` (`512`), `BLK_MAX_SECTORS`,
`BLK_OP_INFO`, `BLK_OP_READ`, `BLK_OP_WRITE`, `blk_req_t`, and `blk_rep_t` for
Ring 3 `/dev/blk` IPC ({need}`ARC_IPC_0001`, {need}`SWR_IPC_0010`).
```

```{icd} POSIX.1-2024 Base VFS metadata, link, FIFO, dirent, and statvfs headers
:id: ICD_IPC_0016
:status: active
:posix: XBD/sys/statvfs.h (BASE), XSH/posix_getdents (BASE), XSH/mkfifo (BASE)
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0012

`libc/include/sys/vfs.h` gives `VFS_OP_CHMOD` through `VFS_OP_STATVFS` and
`vfs_statvfs_t`. `libc/include/sys/statvfs.h`, `sys/stat.h`, `unistd.h`,
`dirent.h`, `stdio.h`, and `stdlib.h` give POSIX.1-2024 Base permissions, links,
FIFOs, offset I/O, `posix_getdents`, `statvfs`, and temporary file APIs
({need}`ARC_IPC_0001`, {need}`SWR_IPC_0012`).
```

```{icd} Ring 3 network stack and virtio-net IPC protocol (/dev/net)
:id: ICD_IPC_0017
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0013

`libc/include/sys/net.h` gives `NET_OP_INFO`, `NET_OP_ARP_RESOLVE`,
`NET_OP_PING`, `NET_OP_SOCK_*`, `NET_OP_STATS`, `net_addr_t`, `net_info_t`,
`net_req_t`, and `net_rep_t` for Ring 3 `/dev/net` IPC ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0013`).
```

```{icd} Terminal and descriptor ioctl interface (<sys/ioctl.h>)
:id: ICD_IPC_0018
:status: active
:refines: ARC_IPC_0001
:satisfies: SWR_IPC_0015

`libc/include/sys/ioctl.h` gives `ioctl` and commands `TIOCGWINSZ`,
`TIOCSWINSZ`, `TIOCSCTTY`, `TIOCNOTTY`, `TIOCGPGRP`, `TIOCSPGRP`, `FIONREAD`,
and `FIONBIO` through Ring 3 `ttyserv` and `fcntl` ({need}`ARC_IPC_0001`,
{need}`SWR_IPC_0015`).
```
