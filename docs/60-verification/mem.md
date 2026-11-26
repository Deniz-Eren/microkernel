# Verification — MEM

Domain `MEM`: Verification items for physical frames, W^X virtual memory
protection, and safe user memory copies.

```{ver} Test of PMM, W^X VMM protection, and SMAP-safe user copies
:id: VER_MEM_0001
:status: active
:tags: integration
:verifies: SYS_MEM_0001, SWR_MEM_0001, SWR_MEM_0002, SWR_MEM_0003

The QEMU test harness `tests/test_runner.py` operates with `-cpu
qemu64,+smap,+smep`. This test makes sure that the kernel initializes the PMM
and VMM. It also makes sure that the kernel makes safe user copies across
address spaces during IPC and signal delivery ({need}`SYS_MEM_0001`,
{need}`SWR_MEM_0001`, {need}`SWR_MEM_0002`, {need}`SWR_MEM_0003`).
```

```{ver} Test of VMMap, VMUnmap, VMCloneCow, W^X, and <sys/mman.h>
:id: VER_MEM_0002
:status: active
:tags: integration
:verifies: SWR_MEM_0004

The QEMU test harness `tests/test_runner.py` starts `proctest` and `procchild`.
These tests do a check of `mmap`, `mprotect`, `msync`, `munmap`, and
`VMCloneCow` page faults, and make sure that the kernel rejects W^X mappings.
CTest `posix.headers` (`tests/posix/test_headers.py`) compiles `<sys/mman.h>`
independently ({need}`SWR_MEM_0004`).
```

```{ver} Test of mmap heap release, realloc, and thread safety
:id: VER_MEM_0003
:status: active
:tags: integration
:verifies: SWR_MEM_0006

The QEMU test harness `tests/test_runner.py` starts `heaptest`
(`tests/heaptest/test_heap.c`). This test makes sure that the allocator aligns
memory to 16 bytes, cycles 20 MiB slabs, and unmaps large memory areas. It also
makes sure that `free` keeps `errno`, `realloc` obeys limits, and four threads
operate safely ({need}`SWR_MEM_0006`).
```

```{ver} Test of option-controlled SHM, MAP_SHARED, ML, and MLR memory
:id: VER_MEM_0004
:status: active
:tags: integration
:verifies: SWR_MEM_0005

The QEMU test harness `tests/test_runner.py` starts `shmtest`
(`tests/shmtest/test_shm.c`). This test does a check of `shm_open`,
`shm_unlink`, `FD_CLOEXEC`, shared `MAP_SHARED` memory between processes,
`mlock`, and `mlockall`. CTest `posix.headers` and `posix.symbol_diff` do a
check of option macros ({need}`SWR_MEM_0005`).
```
