# Verification — IPC

Domain `IPC`: Verification items for channels, synchronous messages, pulses, and
`sysproc` name lookup.

```{ver} Test of synchronous channel IPC, pulses, and name service
:id: VER_IPC_0001
:status: active
:tags: integration
:verifies: SYS_IPC_0001, SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0003

The QEMU test harness `tests/test_runner.py` starts `sysproc`, `ipcserv`, and
`ipccli`. These tests make sure that `ipcserv` attaches `/dev/echo` and `ipccli`
finds it, with three synchronous message replies and pulses
({need}`SYS_IPC_0001`, {need}`SWR_IPC_0001`, {need}`SWR_IPC_0002`,
{need}`SWR_IPC_0003`).
```

```{ver} Test of IOAPIC IRQ pulses, IOPLSet gate, ReadMsgV, and WriteMsgV
:id: VER_IPC_0002
:status: active
:tags: integration
:verifies: SWR_IPC_0006

The QEMU test harness `tests/test_runner.py` starts `rtcdrv` and `drvtest`.
These tests do a check of Ring 3 `IOPLSet` and `-EPERM` errors for unprivileged
processes. They also do a check of three hardware IRQ 8 pulses and `/dev/rtc`
`ReadMsgV` and `WriteMsgV` operations ({need}`SWR_IPC_0006`).
```

```{ver} Test of Ring 3 pipeserv, pipe2, dup3, and ppoll sigmask rules
:id: VER_IPC_0003
:status: active
:tags: integration
:verifies: SWR_IPC_0004

The QEMU test harness `tests/test_runner.py` starts `pipeserv` and
`forkpipetest`. These tests do a check of `pipe2`, `dup3` `O_CLOEXEC`, pipe I/O
between processes, EOF and `POLLHUP`, and `ppoll` timeout and `sigmask` `EINTR`.
CTest `posix.headers` and `posix.async_signal_safe` do a check of `<fcntl.h>`
and `<poll.h>` ({need}`SWR_IPC_0004`).
```

```{ver} Test of POSIX.1-2024 Base <aio.h> operations and optional MSG rules
:id: VER_IPC_0004
:status: active
:tags: integration
:verifies: SWR_IPC_0005

The QEMU test harness `tests/test_runner.py` starts `aiotest`. This test does a
check of `aio_read`, `aio_write`, `SIGEV_SIGNAL`, `aio_cancel`, `aio_suspend`
`EAGAIN` timeout, `aio_fsync`, and `lio_listio`. CTest `posix.headers` and
`posix.async_signal_safe` do a check of `<aio.h>`, `_POSIX_MESSAGE_PASSING`
rules, and async-signal-safety ({need}`SWR_IPC_0005`).
```

```{ver} Test of Ring 3 VFS bootfs, tmpfs, stat, dirent, and cwd operations
:id: VER_IPC_0005
:status: active
:tags: integration
:verifies: SWR_IPC_0007

The QEMU test harness `tests/test_runner.py` starts `vfsserv` and `vfstest`.
These tests do a check of `/boot` read-only `bootfs`, `/tmp` `tmpfs` I/O,
`openat`, `lseek`, `ftruncate`, and `stat`. They also do a check of `readdir`,
`chdir`, `getcwd`, shared `dup` and `fork` offsets, and `rmdir`. CTest POSIX
compatibility gates do a check of `<sys/stat.h>` and `<dirent.h>`
({need}`SWR_IPC_0007`).
```

```{ver} Test of Ring 3 ttyserv, COM1 IRQ 4, termios, and multi-server ppoll
:id: VER_IPC_0006
:status: active
:tags: integration
:verifies: SWR_IPC_0008

The QEMU test harness `tests/test_runner.py` starts `ttyserv` and `ttytest`.
These tests do a check of COM1 IRQ 4 pulses, `isatty`, `/dev/tty`, canonical
line input, raw mode, and multi-server `ppoll`. CTest POSIX compatibility gates
do a check of `<termios.h>` and `tcdrain` ({need}`SWR_IPC_0008`).
```

```{ver} Test of Ring 3 sockserv, AF_UNIX sockets, accept4, and ppoll
:id: VER_IPC_0007
:status: active
:tags: integration
:verifies: SWR_IPC_0009

The QEMU test harness `tests/test_runner.py` starts `sockserv` and `socktest`.
These tests do a check of `socketpair`, `MSG_PEEK`, `shutdown`, `bind`,
`listen`, `connect`, and `accept4` with atomic `SOCK_CLOEXEC` and
`SOCK_NONBLOCK` across `fork()`. They also do a check of `SOCK_DGRAM` `sendto`,
`recvfrom`, and multi-server `ppoll()`. CTest POSIX compatibility gates do a
check of `<sys/socket.h>`, `<sys/un.h>`, async-signal-safety, and cancellation
points ({need}`SWR_IPC_0009`).
```

