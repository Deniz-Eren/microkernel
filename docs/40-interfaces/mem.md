# Interfaces — MEM

Domain `MEM`: Kernel virtual memory and safe user-pointer copy interfaces.

```{icd} Virtual memory mapping and user-copy interface
:id: ICD_MEM_0001
:status: active
:refines: ARC_MEM_0001
:satisfies: SWR_MEM_0002, SWR_MEM_0003

`kernel/include/kernel/vmm.h` and `kernel/include/kernel/uaccess.h` define
per-address-space page-table mapping operations and SMAP-safe
`copy_from_user`/`copy_to_user` entry points for the memory subsystem
({need}`ARC_MEM_0001`), satisfying page isolation ({need}`SWR_MEM_0002`) and
safe user copy ({need}`SWR_MEM_0003`).
```
