/**
 * @file     vmm.c
 * @brief    x86-64 four-level PML4 virtual memory manager
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/sched.h>
#include <kernel/string.h>
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/panic.h>
#include <sys/errno.h>
#include <sys/vm.h>

/* need:: COMP_MEM_0002 */

extern char __bss_start[];
extern char __kboot_stack_top[];

#define PAGE_SIZE 4096ULL
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define USER_VA_MIN 0x400000ULL
#define USER_VA_MAX 0x00007FFFFFFFE000ULL
#define USER_MMAP_BASE 0x20000000ULL
#define USER_MMAP_LIMIT 0x50000000ULL
#define VM_MAX_PAGES 16384ULL /* 64 MiB cap per single VMMap */

static uint64_t *table_at(uint64_t phys) {
    return (uint64_t *)phys_to_virt(phys & ~0xFFFULL);
}

uint64_t vmm_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & ~0xFFFULL;
}

void vmm_set_cr3(uint64_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys & ~0xFFFULL) : "memory");
}

void vmm_invlpg(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Split a huge leaf entry into a child table of 512 smaller pages. Level 1 is a
 * 1 GiB leaf in PDPT (children are 2 MiB PD entries); level 2 is a 2 MiB leaf
 * in PD (children are 4 KiB PT entries). */
static uint64_t split_leaf(uint64_t *table, uint64_t index, int level) {
    uint64_t leaf = table[index];
    if (!(leaf & VMM_PRESENT) || !(leaf & VMM_HUGE)) {
        return 0;
    }
    uint64_t base_phys = leaf & 0x000FFFFFFFFFF000ULL;
    uint64_t flags = leaf & ~(VMM_HUGE | 0xFFFULL); /* keep NX, PWT, PCD... */
    uint64_t child_size = (level == 1) ? (2ULL << 20) : PAGE_SIZE;
    uint64_t t_phys = pmm_alloc_frame();
    if (!t_phys) {
        panic("vmm: out of frames splitting huge page");
    }
    uint64_t *t = table_at(t_phys);
    for (int i = 0; i < 512; i++) {
        t[i] = (base_phys + (uint64_t)i * child_size) | flags;
    }
    table[index] = t_phys | (leaf & (0xFFFULL & ~VMM_HUGE));
    return t_phys;
}

static uint64_t *walk_pml4(uint64_t pml4_phys, uint64_t vaddr, int alloc,
                           uint64_t userflag) {
    uint64_t *pml4 = table_at(pml4_phys);

    uint64_t *levels[4] = {pml4, NULL, NULL, NULL};
    unsigned idxs[3] = {
        (vaddr >> 39) & 0x1FF,
        (vaddr >> 30) & 0x1FF,
        (vaddr >> 21) & 0x1FF,
    };
    uint64_t flags = VMM_PRESENT | VMM_WRITE | userflag;

    for (int l = 0; l < 3; l++) {
        uint64_t ent = levels[l][idxs[l]];
        if (ent & VMM_HUGE) {
            uint64_t t_phys = split_leaf(levels[l], idxs[l], l == 1 ? 1 : 2);
            if (!t_phys) {
                return NULL;
            }
            levels[l + 1] = table_at(t_phys);
            continue;
        }
        if (!(ent & VMM_PRESENT)) {
            if (!alloc) {
                return NULL;
            }
            uint64_t t = pmm_alloc_frame();
            if (!t) {
                panic("vmm: out of frames during walk");
            }
            uint64_t *tbl = table_at(t);
            for (int i = 0; i < 512; i++) {
                tbl[i] = 0;
            }
            levels[l][idxs[l]] = t | flags;
            levels[l + 1] = tbl;
        } else {
            levels[l + 1] = table_at(ent);
        }
    }
    return levels[3]; /* PT */
}

int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    return vmm_map_page_to(vmm_get_cr3(), vaddr, paddr, flags);
}

