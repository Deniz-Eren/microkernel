# Components — IPC

The `IPC` domain includes internal specifications for kernel message queues,
IOAPIC IRQ dispatch, and Ring 3 servers.

```{comp} Kernel channel queues, MsgV engine, and IRQ pulses
:id: COMP_IPC_0001
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0006

`kernel/core/channel.c` and `kernel/core/ipc.c` control channels, connection
tables, priority queues, priority inheritance, `ipc_msg_readv`,
`ipc_msg_writev`, and static IRQ pulses ({need}`ICD_IPC_0001`,
{need}`ARC_IPC_0001`).
```

```{comp} User-space sysproc name registry and ipcserv echo server
:id: COMP_IPC_0002
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001
:satisfies: SWR_IPC_0003

`servers/sysproc/main.c` accepts `NAME_MSG_ATTACH` and `NAME_MSG_QUERY` on PID 1
channel 1. `servers/ipcserv/main.c` records `/dev/echo` in the name registry and
gives replies to concurrent messages ({need}`ICD_IPC_0001`,
{need}`ARC_IPC_0001`).
```

```{comp} IOAPIC IRQ router and Ring 3 IOPL capability check
:id: COMP_IPC_0003
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0002
:satisfies: SWR_IPC_0006

`kernel/core/intr.c` maps the IOAPIC and puts `AttachIntrEvent`, `IntrUnmask`,
and `IOPLSet` behind `CAP_SYS_INTR`. It blocks active IRQ pins until
`IntrUnmask` and sends allocated pulses ({need}`ICD_IPC_0002`,
{need}`SWR_IPC_0006`).
```

```{comp} User-space MC146818 CMOS RTC driver
:id: COMP_IPC_0004
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002
:satisfies: SWR_IPC_0006

`servers/rtcdrv/arch/x86-64/cmos_rtc.c` operates in Ring 3 with `IOPLSet(3)` and
receives hardware IRQ 8 pulses through `AttachIntrEvent` and `IntrUnmask`. It
records `/dev/rtc` and gives replies to clients with `ReadMsgV` and `WriteMsgV`
({need}`ICD_IPC_0001`, {need}`ICD_IPC_0002`, {need}`SWR_IPC_0006`).
```

```{comp} Ring 3 pipe and ppoll server
:id: COMP_IPC_0005
:status: active
:posix: XSH/pipe2 (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0005
:satisfies: SWR_IPC_0004

`servers/pipeserv/main.c` records `/dev/pipe` in Ring 3 and keeps 4 KiB pipe
buffers. It gives replies to deferred synchronous `read`, `write`, and `ppoll`
messages, and accepts `PULSE_CODE_FD_CLOSE` close pulses ({need}`ICD_IPC_0003`,
{need}`SWR_IPC_0004`).
```

```{comp} Async-signal-safe libc descriptor, pipe2, dup3, and ppoll wrappers
:id: COMP_IPC_0006
:status: active
:posix: XSH/pipe2 (BASE), XSH/dup3 (BASE), XSH/ppoll (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0003, ICD_IPC_0004, ICD_IPC_0005
:satisfies: SWR_IPC_0004

`libc/src/fd.c` gives `pipe`, `pipe2`, `dup`, `dup2`, `dup3`, `fcntl`, `close`,
`poll`, and `ppoll` through `SYS_FD_OP` and `/dev/pipe` IPC with atomic
`O_CLOEXEC` and signal mask rules ({need}`ICD_IPC_0004`, {need}`ICD_IPC_0005`,
{need}`SWR_IPC_0004`).
```

```{comp} Ring 3 POSIX.1-2024 asynchronous I/O worker engine
:id: COMP_IPC_0007
:status: active
:posix: XBD/aio.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0006
:satisfies: SWR_IPC_0005

`libc/src/aio.c` does `aio_read`, `aio_write`, `aio_fsync`, `aio_cancel`, and
`lio_listio` on a Ring 3 worker thread with `SYS_FUTEX` and descriptor IPC. It
keeps `aio_error`, `aio_return`, and `aio_suspend` async-signal-safe
({need}`ICD_IPC_0006`, {need}`SWR_IPC_0005`).
```

```{comp} Ring 3 VFS server for bootfs and tmpfs
:id: COMP_IPC_0008
:status: active
:posix: XSH/open (BASE), XBD/sys/stat.h (BASE), XBD/dirent.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0007
:satisfies: SWR_IPC_0007

`servers/vfsserv/main.c` records `/dev/vfs`, gives read-only `boot.tar` entries
at `/boot`, and controls writable `tmpfs` files, directories, open file offsets,
and `PULSE_CODE_FD_CLOSE` slot removal. It gives `/etc/ld.so.conf` and the
`/usr/bin/ldd` and `/usr/sbin/ldconfig` symbolic links ({need}`ICD_IPC_0007`,
{need}`SWR_IPC_0007`).
```

