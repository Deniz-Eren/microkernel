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

## x86-64 Virtual Address Space Layout

Each Ring 3 process runs in an isolated four-level page table (`PML4`) with W^X
enforcement. Kernel text, heap, and the physical direct map (HHDM) occupy the
upper canonical half.

```mermaid
flowchart TD
    subgraph KERNEL["Higher-Half Kernel Space (Ring 0, Supervisor Only)"]
        KELF["0xFFFFFFFF80000000+ : kernel.elf (W^X segments) & kheap"]
        HHDM["0xFFFF888000000000+ : Physical Direct Map (HHDM, SMAP-safe)"]
    end

    GAP["Non-Canonical Address Hole"]

    subgraph USER["Lower-Half Process Space (Ring 3, Per-Process PML4)"]
        MSTACK["0x0000700000000000 (down) : Main Stack (argv, envp, auxv)"]
        TSTACK["0x0000000050000000 - 0x80000000 : pthread Stacks & FS.base TLS"]
        BOOTFS["0x0000000011000000 : Read-Only boot.tar Mapping (vfsserv only)"]
        MMAP["0x0000000010000000+ : mmap / shm_open / libc Heap Arenas (W^X)"]
        UELF["0x0000000000400000+ : User ELF (.text R-X, .rodata R--, .data)"]
    end

    KELF --- HHDM --- GAP --- MSTACK --- TSTACK --- BOOTFS --- MMAP --- UELF
```

## Copy-on-Write (`fork` / `VMCloneCow`) Fault Resolution

`fork` and `VMCloneCow` share writable anonymous pages read-only until either
process writes. Shared `MAP_SHARED` and `SHM` mappings bypass COW so writes stay
visible across processes.

```mermaid
sequenceDiagram
    participant Parent as Parent Process (Ring 3)
    participant VMM as Kernel VMM / PMM (Ring 0)
    participant Child as Child Process (Ring 3)

    Parent->>VMM: fork() / VMCloneCow()
    VMM->>VMM: Mark private writable PTEs RO + VMM_COW, increment PMM refcounts
    VMM-->>Parent: Return child PID
    VMM-->>Child: Return 0 (shares physical frames read-only)

    Child->>VMM: Write to shared page -> #PF (Present + Write + User)
    alt PMM frame refcount > 1
        VMM->>VMM: Allocate new frame, copy 4 KiB via HHDM
        VMM->>VMM: Map new writable PTE in Child, decrement old refcount
    else PMM frame refcount == 1
        VMM->>VMM: Clear VMM_COW and mark PTE writable in place
    end
    VMM-->>Child: Resume instruction (Parent page stays isolated)
```