int vmm_map_page_to(uint64_t pml4_phys, uint64_t vaddr, uint64_t paddr,
                    uint64_t flags) {
    if ((flags & VMM_WRITE) && !(flags & VMM_NX)) {
        panic("vmm: W^X violation: writable+executable map of %llx",
              (unsigned long long)vaddr);
    }
    if (vaddr & 0xFFF || paddr & 0xFFF) {
        return -1;
    }
    uint64_t *pt = walk_pml4(pml4_phys, vaddr, 1, flags & VMM_USER);
    if (!pt) {
        return -1;
    }
    unsigned i1 = (vaddr >> 12) & 0x1FF;
    pt[i1] = (paddr & ~0xFFFULL) | flags | VMM_PRESENT;
    vmm_invlpg(vaddr);
    return 0;
}

int vmm_unmap_page(uint64_t vaddr) {
    uint64_t *pt = walk_pml4(vmm_get_cr3(), vaddr, 0, 0);
    if (!pt) {
        return -1;
    }
    unsigned i1 = (vaddr >> 12) & 0x1FF;
    pt[i1] = 0;
    vmm_invlpg(vaddr);
    return 0;
}

uint64_t vmm_unmap_page_to(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t *pt = walk_pml4(pml4_phys, vaddr, 0, 0);
    if (!pt) {
        return 0;
    }
    unsigned i1 = (vaddr >> 12) & 0x1FF;
    uint64_t ent = pt[i1];
    if (!(ent & VMM_PRESENT)) {
        return 0;
    }
    pt[i1] = 0;
    if (pml4_phys == vmm_get_cr3()) {
        vmm_invlpg(vaddr);
    }
    return ent & PTE_ADDR_MASK;
}

uint64_t vmm_get_phys(uint64_t vaddr) {
    return vmm_get_phys_to(vmm_get_cr3(), vaddr);
}

/* Raw leaf PTE for va in the given address space, or 0 if not mapped. */
uint64_t vmm_get_pte_to(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t *pt = walk_pml4(pml4_phys, vaddr, 0, 0);
    if (!pt) {
        return 0;
    }
    return pt[(vaddr >> 12) & 0x1FF];
}

uint64_t vmm_get_phys_to(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t *pt = walk_pml4(pml4_phys, vaddr, 0, 0);
    if (!pt) {
        return 0;
    }
    unsigned i1 = (vaddr >> 12) & 0x1FF;
    uint64_t ent = pt[i1];
    if (!(ent & VMM_PRESENT)) {
        return 0;
    }
    if (ent & VMM_HUGE) {
        return (ent & 0x000FFFFFFFFFF000ULL) + (vaddr & 0x1FFFFF);
    }
    return (ent & 0x000FFFFFFFFFF000ULL) + (vaddr & 0xFFF);
}

/* Create a fresh user PML4 sharing the kernel's higher-half entries. */
uint64_t vmm_create_user_pml4(void) {
    uint64_t frame = pmm_alloc_frame();
    if (!frame) {
        return 0;
    }
    uint64_t *pml4 = table_at(frame);
    uint64_t *cur = table_at(vmm_get_cr3());
    for (int i = 0; i < 256; i++) {
        pml4[i] = 0; /* user half starts empty (null-page guard included) */
    }
    for (int i = 256; i < 512; i++) {
        pml4[i] = cur[i]; /* kernel: HHDM, heap, kernel image */
    }
    return frame;
}

/* Reclaim all user-half page tables and leaf frames (refcount-safe). */
void vmm_destroy_user_pml4(uint64_t pml4_phys) {
    if (!pml4_phys || pml4_phys == vmm_get_cr3()) {
        return;
    }
    uint64_t *pml4 = table_at(pml4_phys);
    for (int i4 = 0; i4 < 256; i4++) {
        if (!(pml4[i4] & VMM_PRESENT)) {
            continue;
        }
        uint64_t pdpt_phys = pml4[i4] & PTE_ADDR_MASK;
        uint64_t *pdpt = table_at(pdpt_phys);
        for (int i3 = 0; i3 < 512; i3++) {
            if (!(pdpt[i3] & VMM_PRESENT) || (pdpt[i3] & VMM_HUGE)) {
                continue;
            }
            uint64_t pd_phys = pdpt[i3] & PTE_ADDR_MASK;
            uint64_t *pd = table_at(pd_phys);
            for (int i2 = 0; i2 < 512; i2++) {
                if (!(pd[i2] & VMM_PRESENT) || (pd[i2] & VMM_HUGE)) {
                    continue;
                }
                uint64_t pt_phys = pd[i2] & PTE_ADDR_MASK;
                uint64_t *pt = table_at(pt_phys);
                for (int i1 = 0; i1 < 512; i1++) {
                    if (pt[i1] & VMM_PRESENT) {
                        pmm_free_frame(pt[i1] & PTE_ADDR_MASK);
                    }
                }
                pmm_free_frame(pt_phys);
            }
            pmm_free_frame(pd_phys);
        }
        pmm_free_frame(pdpt_phys);
    }
    pmm_free_frame(pml4_phys);
}