```{comp} POSIX.1-2024 libc VFS, stat, dirent, and cwd wrappers
:id: COMP_IPC_0009
:status: active
:posix: XSH/open (BASE), XBD/sys/stat.h (BASE), XBD/dirent.h (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0004, ICD_IPC_0007, ICD_IPC_0008, ICD_IPC_0009
:satisfies: SWR_IPC_0007

`libc/src/vfs.c` gives `open`, `openat`, `creat`, `lseek`, `ftruncate`, `stat`,
`fstat`, `lstat`, `fstatat`, `mkdir`, `unlink`, `rmdir`, `chdir`, `fchdir`,
`getcwd`, `umask`, `access`, and `<dirent.h>` through `/dev/vfs` IPC
({need}`ICD_IPC_0007`, {need}`ICD_IPC_0008`, {need}`ICD_IPC_0009`,
{need}`SWR_IPC_0007`).
```

```{comp} Ring 3 serial TTY server and termios line control
:id: COMP_IPC_0010
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0010
:satisfies: SWR_IPC_0008

`servers/ttyserv/main.c` and `servers/ttyserv/arch/x86-64/uart_16550.c` operate
with `CAP_SYS_INTR` and COM1 IRQ 4 pulses and record `/dev/tty`. They give
canonical and raw `<termios.h>` line control and give replies to deferred
synchronous `read` and `ppoll` messages ({need}`ICD_IPC_0010`,
{need}`SWR_IPC_0008`).
```

```{comp} POSIX.1-2024 libc termios, isatty, and /dev/tty client wrappers
:id: COMP_IPC_0011
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0010, ICD_IPC_0011
:satisfies: SWR_IPC_0008

`libc/src/tty.c` gives async-signal-safe `tcgetattr`, `tcsetattr`, `tcflush`,
`tcdrain`, `tcflow`, `tcsendbreak`, `cf*speed`, `isatty`, and `/dev/tty`
`read`/`ppoll` helpers ({need}`ICD_IPC_0010`, {need}`ICD_IPC_0011`,
{need}`SWR_IPC_0008`).
```

```{comp} Ring 3 AF_UNIX local sockets server
:id: COMP_IPC_0012
:status: active
:posix: XBD/sys/socket.h (BASE), XSH/accept4 (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0003, ICD_IPC_0012
:satisfies: SWR_IPC_0009

`servers/sockserv/main.c` records `/dev/sock` in Ring 3 and gives `AF_UNIX`
`SOCK_STREAM` and `SOCK_DGRAM` sockets. It controls 4 KiB receive rings, accept
queues, deferred synchronous `accept`/`send`/`recv`/`ppoll` messages, and
`PULSE_CODE_FD_CLOSE` slot removal ({need}`ICD_IPC_0012`, {need}`SWR_IPC_0009`).
```

```{comp} POSIX.1-2024 libc AF_UNIX socket and accept4 wrappers
:id: COMP_IPC_0013
:status: active
:posix: XBD/sys/socket.h (BASE), XBD/sys/un.h (BASE), XSH/accept4 (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0012, ICD_IPC_0013
:satisfies: SWR_IPC_0009

`libc/src/socket.c` gives async-signal-safe `socket`, `socketpair`, `bind`,
`listen`, `accept`, `accept4`, `connect`, `send`, `recv`, `sendto`, and
`recvfrom`. It also gives `shutdown`, `getsockname`, `getpeername`,
`getsockopt`, and `setsockopt` through `/dev/sock` IPC with cancellation checks
({need}`ICD_IPC_0012`, {need}`ICD_IPC_0013`, {need}`SWR_IPC_0009`).
```

```{comp} POSIX.1-2024 libc select and pselect wrappers on multi-server ppoll
:id: COMP_IPC_0014
:status: active
:posix: XBD/sys/select.h (BASE), XSH/pselect (BASE), XSH/select (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0005, ICD_IPC_0014
:satisfies: SWR_DOCS_0004

`libc/src/select.c` gives `FD_CLR`, `FD_ISSET`, `FD_SET`, `FD_ZERO`, `select`,
and `pselect` on multi-server `ppoll()` with cancellation point checks. It
changes `POLLNVAL` to `EBADF` ({need}`ICD_IPC_0005`, {need}`ICD_IPC_0014`,
{need}`SWR_DOCS_0004`).
```

```{comp} CAP_SYS_INTR MMIO/DMA/PCI syscalls and Ring 3 virtio-blk driver
:id: COMP_IPC_0015
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0015
:satisfies: SWR_IPC_0010

`kernel/core/vmm.c` and `kernel/core/intr.c` give `CAP_SYS_INTR`
`SYS_VM_MAP_MMIO`, `SYS_DMA_ALLOC`, and `SYS_PCI_CFG` syscalls.
`servers/blkdrv/main.c` operates a Ring 3 Modern VirtIO 1.0 PCI block driver
(`/dev/blk`) with MMIO BARs, adjacent DMA virtqueues, and IOAPIC IRQ pulses
({need}`ICD_IPC_0002`, {need}`ICD_IPC_0015`, {need}`SWR_IPC_0010`).
```

