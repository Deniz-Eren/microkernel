# Verification — MEM

Domain `MEM`: Verification items covering physical frames, virtual memory W^X
isolation, and safe user memory copies.

```{ver} Verify PMM, W^X VMM isolation, and SMAP-safe user copies
:id: VER_MEM_0001
:status: active
:tags: integration
:verifies: SYS_MEM_0001, SWR_MEM_0001, SWR_MEM_0002, SWR_MEM_0003

Verified by the headless QEMU harness `tests/test_runner.py` running with `-cpu
qemu64,+smap,+smep`, confirming PMM/VMM initialization and cross-space user
copies during IPC and signal delivery ({need}`SYS_MEM_0001`,
{need}`SWR_MEM_0001`, {need}`SWR_MEM_0002`, {need}`SWR_MEM_0003`).
```
