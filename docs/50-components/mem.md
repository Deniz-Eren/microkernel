# Components — MEM

Domain `MEM`: white-box specification for physical frames, virtual pages, COW
faults, safe user copy, and `<sys/mman.h>` wrappers.

```{comp} Bitmap physical frame allocator, refcounts, and kernel heap
:id: COMP_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0001

`kernel/core/pmm.c` and `kernel/core/kheap.c` track free 4 KiB physical frames
in a bitmap, keep per-frame COW reference counts, and serve kernel heap memory
({need}`ARC_MEM_0001`).
```

```{comp} Four-level page-table manager, COW handler, and HHDM copier
:id: COMP_MEM_0002
:status: active
:refines: ARC_MEM_0001, ICD_MEM_0001, ICD_MEM_0002
:satisfies: SWR_MEM_0002, SWR_MEM_0003, SWR_MEM_0004

`kernel/core/vmm.c` and `copy_user_pml4` in `kernel/core/syscall.c` manage
per-process PML4 tables with W^X enforcement, `sys_vm_map`, `sys_vm_unmap`,
`sys_vm_protect`, `sys_vm_clone_cow`, `#PF` COW copy, and HHDM copies
({need}`ICD_MEM_0001`, {need}`ICD_MEM_0002`, {need}`ARC_MEM_0001`).
```

```{comp} POSIX.1-2024 libc memory mapping wrappers
:id: COMP_MEM_0003
:status: active
:posix: XBD/sys/mman.h (BASE), XSH/mmap (BASE)
:refines: ARC_MEM_0001, ICD_MEM_0002, ICD_MEM_0003
:satisfies: SWR_MEM_0004

`libc/src/mman.c` gives `mmap`, `munmap`, `mprotect`, and `msync` over `VMMap`,
`VMUnmap`, and `VMProtect` and sets thread-local `errno` only on error
({need}`ICD_MEM_0002`, {need}`ICD_MEM_0003`, {need}`SWR_MEM_0004`).
```

```{comp} Thread-safe mmap-backed coalescing slab and large-block allocator
:id: COMP_MEM_0004
:status: active
:posix: XSH/malloc (BASE), XSH/free (BASE), XSH/realloc (BASE)
:refines: ARC_MEM_0001, ICD_MEM_0003
:satisfies: SWR_MEM_0006

`libc/src/malloc.c` supplies `malloc`, `free`, `calloc`, `realloc`, and
`reallocarray` over `mmap` and `munmap`. It uses a `SYS_FUTEX` lock, 64 KiB
coalescing slabs, direct large mappings, and `errno`-preserving `free`
({need}`ICD_MEM_0003`, {need}`SWR_MEM_0006`).
```
