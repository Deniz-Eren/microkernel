# System and Software Requirements — IPC

The `IPC` domain includes synchronous message passing channels, connections,
priority inheritance, pulses, the name registry, and POSIX.1-2024 descriptor IPC
and AIO.

```{sys} Give synchronous IPC and service lookup
:id: SYS_IPC_0001
:status: active

The system must give synchronous channel message passing, asynchronous pulses,
and user-space service lookup between Ring 3 processes.
```

```{swr} Control IPC channels and client connections
:id: SWR_IPC_0001
:status: active
:derives: SYS_IPC_0001

The kernel must make and remove process receive channels. It must attach and
disconnect client connections for a `(pid, chanid)` endpoint
({need}`SYS_IPC_0001`).
```

```{swr} Move synchronous messages with priority inheritance and pulses
:id: SWR_IPC_0002
:status: active
:derives: SYS_IPC_0001

The IPC subsystem must block `SendMsg` callers until `ReceiveMsg` and `ReplyMsg`
complete. It must increase receiver priority during service and send 8-byte
pulses with `SendPulse` ({need}`SYS_IPC_0001`).
```

```{swr} Find named service endpoints in sysproc
:id: SWR_IPC_0003
:status: active
:derives: SYS_IPC_0001

The `sysproc` name registry must accept `NAME_MSG_ATTACH` messages from servers.
It must find `(pid, chanid)` pairs for `NAME_MSG_QUERY` lookups
({need}`SYS_IPC_0001`).
```

```{swr} Give POSIX.1-2024 Base descriptor IPC and ppoll
:id: SWR_IPC_0004
:status: active
:posix: XSH/pipe2 (BASE), XSH/dup3 (BASE), XSH/ppoll (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0011

Ring 3 system servers through kernel channels must give mandatory Issue 8 Base
functions `pipe2` ([pipe2][fn-pipe2]), `dup3`, and `ppoll` ([ppoll][fn-ppoll],
[poll.h][hdr-poll]). They must obey atomic `O_CLOEXEC` and signal mask rules
({need}`SYS_IPC_0001`).
```

```{swr} Give POSIX.1-2024 Base AIO and optional message queues
:id: SWR_IPC_0005
:status: active
:posix: XBD/aio.h (BASE), XBD/mqueue.h (MSG)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0012

`<aio.h>` ([aio.h][hdr-aio]) must give mandatory Issue 8 Base AIO functions
(`aio_read`, `aio_write`, `aio_error`, `aio_return`, `aio_cancel`, `aio_fsync`,
`aio_suspend`, `lio_listio`) through Ring 3 servers. `<mqueue.h>`
([mqueue.h][hdr-mq], `MSG`) must stay behind `_POSIX_MESSAGE_PASSING`
({need}`SYS_IPC_0001`).
```

```{swr} Send IRQ pulses, give Ring 3 IOPL, and do scatter-gather IPC
:id: SWR_IPC_0006
:status: active
:derives: SYS_IPC_0001

The kernel must send hardware IRQs through the IOAPIC as IPC pulses with
`AttachIntrEvent` and `IntrUnmask`. It must put `IOPLSet` behind `CAP_SYS_INTR`
and move multi-buffer payloads with `ReadMsgV` and `WriteMsgV`
({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 VFS, bootfs, and tmpfs file operations
:id: SWR_IPC_0007
:status: active
:posix: XSH/open (BASE), XBD/sys/stat.h (BASE), XBD/dirent.h (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0016

A Ring 3 VFS server through kernel IPC must give `bootfs` and writable `tmpfs`
files. It must give `open`, `openat`, `lseek`, `ftruncate`, `stat`, `fstat`,
`readdir`, `mkdir`, `unlink`, `chdir`, and `getcwd` ({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 serial TTY server and termios line control
:id: SWR_IPC_0008
:status: active
:posix: XBD/termios.h (BASE), XSH/tcgetattr (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0018

A Ring 3 TTY server with COM1 IRQ 4 pulses must give `<termios.h>` line control
and `stdin` `read()` with `poll` and `ppoll` event status
({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 AF_UNIX local sockets and POSIX.1-2024 accept4
:id: SWR_IPC_0009
:status: active
:posix: XBD/sys/socket.h (BASE), XSH/accept4 (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, ADR_0003, ADR_0020

A Ring 3 socket server through kernel IPC must give `<sys/socket.h>` `AF_UNIX`
streams (`socket`, `socketpair`, `bind`, `listen`, `accept`, `accept4`,
`connect`, `send`, `recv`) with atomic `SOCK_CLOEXEC` and `SOCK_NONBLOCK`
({need}`SYS_IPC_0001`).
```