int vmm_resolve_cow_to(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t page = vaddr & ~0xFFFULL;
    uint64_t *pt = walk_pml4(pml4_phys, page, 0, 0);
    if (!pt) {
        return -1;
    }
    unsigned i1 = (page >> 12) & 0x1FF;
    uint64_t pte = pt[i1];
    if (!(pte & VMM_PRESENT) || !(pte & VMM_USER) || !(pte & VMM_COW)) {
        return -1;
    }
    uint64_t new_flags = (pte & ~(PTE_ADDR_MASK | VMM_COW)) | VMM_WRITE;
    if (!(new_flags & VMM_NX)) {
        return -1; /* W^X invariant: never make executable page writable */
    }
    uint64_t old_phys = pte & PTE_ADDR_MASK;
    if (pmm_frame_refcount(old_phys) <= 1) {
        pt[i1] = old_phys | new_flags;
    } else {
        uint64_t new_phys = pmm_alloc_frame();
        if (!new_phys) {
            return -1;
        }
        memcpy(phys_to_virt(new_phys), phys_to_virt(old_phys), PAGE_SIZE);
        pt[i1] = new_phys | new_flags;
        pmm_free_frame(old_phys);
    }
    if (pml4_phys == vmm_get_cr3()) {
        vmm_invlpg(page);
    }
    return 0;
}

int vmm_handle_page_fault(uint64_t fault_va, uint64_t err) {
    /* Only handle Ring 3 protection-violation writes on present COW pages. */
    if ((err & 0x7ULL) != 0x7ULL) {
        return -1;
    }
    if (fault_va >= 0x0000800000000000ULL) {
        return -1;
    }
    return vmm_resolve_cow_to(vmm_get_cr3(), fault_va);
}

static int range_is_unmapped(uint64_t pml4_phys, uint64_t base,
                             uint64_t npages) {
    for (uint64_t i = 0; i < npages; i++) {
        if (vmm_get_pte_to(pml4_phys, base + i * PAGE_SIZE) & VMM_PRESENT) {
            return 0;
        }
    }
    return 1;
}

static uint64_t find_mmap_region(pcb_t *p, uint64_t npages) {
    uint64_t bytes = npages * PAGE_SIZE;
    if (p->next_mmap_va < USER_MMAP_BASE ||
        p->next_mmap_va >= USER_MMAP_LIMIT) {
        p->next_mmap_va = USER_MMAP_BASE;
    }
    uint64_t start = p->next_mmap_va;
    for (int pass = 0; pass < 2; pass++) {
        uint64_t va = (pass == 0) ? start : USER_MMAP_BASE;
        uint64_t end = (pass == 0) ? USER_MMAP_LIMIT : start;
        while (va + bytes <= end) {
            if (range_is_unmapped(p->pml4_phys, va, npages)) {
                p->next_mmap_va = va + bytes;
                return va;
            }
            va += PAGE_SIZE;
        }
    }
    return 0;
}

static uint64_t prot_to_pte_flags(uint64_t prot) {
    uint64_t flags = VMM_PRESENT;
    if (prot != VM_PROT_NONE) {
        flags |= VMM_USER;
    }
    if (prot & VM_PROT_WRITE) {
        flags |= VMM_WRITE;
    }
    if (!(prot & VM_PROT_EXEC)) {
        flags |= VMM_NX;
    }
    return flags;
}

