// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once

/* need:: ICD_MEM_0001 */
#include <stddef.h>
#include <stdint.h>

/* Higher-half direct map: virt = phys + HHDM_OFFSET (valid for all RAM). */
#define HHDM_OFFSET 0xFFFF800000000000ULL

/* PTE flag bits (raw x86 page-table bits). */
#define VMM_PRESENT (1ULL << 0)
#define VMM_WRITE   (1ULL << 1)
#define VMM_USER    (1ULL << 2)
#define VMM_PWT     (1ULL << 3)
#define VMM_PCD     (1ULL << 4)
#define VMM_HUGE    (1ULL << 7)
#define VMM_GLOBAL  (1ULL << 8)
#define VMM_NX      (1ULL << 63)

static inline void *phys_to_virt(uint64_t p) {
    return (void *)(uintptr_t)(p + HHDM_OFFSET);
}

static inline uint64_t virt_to_phys(const void *v) {
    return (uint64_t)(uintptr_t)v - HHDM_OFFSET;
}

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define KERNEL_PHYS_BASE 0x00100000ULL
#define KERNEL_HEAP_BASE 0xFFFFC00000000000ULL

static inline uint64_t kern_virt_to_phys(uint64_t vaddr) {
    return vaddr - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
}

void vmm_init(void);
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);
int vmm_map_page_to(uint64_t pml4_phys, uint64_t vaddr, uint64_t paddr,
                    uint64_t flags);
int vmm_unmap_page(uint64_t vaddr);
uint64_t vmm_get_phys(uint64_t vaddr); /* 0 if not mapped */
uint64_t vmm_get_phys_to(uint64_t pml4_phys, uint64_t vaddr);
uint64_t vmm_get_pte_to(uint64_t pml4_phys, uint64_t vaddr); /* raw PTE or 0 */
uint64_t vmm_create_user_pml4(void);
void vmm_set_cr3(uint64_t pml4_phys);
uint64_t vmm_get_cr3(void);
void vmm_invlpg(uint64_t vaddr);