```{swr} Give CAP_SYS_INTR MMIO and DMA syscalls and Ring 3 virtio-blk driver
:id: SWR_IPC_0010
:status: active
:derives: SYS_IPC_0001
:traces: ADR_0003, ADR_0008, ADR_0016

The kernel must give `CAP_SYS_INTR` syscalls for MMIO device access, PCI
configuration, and DMA buffers. A Ring 3 PCI `virtio-blk` driver (`/dev/blk`)
must do sector read and write I/O with IOAPIC IRQ pulses ({need}`SYS_IPC_0001`).
```

```{swr} Mount a writable disk filesystem in vfsserv on /dev/blk
:id: SWR_IPC_0011
:status: active
:derives: SYS_IPC_0001
:traces: ADR_0003, ADR_0016

`vfsserv` must mount a writable disk filesystem (for example, `ext2`) on
`/dev/blk`. Files and directories must stay available across QEMU starts
({need}`SYS_IPC_0001`).
```

```{swr} Give POSIX.1-2024 Base VFS links, permissions, FIFOs, and statvfs
:id: SWR_IPC_0012
:status: active
:posix: XBD/sys/statvfs.h (BASE), XSH/posix_getdents (BASE), XSH/mkfifo (BASE)
:derives: SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0003, ADR_0030

`vfsserv` and `libc` must give POSIX.1-2024 Base permissions (`chmod*`,
`chown*`, `faccessat`), hard and symbolic links (`link*`, `symlink*`,
`readlink*`), FIFOs (`mkfifo*`), and offset I/O (`pread`, `pwrite`, `truncate`,
`posix_close`). They must also give `posix_getdents`, `scandir`, `alphasort`,
`readdir_r`, [sys/statvfs.h][hdr-statvfs], `remove`, temporary file helpers, and
the 15 XCU Base file utilities ({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 PCI virtio-net driver and TCP/IP server
:id: SWR_IPC_0013
:status: active
:derives: SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0002, ADR_0031, ADR_0042

A Ring 3 `netserv` process with `CAP_SYS_INTR` must control Modern VirtIO 1.0
PCI `virtio-net` hardware with active MMIO, DMA, and IOAPIC IRQ pulses. It must
record `/dev/net` in the name registry and give loopback (`127.0.0.1`, `::1`),
Ethernet, ARP, IPv4, and IPv6. It must also give ICMPv4, ICMPv6, UDP, and TCP
with no new Ring 0 syscalls ({need}`SYS_IPC_0001`).
```

```{swr} Give host-to-guest TCP relay, netserv wire checks, nc, and ping
:id: SWR_IPC_0014
:status: active
:derives: SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0002, ADR_0042, ADR_0052

`netserv` must record same-subnet ARP entries from received IPv4 frames and keep
a buffer of a minimum of 32 KiB for each TCP socket. It must send window-update
ACKs and prevent TX virtqueue slot overwrites. User space must give Ring 3 `nc`
and `ping`, and `scripts/env.sh run` must send localhost TCP ports into the VM
({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 PTYs, virtual character devices, and SCM_RIGHTS
:id: SWR_IPC_0015
:status: active
:posix: XSH/posix_openpt, XSH/grantpt, XSH/unlockpt, XSH/ptsname (XSI)
:derives: SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0002, ADR_0003, ADR_0053

`ttyserv` must give Ring 3 PTY pairs (`/dev/ptmx`, `/dev/pts/N`) with `termios`,
`winsize`, `SIGWINCH`, `ISIG`, `poll`, and `<sys/ioctl.h>`. `vfsserv` must give
`/dev/null`, `/dev/zero`, and ChaCha20 `/dev/urandom`. `sockserv` must send open
descriptors across `AF_UNIX` `SCM_RIGHTS` messages ({need}`SYS_IPC_0001`).
```

```{swr} Give Ring 3 OpenSSH Portable sshd, ssh-keygen, and boot setup
:id: SWR_IPC_0016
:status: active
:derives: SYS_IPC_0001
:traces: BDR_0003, BDR_0004, ADR_0002, ADR_0054, ADR_0055, ADR_0056

The CMake build must download and compile OpenSSH Portable `sshd`,
`sshd-session`, and `ssh-keygen` for Ring 3 into `boot.tar`. The build must not
keep upstream source in git. These executables must link against sysroot OpenSSL
and `zlib`. The interactive `init=sh` boot process must start `/boot/sshd-setup`
to prepare `/mnt/etc/ssh`, Ed25519, ECDSA, and RSA host keys, and
`/mnt/etc/passwd`. It must start `/boot/sshd` on TCP port 22 for RSA, ECDSA,
Ed25519, compressed (`zlib@openssh.com`), password, and PTY sessions
({need}`SYS_IPC_0001`).
```

[fn-pipe2]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/pipe2.html
[fn-ppoll]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/ppoll.html
[hdr-poll]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/poll.h.html
[hdr-aio]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/aio.h.html
[hdr-mq]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/mqueue.h.html
[hdr-statvfs]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_statvfs.h.html