long sys_vm_map(uint64_t va_hint, uint64_t len, uint64_t prot, uint64_t flags) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (len == 0 ||
        (prot & ~(uint64_t)(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC))) {
        return -EINVAL;
    }
    if ((prot & VM_PROT_WRITE) && (prot & VM_PROT_EXEC)) {
        return -EACCES; /* W^X policy */
    }
    if (flags & ~(uint64_t)(VM_MAP_PRIVATE | VM_MAP_ANON | VM_MAP_FIXED)) {
        return -EINVAL;
    }
    uint64_t npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages == 0 || npages > VM_MAX_PAGES) {
        return -ENOMEM;
    }
    uint64_t bytes = npages * PAGE_SIZE;
    uint64_t base = 0;

    if (va_hint != 0) {
        if ((va_hint & 0xFFFULL) != 0 || va_hint < USER_VA_MIN ||
            va_hint > USER_MMAP_LIMIT || bytes > USER_MMAP_LIMIT - va_hint) {
            return -EINVAL;
        }
        if (flags & VM_MAP_FIXED) {
            for (uint64_t i = 0; i < npages; i++) {
                uint64_t old =
                    vmm_unmap_page_to(p->pml4_phys, va_hint + i * PAGE_SIZE);
                if (old) {
                    pmm_free_frame(old);
                }
            }
            base = va_hint;
        } else if (range_is_unmapped(p->pml4_phys, va_hint, npages)) {
            base = va_hint;
        }
    }
    if (!base) {
        if (flags & VM_MAP_FIXED) {
            return -EINVAL;
        }
        base = find_mmap_region(p, npages);
        if (!base) {
            return -ENOMEM;
        }
    }

    uint64_t pte_flags = prot_to_pte_flags(prot);
    for (uint64_t i = 0; i < npages; i++) {
        uint64_t frame = pmm_alloc_frame();
        if (!frame) {
            for (uint64_t j = 0; j < i; j++) {
                uint64_t old =
                    vmm_unmap_page_to(p->pml4_phys, base + j * PAGE_SIZE);
                if (old) {
                    pmm_free_frame(old);
                }
            }
            return -ENOMEM;
        }
        memset(phys_to_virt(frame), 0, PAGE_SIZE);
        if (vmm_map_page_to(p->pml4_phys, base + i * PAGE_SIZE, frame,
                            pte_flags) != 0) {
            pmm_free_frame(frame);
            for (uint64_t j = 0; j < i; j++) {
                uint64_t old =
                    vmm_unmap_page_to(p->pml4_phys, base + j * PAGE_SIZE);
                if (old) {
                    pmm_free_frame(old);
                }
            }
            return -ENOMEM;
        }
    }
    return (long)base;
}

long sys_vm_unmap(uint64_t va, uint64_t len) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (len == 0 || (va & 0xFFFULL) != 0 || va < USER_VA_MIN ||
        va > USER_VA_MAX) {
        return -EINVAL;
    }
    uint64_t npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages == 0 || npages > (USER_VA_MAX - va) / PAGE_SIZE) {
        return -EINVAL;
    }
    for (uint64_t i = 0; i < npages; i++) {
        uint64_t old = vmm_unmap_page_to(p->pml4_phys, va + i * PAGE_SIZE);
        if (old) {
            pmm_free_frame(old);
        }
    }
    return 0;
}

long sys_vm_protect(uint64_t va, uint64_t len, uint64_t prot) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (len == 0 || (va & 0xFFFULL) != 0 || va < USER_VA_MIN ||
        va > USER_VA_MAX) {
        return -EINVAL;
    }
    if (prot & ~(uint64_t)(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC)) {
        return -EINVAL;
    }
    if ((prot & VM_PROT_WRITE) && (prot & VM_PROT_EXEC)) {
        return -EACCES; /* W^X policy */
    }
    uint64_t npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages == 0 || npages > (USER_VA_MAX - va) / PAGE_SIZE) {
        return -EINVAL;
    }
    for (uint64_t i = 0; i < npages; i++) {
        if (!(vmm_get_pte_to(p->pml4_phys, va + i * PAGE_SIZE) & VMM_PRESENT)) {
            return -ENOMEM;
        }
    }
    uint64_t want_flags = prot_to_pte_flags(prot);
    for (uint64_t i = 0; i < npages; i++) {
        uint64_t page = va + i * PAGE_SIZE;
        uint64_t *pt = walk_pml4(p->pml4_phys, page, 0, 0);
        if (!pt) {
            return -ENOMEM;
        }
        unsigned i1 = (page >> 12) & 0x1FF;
        uint64_t pte = pt[i1];
        uint64_t phys = pte & PTE_ADDR_MASK;
        uint64_t nflags = want_flags;
        if ((pte & VMM_COW) && (prot & VM_PROT_WRITE)) {
            nflags = (nflags & ~VMM_WRITE) | VMM_COW;
        }
        pt[i1] = phys | nflags;
        vmm_invlpg(page);
    }
    return 0;
}