```{comp} Writable ext2 disk filesystem mounted in vfsserv on /dev/blk
:id: COMP_IPC_0016
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0007, ICD_IPC_0015
:satisfies: SWR_IPC_0011

`servers/vfsserv/ext2.c` and `servers/vfsserv/main.c` format and mount a rev-1
`ext2` filesystem on `/mnt` through `/dev/blk`. They give 12 `direct` and
single-indirect data blocks, write-through `open`/`read`/`write`/`ftruncate`,
`mkdir`, `unlink`, `rmdir`, `rename`, `EXDEV`, and unlinked open inode removal
({need}`ICD_IPC_0007`, {need}`ICD_IPC_0015`, {need}`SWR_IPC_0011`).
```

```{comp} VFS links, permissions, FIFOs, offset I/O, statvfs, and XCU utilities
:id: COMP_IPC_0017
:status: active
:posix: XBD/sys/statvfs.h (BASE), XSH/posix_getdents (BASE), XSH/mkfifo (BASE)
:refines: ARC_IPC_0001, ICD_IPC_0016
:satisfies: SWR_IPC_0012

`servers/vfsserv/main.c`, `libc/src/vfs.c`, and `libc/src/vfs_helpers.c` give
permissions, ownership, symbolic links, hard links, synchronous FIFOs, offset
I/O, `posix_getdents`, `scandir`, `statvfs`, and temporary file helpers.
`bin/utils/` gives `chmod`, `chown`, `chgrp`, `ln`, `mkfifo`, `readlink`,
`realpath`, `touch`, `rmdir`, `df`, `du`, `dd`, `basename`, `dirname`, and
`pathchk` ({need}`ICD_IPC_0016`, {need}`SWR_IPC_0012`).
```

```{comp} Ring 3 PCI virtio-net driver and TCP/IP server (/dev/net)
:id: COMP_IPC_0018
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0001, ICD_IPC_0002, ICD_IPC_0017
:satisfies: SWR_IPC_0013

`servers/netserv/virtio_net.c` controls Modern VirtIO 1.0 PCI `virtio-net` with
MMIO BARs, adjacent DMA RX/TX virtqueues, and IOAPIC IRQ pulses.
`servers/netserv/net_proto.c` and `servers/netserv/main.c` give `/dev/net` with
loopback, Ethernet, ARP, IPv4, IPv6, ICMPv4, ICMPv6, UDP, TCP, and `poll`
({need}`ICD_IPC_0002`, {need}`ICD_IPC_0017`, {need}`SWR_IPC_0013`).
```

```{comp} Ring 3 netcat (nc), ping CLI, and host-to-guest network connections
:id: COMP_IPC_0019
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0017
:satisfies: SWR_IPC_0014

`bin/utils/nc.c` and `bin/utils/ping.c` give TCP/UDP client and listener
utilities (`-l`, `-k`, `-e`, `-u`) and ICMPv4/ICMPv6 `ping` through `/dev/net`.
`servers/netserv/` and `scripts/env.sh` give localhost host-to-guest TCP port
connections and safe RX/TX packet operation ({need}`ICD_IPC_0017`,
{need}`SWR_IPC_0014`).
```

```{comp} Ring 3 PTY multiplexer, virtual devices, and SCM_RIGHTS descriptors
:id: COMP_IPC_0020
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0010, ICD_IPC_0012, ICD_IPC_0018
:satisfies: SWR_IPC_0015

`servers/ttyserv/main.c` and `libc/src/tty.c` give Ring 3 PTY pairs
(`/dev/ptmx`, `/dev/pts/N`) and `ioctl`. `servers/vfsserv/main.c` gives
`/dev/null`, `/dev/zero`, and ChaCha20 `/dev/urandom`. `servers/sockserv/main.c`
and `libc/src/socket.c` give `AF_UNIX` `SCM_RIGHTS` FD messages
({need}`ICD_IPC_0018`, {need}`SWR_IPC_0015`).
```

```{comp} Ring 3 OpenSSH Portable build integration and boot setup helper
:id: COMP_IPC_0021
:status: active
:refines: ARC_IPC_0001, ICD_IPC_0017, ICD_IPC_0018
:satisfies: SWR_IPC_0016

`cmake/openssh.cmake`, `cmake/openssh_config.h`, and `cmake/openssh_compat/`
download and compile OpenSSH Portable `9.9p2` (`sshd`, `sshd-session`,
`ssh-keygen`) for Ring 3. They link these executables with sysroot OpenSSL
`libcrypto.a` and `zlib` `libz.a` into `boot.tar`. `bin/utils/sshd_setup.c`
initializes `/mnt/etc/ssh`, `/mnt/etc/passwd`, and Ed25519, ECDSA, and RSA host
keys, and starts `/boot/sshd` with `Compression delayed` on the `init=sh`
interactive boot process ({need}`ICD_IPC_0017`, {need}`ICD_IPC_0018`,
{need}`SWR_IPC_0016`).
```
