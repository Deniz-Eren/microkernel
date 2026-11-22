/**
 * @file     idt.c
 * @brief    x86-64 IDT setup and exception/interrupt dispatcher
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "idt.h"
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/signal.h>
#include <kernel/vmm.h>
#include "apic.h"

/* need:: COMP_BOOT_0001 */

typedef struct {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_reg_t;

static idt_entry_t g_idt[256] __attribute__((aligned(16)));
static idt_reg_t g_idtr;

extern const void *const isr_stub_table[256];

static void idt_set(unsigned vec, uint64_t handler, uint16_t sel,
                    uint8_t flags) {
    idt_entry_t *e = &g_idt[vec];
    e->offset_lo = (uint16_t)(handler & 0xFFFF);
    e->selector = sel;
    e->ist = 0;
    e->flags = flags;
    e->offset_mid = (uint16_t)((handler >> 16) & 0xFFFF);
    e->offset_hi = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    e->reserved = 0;
}

void idt_init(void) {
    for (unsigned i = 0; i < 256; i++) {
        idt_set(i, (uint64_t)(uintptr_t)isr_stub_table[i], 0x08, 0x8E);
    }
    g_idtr.limit = sizeof(g_idt) - 1;
    g_idtr.base = (uint64_t)g_idt;
    __asm__ volatile("lidt %0" : : "m"(g_idtr) : "memory");
}

static const char *exc_name(unsigned v) {
    static const char *names[32] = {
        "DE divide error",
        "DB debug",
        "NMI",
        "BP breakpoint",
        "OF overflow",
        "BR bound range",
        "UD invalid opcode",
        "NM device n/a",
        "DF double fault",
        "CPOP",
        "TS invalid TSS",
        "NP segment absent",
        "SS stack fault",
        "GP general protection",
        "PF page fault",
        "x87",
        "AC alignment",
        "MC machine check",
        "XM simd fp",
        "VE virtualization",
        "CP control prot",
        "res 21",
        "res 22",
        "res 23",
        "res 24",
        "res 25",
        "res 26",
        "res 27",
        "HV hypervisor",
        "VC vmm comm",
        "SX security",
        "res 31",
    };
    return names[v];
}

static void dump_trap(trapframe_t *tf) {
    printk("\n!!! kernel exception: vec=%llu %s err=0x%llx\n",
           (unsigned long long)tf->vec, exc_name(tf->vec),
           (unsigned long long)tf->err);
    printk("  rip=%llx rsp=%llx rflags=%llx cs=%llx ss=%llx\n",
           (unsigned long long)tf->rip, (unsigned long long)tf->rsp,
           (unsigned long long)tf->rflags, (unsigned long long)tf->cs,
           (unsigned long long)tf->ss);
    printk("  rax=%llx rbx=%llx rcx=%llx rdx=%llx\n",
           (unsigned long long)tf->rax, (unsigned long long)tf->rbx,
           (unsigned long long)tf->rcx, (unsigned long long)tf->rdx);
    printk("  rsi=%llx rdi=%llx rbp=%llx r8=%llx r9=%llx\n",
           (unsigned long long)tf->rsi, (unsigned long long)tf->rdi,
           (unsigned long long)tf->rbp, (unsigned long long)tf->r8,
           (unsigned long long)tf->r9);
    printk("  r10=%llx r11=%llx r12=%llx r13=%llx r14=%llx r15=%llx\n",
           (unsigned long long)tf->r10, (unsigned long long)tf->r11,
           (unsigned long long)tf->r12, (unsigned long long)tf->r13,
           (unsigned long long)tf->r14, (unsigned long long)tf->r15);
    if (tf->vec == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        printk("  cr2(fault addr)=%llx\n", (unsigned long long)cr2);
    }
    printk("!!! system halted\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

void interrupt_handler(trapframe_t *tf) {
    uint64_t vec = tf->vec;
    if (vec == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        if (vmm_handle_page_fault(cr2, tf->err) == 0) {
            signal_deliver_check(tf);
            return;
        }
    }
    if (vec < 32) {
        dump_trap(tf);
    }
    apic_irq_dispatch(vec);
    /* Return-to-user boundary for interrupt frames. If the tick's schedule()
     * switched away, this frame stays parked on the kernel stack and this check
     * runs when the thread finally resumes — g_current is correct in both
     * cases. */
    signal_deliver_check(tf);
}
