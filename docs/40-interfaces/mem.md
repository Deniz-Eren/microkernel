# Interfaces — MEM

Domain `MEM`: kernel virtual memory, safe user-copy, native VM, and POSIX.1-2024
`<sys/mman.h>` interfaces.

```{icd} Virtual memory mapping and user-copy interface
:id: ICD_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0002, SWR_MEM_0003

`kernel/include/kernel/vmm.h` and `kernel/include/kernel/uaccess.h` specify
per-address-space page-table operations, `VMM_COW` fault resolution, and
SMAP-safe `copy_from_user`/`copy_to_user` entry points ({need}`ARC_MEM_0001`,
{need}`SWR_MEM_0002`, {need}`SWR_MEM_0003`).
```

```{icd} Ring 3 native virtual memory syscall interface
:id: ICD_MEM_0002
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0004

`libc/include/sys/vm.h` gives `VM_PROT_*`, `VM_MAP_*`, `VMMap`, `VMUnmap`,
`VMProtect`, and `VMCloneCow` wrappers for user address-space management
({need}`ARC_MEM_0001`, {need}`SWR_MEM_0004`).
```

```{icd} POSIX.1-2024 <sys/mman.h> public interface
:id: ICD_MEM_0003
:status: active
:posix: XBD/sys/mman.h (BASE), XSH/mmap (BASE)
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0004

`libc/include/sys/mman.h` declares `PROT_*`, `MAP_*`, `MAP_FAILED`, `MS_*`,
`mmap`, `munmap`, `mprotect`, and `msync` with no `<sys/microkernel.h>` leakage
({need}`ARC_MEM_0001`, {need}`SWR_MEM_0004`).
```
