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
