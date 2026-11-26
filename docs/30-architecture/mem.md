# Architecture — MEM

Domain `MEM`: Architecture items for physical memory and virtual memory.

```{arc} Kernel memory subsystem
:id: ARC_MEM_0001
:status: active
:satisfies: SYS_MEM_0001
:traces: ADR_0002

The memory subsystem includes the bitmap frame allocator, the four-level W^X
virtual memory manager, the kernel heap, and HHDM user-memory functions. These
components isolate process address spaces ({need}`SYS_MEM_0001`).
```

## x86-64 Virtual Address Space Layout

Each Ring 3 process operates in an isolated four-level page table (`PML4`) that
obeys W^X rules. Kernel code, the kernel heap, and the HHDM physical window are
in the higher half.

```mermaid
flowchart TD
    subgraph KERNEL["Higher-Half Kernel Space (Ring 0 Only)"]
        KELF["0xFFFFFFFF80000000+ : kernel.elf (W^X segments) and kheap"]
        HHDM["0xFFFF888000000000+ : HHDM Physical Window (SMAP-safe)"]
    end

    GAP["Non-Canonical Address Hole"]

    subgraph USER["Lower-Half Process Space (Ring 3, Process PML4)"]
        MSTACK["0x0000700000000000 (down) : Primary Stack (argv, envp, auxv)"]
        TSTACK["0x0000000050000000-0x80000000 : pthread Stacks and FS.base TLS"]
        BOOTFS["0x0000000011000000 : Read-Only boot.tar Window (vfsserv only)"]
        MMAP["0x0000000010000000+ : mmap, shm_open, and libc Heap (W^X)"]
        UELF["0x0000000000400000+ : User ELF (.text R-X, .rodata R--, .data)"]
    end

    KELF --- HHDM --- GAP --- MSTACK --- TSTACK --- BOOTFS --- MMAP --- UELF
```

## Copy-on-Write (`fork` and `VMCloneCow`) Page Faults

`fork` and `VMCloneCow` share private writable pages as read-only until a
process writes to a page. `MAP_SHARED` and `SHM` pages do not use COW, and all
processes that map them can read the writes.

```mermaid
sequenceDiagram
    participant Parent as Parent Process (Ring 3)
    participant VMM as Kernel VMM / PMM (Ring 0)
    participant Child as Child Process (Ring 3)

    Parent->>VMM: fork() / VMCloneCow()
    VMM->>VMM: Set private PTEs read-only with VMM_COW, increase PMM refcounts
    VMM-->>Parent: Give child PID
    VMM-->>Child: Give 0 (read-only shared frames)

    Child->>VMM: Write to shared page -> #PF (Present + Write + User)
    alt PMM frame refcount > 1
        VMM->>VMM: Allocate new frame and make 4 KiB copy through HHDM
        VMM->>VMM: Map new writable PTE in Child and decrease previous refcount
    else PMM frame refcount == 1
        VMM->>VMM: Remove VMM_COW and set PTE writable
    end
    VMM-->>Child: Continue instruction (Parent page stays isolated)
```
