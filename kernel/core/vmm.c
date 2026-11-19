#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/panic.h>

extern char __bss_start[];
extern char __kboot_stack_top[];

#define PAGE_SIZE 4096ULL

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

/* Split a huge leaf entry into a child table of 512 smaller pages.
 * level: 1 = 1 GiB leaf in the PDPT (children are 2 MiB PD entries),
 *        2 = 2 MiB leaf in the PD (children are 4 KiB PT entries). */
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
                (uint64_t)(uintptr_t)__data_end, VMM_PRESENT | VMM_WRITE | VMM_NX);
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
