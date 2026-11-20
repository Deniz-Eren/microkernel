# Verification — IPC

Domain `IPC`: Verification items covering channels, synchronous message passing,
pulses, and `sysproc` name discovery.

```{ver} Verify synchronous channel IPC, pulses, and name service
:id: VER_IPC_0001
:status: active
:tags: integration
:verifies: SYS_IPC_0001, SWR_IPC_0001, SWR_IPC_0002, SWR_IPC_0003

Verified by the headless QEMU harness `tests/test_runner.py` running `sysproc`,
`ipcserv`, and `ipccli`, confirming `/dev/echo` registration and lookup, three
synchronous message round trips, and pulse delivery ({need}`SYS_IPC_0001`,
{need}`SWR_IPC_0001`, {need}`SWR_IPC_0002`, {need}`SWR_IPC_0003`).
```