static int clone_cow_page(uint64_t src_pml4, uint64_t dst_pml4, uint64_t page) {
    uint64_t *src_pt = walk_pml4(src_pml4, page, 0, 0);
    if (!src_pt) {
        return 0;
    }
    unsigned i1 = (page >> 12) & 0x1FF;
    uint64_t pte = src_pt[i1];
    if (!(pte & VMM_PRESENT)) {
        return 0;
    }
    uint64_t phys = pte & PTE_ADDR_MASK;
    uint64_t flags = pte & ~PTE_ADDR_MASK;
    if (flags & VMM_WRITE) {
        flags = (flags & ~VMM_WRITE) | VMM_COW;
        src_pt[i1] = phys | flags;
        if (src_pml4 == vmm_get_cr3()) {
            vmm_invlpg(page);
        }
    }
    pmm_frame_inc(phys);
    uint64_t old = vmm_unmap_page_to(dst_pml4, page);
    if (old) {
        pmm_free_frame(old);
    }
    if (vmm_map_page_to(dst_pml4, page, phys, flags) != 0) {
        pmm_free_frame(phys);
        return -ENOMEM;
    }
    return 0;
}

long sys_vm_clone_cow(uint64_t dst_pid, uint64_t va, uint64_t len) {
    pcb_t *src = proc_current();
    if (!src) {
        return -EPERM;
    }
    pcb_t *dst = proc_find((uint32_t)dst_pid);
    if (!dst || dst == src || dst->state != PROC_ALIVE) {
        return -ESRCH;
    }
    if (dst->parent_pid != src->pid && src->parent_pid != dst->pid) {
        return -EPERM;
    }

    if (va == 0 && len == 0) {
        /* Clone entire lower-half user address space COW. */
        uint64_t *pml4 = table_at(src->pml4_phys);
        for (int i4 = 0; i4 < 256; i4++) {
            if (!(pml4[i4] & VMM_PRESENT)) {
                continue;
            }
            uint64_t *pdpt = table_at(pml4[i4]);
            for (int i3 = 0; i3 < 512; i3++) {
                if (!(pdpt[i3] & VMM_PRESENT) || (pdpt[i3] & VMM_HUGE)) {
                    continue;
                }
                uint64_t *pd = table_at(pdpt[i3]);
                for (int i2 = 0; i2 < 512; i2++) {
                    if (!(pd[i2] & VMM_PRESENT) || (pd[i2] & VMM_HUGE)) {
                        continue;
                    }
                    uint64_t *pt = table_at(pd[i2]);
                    for (int i1 = 0; i1 < 512; i1++) {
                        if (!(pt[i1] & VMM_PRESENT)) {
                            continue;
                        }
                        uint64_t page =
                            ((uint64_t)i4 << 39) | ((uint64_t)i3 << 30) |
                            ((uint64_t)i2 << 21) | ((uint64_t)i1 << 12);
                        int rc = clone_cow_page(src->pml4_phys, dst->pml4_phys,
                                                page);
                        if (rc < 0) {
                            return rc;
                        }
                    }
                }
            }
        }
        return 0;
    }

    if (len == 0 || (va & 0xFFFULL) != 0 || va < USER_VA_MIN ||
        va > USER_VA_MAX) {
        return -EINVAL;
    }
    uint64_t npages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    if (npages == 0 || npages > (USER_VA_MAX - va) / PAGE_SIZE) {
        return -EINVAL;
    }
    for (uint64_t i = 0; i < npages; i++) {
        int rc =
            clone_cow_page(src->pml4_phys, dst->pml4_phys, va + i * PAGE_SIZE);
        if (rc < 0) {
            return rc;
        }
    }
    return 0;
}

