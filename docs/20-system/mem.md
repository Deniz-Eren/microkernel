# System and Software Requirements — MEM

Domain `MEM`: Physical frame allocation, virtual address-space isolation, W^X
enforcement, and safe user memory access.

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
