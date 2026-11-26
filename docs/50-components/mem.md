# Components — MEM

The `MEM` domain includes internal specifications for physical frames, virtual
pages, COW faults, safe user copy, and `<sys/mman.h>` wrappers.

```{comp} Bitmap physical frame allocator, COW counts, and kernel heap
:id: COMP_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0001

`kernel/core/pmm.c` and `kernel/core/kheap.c` monitor free 4 KiB physical frames
in a bitmap, keep frame COW counts, and allocate kernel heap memory
({need}`ARC_MEM_0001`).
```

```{comp} 4-level page table manager, COW handler, and HHDM copy module
:id: COMP_MEM_0002
:status: active
:refines: ARC_MEM_0001, ICD_MEM_0001, ICD_MEM_0002
:satisfies: SWR_MEM_0002, SWR_MEM_0003, SWR_MEM_0004

`kernel/core/vmm.c` and `copy_user_pml4` in `kernel/core/syscall.c` control
process PML4 tables with W^X checks, `sys_vm_map`, `sys_vm_unmap`,
`sys_vm_protect`, `sys_vm_clone_cow`, `#PF` COW copy, and HHDM copy operations
({need}`ICD_MEM_0001`, {need}`ICD_MEM_0002`, {need}`ARC_MEM_0001`).
```

```{comp} POSIX.1-2024 libc memory mapping wrappers
:id: COMP_MEM_0003
:status: active
:posix: XBD/sys/mman.h (BASE), XSH/mmap (BASE)
:refines: ARC_MEM_0001, ICD_MEM_0002, ICD_MEM_0003
:satisfies: SWR_MEM_0004

`libc/src/mman.c` gives `mmap`, `munmap`, `mprotect`, and `msync` on `VMMap`,
`VMUnmap`, and `VMProtect` and sets thread-local `errno` only on error
({need}`ICD_MEM_0002`, {need}`ICD_MEM_0003`, {need}`SWR_MEM_0004`).
```

```{comp} Thread-safe mmap slab and large-block allocator
:id: COMP_MEM_0004
:status: active
:posix: XSH/malloc (BASE), XSH/free (BASE), XSH/realloc (BASE)
:refines: ARC_MEM_0001, ICD_MEM_0003
:satisfies: SWR_MEM_0006

`libc/src/malloc.c` gives `malloc`, `free`, `calloc`, `realloc`, and
`reallocarray` on `mmap` and `munmap`. It uses a `SYS_FUTEX` mutex, 64 KiB
merged slabs, and large page regions, and `free` keeps `errno` unchanged
({need}`ICD_MEM_0003`, {need}`SWR_MEM_0006`).
```

```{comp} Kernel POSIX shared memory objects and ML/MLR page lock control
:id: COMP_MEM_0005
:status: active
:posix: XBD/sys/mman.h (ML, MLR, SHM)
:refines: ARC_MEM_0001, ICD_MEM_0001, ICD_MEM_0002, ICD_MEM_0003
:satisfies: SWR_MEM_0005

`kernel/core/shm.c` controls PMM-counted SHM objects (`SYS_SHM_OP`,
`SYS_SHM_MAP`, `FD_KIND_SHM`) for multi-process `MAP_SHARED`.
`kernel/core/vmm.c` obeys `VMM_SHARED` across `fork()` and `VMM_LOCKED` for
`SYS_VM_MLOCK` ({need}`ICD_MEM_0003`, {need}`SWR_MEM_0005`).
```