```{ver} Test of CAP_SYS_INTR MMIO, DMA, PCI EPERM, and /dev/blk data storage
:id: VER_IPC_0008
:status: active
:tags: integration
:verifies: SWR_IPC_0010

The QEMU test harness `tests/test_runner.py` starts `blkdrv` and `blktest`
(`tests/blktest/test_blk.c`). These tests do a check of unprivileged `EPERM` on
`VMMapMMIO`, `DMAAlloc`, and `PCIConfig*`, multi-sector `/dev/blk` I/O, and
IOAPIC IRQ pulses. CTest `ipc.blk_persist` (`tests/boot/test_blk_persist.py`)
makes sure that disk sectors stay unchanged across two cold QEMU starts
({need}`SWR_IPC_0010`).
```

```{ver} Test of writable ext2 /mnt data storage across QEMU cold starts
:id: VER_IPC_0009
:status: active
:tags: integration
:verifies: SWR_IPC_0011

The QEMU test harness `tests/test_runner.py` starts `ext2test`
(`tests/ext2test/test_ext2.c`). This test does a check of `/mnt` directories, 16
KiB multi-block I/O, `ftruncate`, `rename`, `EXDEV`, `unlink`, `rmdir`, and
`readdir`. CTest `ipc.vfs_persist` (`tests/boot/test_ext2_persist.py`) examines
the disk `ext2` superblock and makes sure that files and directories stay
unchanged across two cold QEMU starts ({need}`SWR_IPC_0011`).
```

```{ver} Test of POSIX.1-2024 VFS links, permissions, FIFOs, and XCU utilities
:id: VER_IPC_0010
:status: active
:tags: integration
:verifies: SWR_IPC_0012

`tests/utilstest/test_utils.c` does a check of `chmod*`, `chown*`, `faccessat`,
hard and symbolic links with `ELOOP`, and synchronous FIFOs between processes.
It also does a check of `pread`, `pwrite`, `truncate`, `posix_getdents`,
`scandir`, `readdir_r`, `statvfs`, and temporary file interfaces.
`tests/shtest/test_sh.c` starts all 15 XCU file utilities through `/boot/sh`.
CTest compatibility gates 1 to 3 do a check of `<sys/statvfs.h>`, Base symbol
rules, and async-signal-safety ({need}`SWR_IPC_0012`).
```

```{ver} Test of Ring 3 PCI virtio-net driver and TCP, IP, and loopback server
:id: VER_IPC_0011
:status: active
:tags: integration
:verifies: SWR_IPC_0013

The QEMU test harness `tests/test_runner.py` boots with `virtio-net-pci` on
`-netdev user` and starts `netserv` and `nettest` (`tests/nettest/test_net.c`).
These tests do a check of hardware ARP and ICMPv4 echo to SLIRP gateway
`10.0.2.2` with IOAPIC IRQ pulses. They also do a check of loopback ICMPv4 and
ICMPv6, IPv4 and IPv6 UDP, and TCP handshake, data, `FIN`, `RST`, and resource
release ({need}`SWR_IPC_0013`).
```

```{ver} Test of host-to-guest TCP forward, netserv streams, nc, and ping
:id: VER_IPC_0012
:status: active
:tags: integration
:verifies: SWR_IPC_0014

CTest `ipc.net_hostfwd` (`tests/boot/test_net_hostfwd.py`) boots Microkernel
with QEMU SLIRP `hostfwd` and sends a multi-segment TCP stream from a host
Python client to `/boot/nc -l` on PCI `virtio-net`. `shtest`
(`tests/shtest/test_sh.c`) does a check of `/boot/ping` (`127.0.0.1` and
`10.0.2.2`) and `/boot/nc` loopback TCP sessions ({need}`SWR_IPC_0014`).
```

```{ver} Test of Ring 3 PTYs, virtual character devices, and SCM_RIGHTS
:id: VER_IPC_0013
:status: active
:tags: integration
:verifies: SWR_IPC_0015

`ttytest` (`tests/ttytest/test_tty.c`) does a check of PTY controller and
subordinate I/O, `ioctl`, `SIGWINCH`, `ISIG`, and `POLLHUP`. `socktest`
(`tests/socktest/test_sock.c`) makes sure that `SCM_RIGHTS` sends descriptors
between processes with `MSG_CMSG_CLOEXEC`. `utilstest`
(`tests/utilstest/test_utils.c`) does a check of `/dev/null`, `/dev/zero`, and
`/dev/urandom` ({need}`SWR_IPC_0015`).
```

```{ver} Test of OpenSSH client logins into Microkernel sshd in QEMU
:id: VER_IPC_0014
:status: active
:tags: integration
:verifies: SWR_IPC_0016

CTest `ipc.sshd_e2e` (`tests/boot/test_sshd_e2e.py`) boots Microkernel with
`init=sh` and localhost QEMU SLIRP `hostfwd` to guest port 22. This test makes
sure that `/boot/sshd` starts automatically and connects with the OpenSSH `ssh`
client. It does a check of Ed25519, RSA (`aes128-gcm@openssh.com`), and ECDSA
P-256 (`ecdh-sha2-nistp256`) authentication sessions. It also does a check of
`zlib@openssh.com` compression (`-C`), password login, and an interactive `-tt`
PTY session on `/dev/pts/` ({need}`SWR_IPC_0016`).
```
