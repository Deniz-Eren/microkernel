/**
 * @file     gdt.c
 * @brief    x86-64 per-CPU GDT and TSS initialization for Ring 0 and Ring 3
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "gdt.h"
#include <stddef.h>
#include <kernel/arch.h>
#include <kernel/printk.h>
#include <kernel/smp.h>
#include <kernel/string.h>

/* need:: COMP_BOOT_0001 COMP_PROC_0008 */

/*
 * 64-bit per-CPU GDT layout: 0x00 null, 0x08 kernel code (L=1), 0x10 kernel
 * data, 0x18 user data (DPL=3, precedes user code for SYSRET), 0x20 user code
 * (L=1, DPL=3), 0x28 TSS (16 bytes, installed per CPU).
 */
static uint64_t g_gdt[SMP_MAX_CPUS][8] __attribute__((aligned(16)));
static tss_t g_tss[SMP_MAX_CPUS] __attribute__((aligned(16)));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdtr g_gdtr[SMP_MAX_CPUS];

void gdt_init_cpu(uint32_t cpu_idx) {
    if (cpu_idx >= SMP_MAX_CPUS) {
        return;
    }
    uint64_t *gdt = g_gdt[cpu_idx];
    gdt[0] = 0x0000000000000000ULL;
    gdt[1] = 0x00AF9A000000FFFFULL; /* 64-bit kernel code */
    gdt[2] = 0x00CF92000000FFFFULL; /* kernel data */
    gdt[3] = 0x00CFF2000000FFFFULL; /* user data */
    gdt[4] = 0x00AFFA000000FFFFULL; /* 64-bit user code */
    gdt[5] = 0;
    gdt[6] = 0;
    gdt[7] = 0;

    g_gdtr[cpu_idx].limit = (uint16_t)(sizeof(g_gdt[cpu_idx]) - 1u);
    g_gdtr[cpu_idx].base = (uint64_t)(uintptr_t)gdt;

    __asm__ volatile("lgdt %0" : : "m"(g_gdtr[cpu_idx]) : "memory");

    /* Reload CS with a far return; reload data segments. */
    __asm__ volatile("pushq $0x08\n"
                     "leaq 1f(%%rip), %%rax\n"
                     "pushq %%rax\n"
                     "lretq\n"
                     "1:\n"
                     :
                     :
                     : "rax", "memory");

    __asm__ volatile("mov $0x10, %%ax\n"
                     "mov %%ax, %%ds\n"
                     "mov %%ax, %%es\n"
                     "mov %%ax, %%ss\n"
                     "mov %%ax, %%fs\n"
                     "mov %%ax, %%gs\n"
                     :
                     :
                     : "ax", "memory");
}

void gdt_init(void) {
    gdt_init_cpu(0u);
}

void tss_init_cpu(uint32_t cpu_idx) {
    if (cpu_idx >= SMP_MAX_CPUS) {
        return;
    }
    tss_t *tss = &g_tss[cpu_idx];
    memset(tss, 0, sizeof(*tss));
    tss->iopb_offset = sizeof(*tss); /* no I/O bitmap */

    uint64_t base = (uint64_t)(uintptr_t)tss;
    uint32_t limit = (uint32_t)(sizeof(*tss) - 1u);
    uint64_t lo = 0x0000890000000000ULL;
    lo |= (limit & 0xFFFFULL);
    lo |= ((base & 0xFFFFFFULL) << 16);
    lo |= (((base >> 24) & 0xFFULL) << 56);
    lo |= ((limit & 0xF0000ULL) << 32);
    uint64_t hi = ((base >> 32) & 0xFFFFFFFFULL);

    g_gdt[cpu_idx][5] = lo;
    g_gdt[cpu_idx][6] = hi;

    __asm__ volatile("lgdt %0" : : "m"(g_gdtr[cpu_idx]) : "memory");
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS) : "memory");
    if (cpu_idx == 0u) {
        printk("[gdt] TSS installed at %p\n", (void *)tss);
    }
}

void tss_init(void) {
    tss_init_cpu(0u);
}

void tss_set_rsp0_cpu(uint32_t cpu_idx, uint64_t rsp0) {
    if (cpu_idx < SMP_MAX_CPUS) {
        g_tss[cpu_idx].rsp0 = rsp0;
    }
}

void tss_set_rsp0(uint64_t rsp0) {
    tss_set_rsp0_cpu(arch_cpu_index(), rsp0);
}
