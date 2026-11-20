# Architecture — MEM

Domain `MEM`: Structural architecture elements for physical and virtual memory
management.

```{arc} Kernel memory management subsystem
:id: ARC_MEM_0001
:status: active
:satisfies: SYS_MEM_0001
:traces: ADR_0002

The memory subsystem combines the bitmap physical frame allocator, four-level
W^X virtual memory manager, kernel heap, and HHDM-backed user memory accessors
to satisfy memory isolation ({need}`SYS_MEM_0001`).
```
