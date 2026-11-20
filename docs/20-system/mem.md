# System and Software Requirements — MEM

Domain `MEM`: physical memory allocator, virtual memory, W^X policy, safe user
memory copy, and POSIX.1-2024 memory mapping.

```{sys} Isolate kernel and user memory with W^X protection
:id: SYS_MEM_0001
:status: active

The system must manage physical memory and virtual memory in isolated address
spaces. It must obey the W^X policy and examine page permissions before each
user memory copy.
```

```{swr} Manage physical frames and kernel heap
:id: SWR_MEM_0001
:status: active
:derives: SYS_MEM_0001

The memory subsystem must monitor usable RAM in a physical bitmap allocator. It
must allocate kernel heap memory for kernel data structures
({need}`SYS_MEM_0001`).
```

```{swr} Use four-level page tables and W^X policy
:id: SWR_MEM_0002
:status: active
:derives: SYS_MEM_0001

The virtual memory manager must keep four-level x86-64 PML4 page tables for each
process. It must map each page as writable or executable, not the two together
({need}`SYS_MEM_0001`).
```

```{swr} Copy user memory with SMAP-safe permission checks
:id: SWR_MEM_0003
:status: active
:derives: SYS_MEM_0001

The kernel must copy data between kernel buffers and user address spaces through
the HHDM. It must examine page mappings and write permissions with SMAP active
({need}`SYS_MEM_0001`).
```

```{swr} Supply POSIX.1-2024 Base memory mapping interfaces
:id: SWR_MEM_0004
:status: active
:posix: XBD/sys/mman.h (BASE), XSH/mmap (BASE)
:derives: SYS_MEM_0001
:traces: BDR_0003, ADR_0003

`<sys/mman.h>` ([sys/mman.h][hdr-mman]) must declare mandatory Issue 8 Base
functions `mmap` ([mmap][fn-mmap]), `mprotect`, `msync`, and `munmap`. These
functions must use `VMMap`, `VMUnmap`, and `VMCloneCow` and obey the W^X policy
({need}`SYS_MEM_0001`).
```

```{swr} Put optional POSIX memory locking and shared memory behind macros
:id: SWR_MEM_0005
:status: draft
:posix: XBD/sys/mman.h (ML, MLR, SHM)
:derives: SYS_MEM_0001
:traces: BDR_0003, ADR_0003

`<sys/mman.h>` ([sys/mman.h][hdr-mman]) must put `mlock`/`munlock` (`MLR`),
`mlockall`/`munlockall` (`ML`), and `shm_open`/`shm_unlink` (`SHM`) behind
`_POSIX_MEMLOCK_RANGE`, `_POSIX_MEMLOCK`, and `_POSIX_SHARED_MEMORY_OBJECTS` in
`<unistd.h>` ({need}`SYS_MEM_0001`).
```

[hdr-mman]:
  https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_mman.h.html
[fn-mmap]:
  https://pubs.opengroup.org/onlinepubs/9799919799/functions/mmap.html
