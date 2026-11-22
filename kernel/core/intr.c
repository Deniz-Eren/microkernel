/**
 * @file     intr.c
 * @brief    IOAPIC IRQ routing, IRQ-to-IPC pulse delivery, and Ring 3 IOPL
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/acpi.h>
#include <kernel/arch.h>
#include <kernel/intr.h>
#include <kernel/ipc.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include <sys/intr.h>
#include "channel.h"

/* need:: COMP_IPC_0003 */

#define IOAPIC_PHYS_BASE 0xFEC00000ULL
#define IOAPIC_VA 0xFFFFFFFF90001000ULL
#define IOAPIC_REGSEL 0x00
#define IOAPIC_IOWIN 0x10
#define IOAPIC_REG_VER 0x01
#define IOAPIC_REDTBL_BASE 0x10
#define IOAPIC_POLARITY_LOW (1u << 13)
#define IOAPIC_TRIGGER_LEVEL (1u << 15)
#define IOAPIC_MASK_BIT (1u << 16)

#define IRQ_VECTOR_BASE 0x20u
#define MAX_IRQS 24u

typedef struct {
    int attached;
    uint32_t owner_pid;
    int chanid;
    uint32_t pulse_code;
    uint32_t pulse_val;
    chan_msg_t pulse_node; /* pre-allocated: IRQ context never calls kmalloc */
} irq_slot_t;

static volatile uint32_t *g_ioapic;
static uint32_t g_ioapic_max_irq;
static uint32_t g_bsp_lapic_id;
static irq_slot_t g_irq_slots[MAX_IRQS];

static uint32_t ioapic_read(uint32_t reg) {
    g_ioapic[IOAPIC_REGSEL / 4] = reg;
    return g_ioapic[IOAPIC_IOWIN / 4];
}

static void ioapic_write(uint32_t reg, uint32_t val) {
    g_ioapic[IOAPIC_REGSEL / 4] = reg;
    g_ioapic[IOAPIC_IOWIN / 4] = val;
}

static void ioapic_set_entry(uint32_t irq, uint32_t vector, int masked) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t lo = vector & 0xFFu;
    uint16_t iso_flags = acpi_gsi_flags(irq);
    if ((iso_flags & 0x03u) == 0x03u) {
        lo |= IOAPIC_POLARITY_LOW;
    }
    if (((iso_flags >> 2) & 0x03u) == 0x03u) {
        lo |= IOAPIC_TRIGGER_LEVEL;
    }
    if (masked) {
        lo |= IOAPIC_MASK_BIT;
    }
    uint32_t hi = (g_bsp_lapic_id & 0xFFu) << 24;
    ioapic_write(IOAPIC_REDTBL_BASE + 2u * irq + 1u, hi);
    ioapic_write(IOAPIC_REDTBL_BASE + 2u * irq, lo);
}

static void ioapic_mask_irq(uint32_t irq) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t reg = IOAPIC_REDTBL_BASE + 2u * irq;
    ioapic_write(reg, ioapic_read(reg) | IOAPIC_MASK_BIT);
}

static void ioapic_unmask_irq(uint32_t irq) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t reg = IOAPIC_REDTBL_BASE + 2u * irq;
    ioapic_write(reg, ioapic_read(reg) & ~IOAPIC_MASK_BIT);
}

void ioapic_init(void) {
    uint64_t ioapic_phys = acpi_info()->ioapic_phys;
    if (ioapic_phys == 0ULL) {
        ioapic_phys = IOAPIC_PHYS_BASE;
    }
    vmm_map_page(IOAPIC_VA, ioapic_phys,
                 VMM_PRESENT | VMM_WRITE | VMM_PCD | VMM_NX);
    g_ioapic = (volatile uint32_t *)IOAPIC_VA;
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    uint32_t pins = ((ver >> 16) & 0xFFu) + 1u;
    if (pins > MAX_IRQS) {
        pins = MAX_IRQS;
    }
    g_ioapic_max_irq = pins;
    g_bsp_lapic_id = arch_bsp_cpu_id();

    /* Mask all IOAPIC lines until a privileged Ring 3 driver attaches them. */
    for (uint32_t i = 0; i < g_ioapic_max_irq; i++) {
        ioapic_set_entry(i, IRQ_VECTOR_BASE + i, 1);
    }
    printk("[ioapic] mapped @ %llx, %u redirection entries (bsp id=%u)\n",
           (unsigned long long)ioapic_phys, g_ioapic_max_irq, g_bsp_lapic_id);
}

int intr_dispatch_irq(uint64_t vec) {
    if (vec <= IRQ_VECTOR_BASE || vec >= IRQ_VECTOR_BASE + g_ioapic_max_irq) {
        return 0;
    }
    uint32_t irq = (uint32_t)(vec - IRQ_VECTOR_BASE);
    irq_slot_t *slot = &g_irq_slots[irq];
    if (!slot->attached) {
        return 0;
    }
    /* Mask the line until the Ring 3 driver acknowledges and calls IntrUnmask,
     * send LAPIC EOI before any potential preemption, then deliver pulse. */
    ioapic_mask_irq(irq);
    arch_irq_eoi();
    ipc_deliver_irq_pulse(slot->chanid, &slot->pulse_node, slot->pulse_code,
                          slot->pulse_val);
    return 1;
}

long sys_intr_attach_event(uint64_t irq, uint64_t chanid, uint64_t code,
                           uint64_t value) {
    pcb_t *p = proc_current();
    if (!p) {
        return -EPERM;
    }
    if (!(p->caps & CAP_SYS_INTR)) {
        return -EPERM;
    }
    if (irq == 0 || irq >= g_ioapic_max_irq) {
        return -EINVAL;
    }
    channel_t *ch = channel_get((int)chanid);
    if (!ch || ch->owner_pid != p->pid) {
        return -ESRCH;
    }
    irq_slot_t *slot = &g_irq_slots[irq];
    if (slot->attached && slot->owner_pid != p->pid) {
        return -EBUSY;
    }
    slot->attached = 1;
    slot->owner_pid = p->pid;
    slot->chanid = (int)chanid;
    slot->pulse_code = (uint32_t)code;
    slot->pulse_val = (uint32_t)value;
    slot->pulse_node.is_static = 1;
    slot->pulse_node.in_use = 0;

    ioapic_set_entry((uint32_t)irq, IRQ_VECTOR_BASE + (uint32_t)irq, 0);
    return 0;
}

long sys_intr_unmask(uint64_t irq) {
    pcb_t *p = proc_current();
    if (!p || !(p->caps & CAP_SYS_INTR)) {
        return -EPERM;
    }
    if (irq == 0 || irq >= g_ioapic_max_irq) {
        return -EINVAL;
    }
    irq_slot_t *slot = &g_irq_slots[irq];
    if (!slot->attached || slot->owner_pid != p->pid) {
        return -EPERM;
    }
    ioapic_unmask_irq((uint32_t)irq);
    return 0;
}

long sys_iopl_set(trapframe_t *tf, uint64_t level) {
    pcb_t *p = proc_current();
    tcb_t *t = sched_current();
    if (!p || !t) {
        return -EPERM;
    }
    if (level != 0 && level != 3) {
        return -EINVAL;
    }
    if (level == 3 && !(p->caps & CAP_SYS_INTR)) {
        return -EPERM;
    }
    t->iopl = (uint8_t)level;
    tf->rflags = (tf->rflags & ~(3ULL << 12)) | ((level & 3ULL) << 12);
    return 0;
}
