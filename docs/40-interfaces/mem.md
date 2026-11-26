# Interfaces — MEM

The `MEM` domain includes kernel virtual memory, safe user-copy, native VM, and
POSIX.1-2024 `<sys/mman.h>` interfaces.

```{icd} Virtual memory mapping and user-copy interface
:id: ICD_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0002, SWR_MEM_0003

`kernel/include/kernel/vmm.h` and `kernel/include/kernel/uaccess.h` give
address-space page table operations, `VMM_COW` fault resolution, and SMAP-safe
`copy_from_user`/`copy_to_user` entry points ({need}`ARC_MEM_0001`,
{need}`SWR_MEM_0002`, {need}`SWR_MEM_0003`).
```

```{icd} Ring 3 native virtual memory syscall interface
:id: ICD_MEM_0002
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0004

`libc/include/sys/vm.h` gives `VM_PROT_*`, `VM_MAP_*`, `VMMap`, `VMUnmap`,
`VMProtect`, and `VMCloneCow` wrappers for user address-space control
({need}`ARC_MEM_0001`, {need}`SWR_MEM_0004`).
```

```{icd} POSIX.1-2024 <sys/mman.h> public interface
:id: ICD_MEM_0003
:status: active
:posix: XBD/sys/mman.h (BASE), XSH/mmap (BASE)
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0004

`libc/include/sys/mman.h` gives `PROT_*`, `MAP_*`, `MAP_FAILED`, `MS_*`, `mmap`,
`munmap`, `mprotect`, and `msync`. It does not include `<sys/microkernel.h>`
({need}`ARC_MEM_0001`, {need}`SWR_MEM_0004`).
```