/* Map a whole range of kernel image pages with per-section permissions. */
static void remap_range(const char *name, uint64_t vstart, uint64_t vend,
                        uint64_t flags) {
    vstart &= ~0xFFFULL;
    vend = (vend + 0xFFF) & ~0xFFFULL;
    for (uint64_t v = vstart; v < vend; v += PAGE_SIZE) {
        uint64_t p = kern_virt_to_phys(v);
        if (vmm_map_page(v, p, flags) != 0) {
            panic("vmm: failed remapping %s page at %llx", name,
                  (unsigned long long)v);
        }
    }
    printk("[vmm] %s: %llx-%llx -> %llx-%llx (%s)\n", name,
           (unsigned long long)vstart, (unsigned long long)vend,
           (unsigned long long)kern_virt_to_phys(vstart),
           (unsigned long long)kern_virt_to_phys(vend - 1),
           (flags & VMM_WRITE) ? "RW+NX" : ((flags & VMM_NX) ? "R+NX" : "RX"));
}

void vmm_init(void) {
    uint64_t pml4_phys = vmm_get_cr3();
    uint64_t *pml4 = table_at(pml4_phys);

    /* Fine-grained W^X-compliant remap of the kernel image. */
    extern char __text_start[], __text_end[];
    extern char __rodata_start[], __rodata_end[];
    extern char __data_start[], __data_end[];
    extern char __bss_end[];
    remap_range("text", (uint64_t)(uintptr_t)__text_start,
                (uint64_t)(uintptr_t)__text_end, VMM_PRESENT); /* RX */
    remap_range("rodata", (uint64_t)(uintptr_t)__rodata_start,
                (uint64_t)(uintptr_t)__rodata_end, VMM_PRESENT | VMM_NX);
    remap_range("data", (uint64_t)(uintptr_t)__data_start,
                (uint64_t)(uintptr_t)__data_end,
                VMM_PRESENT | VMM_WRITE | VMM_NX);
    remap_range("bss", (uint64_t)(uintptr_t)__bss_start,
                (uint64_t)(uintptr_t)__kboot_stack_top,
                VMM_PRESENT | VMM_WRITE | VMM_NX);

    /* NX the remaining coarse 2 MiB kernel-window entries (data regions).
     * Only still-huge leaves: split entries point at PTs and must stay X. */
    unsigned i4 = (KERNEL_VIRT_BASE >> 39) & 0x1FF;
    unsigned i3 = (KERNEL_VIRT_BASE >> 30) & 0x1FF;
    uint64_t *pdpt_k = table_at(pml4[i4]);
    uint64_t *pd_k = table_at(pdpt_k[i3]);
    for (int i = 0; i < 512; i++) {
        if ((pd_k[i] & VMM_PRESENT) && (pd_k[i] & VMM_HUGE)) {
            pd_k[i] |= VMM_NX;
        }
    }
    /* NX all HHDM/identity PD entries (pure data mappings). */
    uint64_t *pdpt_hh = table_at(pml4[256]);
    for (int i = 0; i < 512; i++) {
        if (pdpt_hh[i] & VMM_PRESENT) {
            uint64_t *pd = table_at(pdpt_hh[i]);
            for (int j = 0; j < 512; j++) {
                if (pd[j] & VMM_PRESENT) {
                    pd[j] |= VMM_NX;
                }
            }
        }
    }

    /* Drop the boot identity map of low memory (PML4[0]). */
    pml4[0] = 0;

    /* Enable SMEP/SMAP and flush the whole TLB via CR3 reload. */
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 20) | (1ULL << 21); /* SMEP | SMAP */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");

    vmm_set_cr3(pml4_phys);
    printk("[vmm] kernel PML4 @ %llx, HHDM off=0x%llx, identity map dropped\n",
           (unsigned long long)pml4_phys, (unsigned long long)HHDM_OFFSET);
}
