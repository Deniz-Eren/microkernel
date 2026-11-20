# System and Software Requirements — MEM

Domain `MEM`: Physical frame allocation, virtual address-space isolation, W^X
enforcement, safe user memory access, and POSIX.1-2024 memory mapping.

```{sys} Isolate kernel and user memory with W^X protection
:id: SYS_MEM_0001
:status: active

The system shall manage physical and virtual memory with per-process address
spaces, enforce W^X page permissions, and validate permissions before copying
user memory.
```

```{swr} Manage physical frames and kernel heap
:id: SWR_MEM_0001
:status: active
:derives: SYS_MEM_0001

The memory subsystem shall track usable RAM in a physical bitmap allocator and
provide dynamic kernel heap allocation for kernel data structures
({need}`SYS_MEM_0001`).
```

```{swr} Enforce four-level page tables and W^X policy
:id: SWR_MEM_0002
:status: active
:derives: SYS_MEM_0001

The virtual memory manager shall maintain per-process x86-64 PML4 page tables
and map every page as writable or executable, never both ({need}`SYS_MEM_0001`).
```

```{swr} Copy user memory with SMAP-safe permission checks
:id: SWR_MEM_0003
:status: active
:derives: SYS_MEM_0001

The kernel shall copy data between kernel buffers and any user address space
through the HHDM after verifying page presence and write permission without
bypassing SMAP ({need}`SYS_MEM_0001`).
```

```{swr} Provide POSIX.1-2024 Base memory mapping interfaces
:id: SWR_MEM_0004
:status: draft
:derives: SYS_MEM_0001
:traces: BDR_0003, ADR_0003

`<sys/mman.h>` ([sys/mman.h][hdr-mman]) shall expose mandatory Issue 8 Base
functions `mmap` ([mmap][fn-mmap]), `mprotect`, `msync`, and `munmap` backed by
`VMMap`, `VMUnmap`, and `VMCloneCow` without violating the kernel W^X invariant
({need}`SYS_MEM_0001`).
```

```{swr} Gate optional POSIX memory locking and shared memory objects
:id: SWR_MEM_0005
:status: draft
:derives: SYS_MEM_0001
:traces: BDR_0003, ADR_0003

Optional `<sys/mman.h>` ([sys/mman.h][hdr-mman]) interfaces `mlock`/`munlock`
(`MLR`), `mlockall`/`munlockall` (`ML`), and `shm_open`/`shm_unlink` (`SHM`)
shall remain gated behind `_POSIX_MEMLOCK_RANGE`, `_POSIX_MEMLOCK`, and
`_POSIX_SHARED_MEMORY_OBJECTS` in `<unistd.h>` ({need}`SYS_MEM_0001`).
```

[hdr-mman]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_mman.h.html
[fn-mmap]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/mmap.html
