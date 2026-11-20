# Components — MEM

Domain `MEM`: White-box specifications for physical frames, virtual pages, and
safe user copy.

```{comp} Bitmap physical frame allocator and kernel heap
:id: COMP_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0001

`kernel/core/pmm.c` and `kernel/core/kheap.c` track free 4 KiB physical frames
in a bitmap and serve kernel heap allocations for the memory subsystem
({need}`ARC_MEM_0001`).
```

```{comp} Four-level page-table manager and HHDM user copier
:id: COMP_MEM_0002
:status: active
:refines: ARC_MEM_0001, ICD_MEM_0001
:satisfies: SWR_MEM_0002, SWR_MEM_0003

`kernel/core/vmm.c` and `copy_user_pml4` in `kernel/core/syscall.c` build
per-process PML4 page tables with W^X enforcement and translate each user page
through the HHDM ({need}`ICD_MEM_0001`) for the memory subsystem
({need}`ARC_MEM_0001`).
```
