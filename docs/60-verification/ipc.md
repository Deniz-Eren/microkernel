# Verification — IPC

Domain `IPC`: Verification items covering channels, synchronous message passing,
pulses, and `sysproc` name discovery.

```{ver} Verify synchronous channel IPC, pulses, and name service
:id: VER_IPC_0001
:status: active
:tags: integration
:verifies: SYS_IPC_0001, SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0003

The headless QEMU harness `tests/test_runner.py` runs `sysproc`, `ipcserv`, and
`ipccli`. It checks `/dev/echo` registration and lookup, three synchronous
message replies, and pulses ({need}`SYS_IPC_0001`, {need}`SWR_IPC_0001`,
{need}`SWR_IPC_0002`, {need}`SWR_IPC_0003`).
```

```{ver} Verify IOAPIC IRQ pulses, IOPLSet gate, ReadMsgV, and WriteMsgV
:id: VER_IPC_0002
:status: active
:tags: integration
:verifies: SWR_IPC_0006

The headless QEMU harness `tests/test_runner.py` runs `rtcdrv` and `drvtest`. It
checks Ring 3 `IOPLSet`, `-EPERM` denial for unprivileged processes, 3 hardware
IRQ 8 pulses, and `/dev/rtc` `ReadMsgV`/`WriteMsgV` round trips
({need}`SWR_IPC_0006`).
```

```{ver} Verify Ring 3 pipeserv, pipe2, dup3, and ppoll sigmask rules
:id: VER_IPC_0003
:status: active
:tags: integration
:verifies: SWR_IPC_0004

The headless QEMU harness `tests/test_runner.py` runs `pipeserv` and
`forkpipetest`. It checks `pipe2`, `dup3` `O_CLOEXEC`, cross-process pipe I/O,
EOF/`POLLHUP`, and `ppoll` timeout and `sigmask` `EINTR`. CTest `posix.headers`
and `posix.async_signal_safe` check `<fcntl.h>` and `<poll.h>`
({need}`SWR_IPC_0004`).
```

```{ver} Verify POSIX.1-2024 Base <aio.h> operations and optional MSG gating
:id: VER_IPC_0004
:status: active
:tags: integration
:verifies: SWR_IPC_0005

The headless QEMU harness `tests/test_runner.py` runs `aiotest`. It checks
`aio_read`, `aio_write`, `SIGEV_SIGNAL`, `aio_cancel`, `aio_suspend` `EAGAIN`
timeout, `aio_fsync`, and `lio_listio`. CTest `posix.headers` and
`posix.async_signal_safe` check `<aio.h>`, `_POSIX_MESSAGE_PASSING` gating, and
async-signal-safety ({need}`SWR_IPC_0005`).
```

```{ver} Verify Ring 3 VFS bootfs, tmpfs, stat, dirent, and cwd operations
:id: VER_IPC_0005
:status: active
:tags: integration
:verifies: SWR_IPC_0007

The headless QEMU harness `tests/test_runner.py` runs `vfsserv` and `vfstest`.
It checks `/boot` read-only `bootfs`, `/tmp` `tmpfs` I/O, `openat`, `lseek`,
`ftruncate`, `stat`, `readdir`, `chdir`, `getcwd`, shared `dup`/`fork` offsets,
and `rmdir`. CTest POSIX gates check `<sys/stat.h>` and `<dirent.h>`
({need}`SWR_IPC_0007`).
```

```{ver} Verify Ring 3 ttyserv, COM1 IRQ 4, termios, and multi-server ppoll
:id: VER_IPC_0006
:status: active
:tags: integration
:verifies: SWR_IPC_0008

The headless QEMU harness `tests/test_runner.py` runs `ttyserv` and `ttytest`.
It checks COM1 IRQ 4 pulses, `isatty`, `/dev/tty`, canonical line editing, raw
mode, and multi-server `ppoll`. CTest POSIX gates check `<termios.h>` and
`tcdrain` ({need}`SWR_IPC_0008`).
```

```{ver} Verify Ring 3 sockserv, AF_UNIX sockets, accept4, and ppoll
:id: VER_IPC_0007
:status: active
:tags: integration
:verifies: SWR_IPC_0009

The headless QEMU harness `tests/test_runner.py` runs `sockserv` and `socktest`.
It checks `socketpair`, `MSG_PEEK`, `shutdown`, `bind`/`listen`/`connect`, and
`accept4` with atomic `SOCK_CLOEXEC` and `SOCK_NONBLOCK` across `fork()`. It
also checks `SOCK_DGRAM` `sendto`/`recvfrom` and multi-server `ppoll()`. CTest
POSIX gates check `<sys/socket.h>`, `<sys/un.h>`, async-signal-safety, and
cancellation points ({need}`SWR_IPC_0009`).
```

```{ver} Verify CAP_SYS_INTR MMIO/DMA/PCI EPERM and /dev/blk persistence
:id: VER_IPC_0008
:status: active
:tags: integration
:verifies: SWR_IPC_0010

The headless QEMU harness `tests/test_runner.py` runs `blkdrv` and `blktest`
(`tests/blktest/test_blk.c`). It checks unprivileged `EPERM` on `VMMapMMIO`,
`DMAAlloc`, and `PCIConfig*`, multi-sector `/dev/blk` read/write I/O, and IOAPIC
IRQ pulses. CTest `ipc.blk_persist` (`tests/boot/test_blk_persist.py`) checks
sector persistence across two cold QEMU restarts ({need}`SWR_IPC_0010`).
```

```{ver} Verify writable ext2 /mnt persistence across QEMU cold restarts
:id: VER_IPC_0009
:status: active
:tags: integration
:verifies: SWR_IPC_0011

The headless QEMU harness `tests/test_runner.py` runs `ext2test`
(`tests/ext2test/test_ext2.c`). It checks `/mnt` directory creation, 16 KiB
multi-block writes and reads, `ftruncate`, `rename`, `EXDEV`, `unlink`, `rmdir`,
and `readdir`. CTest `ipc.vfs_persist` (`tests/boot/test_ext2_persist.py`)
checks the on-disk `ext2` superblock and verifies file and directory persistence
across two cold QEMU restarts ({need}`SWR_IPC_0011`).
```
