// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include "gdt.h"
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/string.h>

/*
 * 64-bit GDT. Layout:
 *   0x00 null
 *   0x08 kernel code (L=1)
 *   0x10 kernel data
 *   0x18 user data   (DPL=3)  <- precedes user code for SYSRET
 *   0x20 user code   (L=1, DPL=3)
 *   0x28 TSS (16 bytes, installed later)
 */
static uint64_t g_gdt[8] __attribute__((aligned(16)));
static tss_t g_tss __attribute__((aligned(16)));

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdtr g_gdtr;

void gdt_init(void) {
    g_gdt[0] = 0x0000000000000000ULL;
    g_gdt[1] = 0x00AF9A000000FFFFULL; /* 64-bit kernel code */
    g_gdt[2] = 0x00CF92000000FFFFULL; /* kernel data */
    g_gdt[3] = 0x00CFF2000000FFFFULL; /* user data */
    g_gdt[4] = 0x00AFFA000000FFFFULL; /* 64-bit user code */
    g_gdt[5] = 0;
    g_gdt[6] = 0;
    g_gdt[7] = 0;

    g_gdtr.limit = sizeof(g_gdt) - 1;
    g_gdtr.base = (uint64_t)g_gdt;

    __asm__ volatile("lgdt %0" : : "m"(g_gdtr) : "memory");

    /* Reload CS with a far return; reload data segments. */
    __asm__ volatile(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        :
        : "rax", "memory");

    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        :
        :
        : "ax", "memory");
}

void tss_init(void) {
    memset(&g_tss, 0, sizeof(g_tss));
    g_tss.iopb_offset = sizeof(g_tss); /* no I/O bitmap */
    gdt_install_tss((uint64_t)(uintptr_t)&g_tss, sizeof(g_tss) - 1);
}

void tss_set_rsp0(uint64_t rsp0) {
    g_tss.rsp0 = rsp0;
}

void gdt_install_tss(uint64_t tss_vaddr, uint32_t tss_limit) {
    /* Two-slot system descriptor: 16 bytes spanning g_gdt[5..6]. */
    uint64_t base = tss_vaddr;
    /* Access byte (P=1, DPL=0, type 0x9 = 64-bit available TSS) at bits 40-47. */
    uint64_t lo = 0x0000890000000000ULL;
    lo |= (tss_limit & 0xFFFFULL);
    lo |= ((base & 0xFFFFFFULL) << 16);
    lo |= (((base >> 24) & 0xFFULL) << 56);
    lo |= ((tss_limit & 0xF0000ULL) << 32);
    uint64_t hi = ((base >> 32) & 0xFFFFFFFFULL);

    g_gdt[5] = lo;
    g_gdt[6] = hi;

    __asm__ volatile("lgdt %0" : : "m"(g_gdtr) : "memory");
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS) : "memory");
    printk("[gdt] TSS installed at %p\n", (void *)tss_vaddr);
}
