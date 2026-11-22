/**
 * @file     intr.c
 * @brief    SMP-safe IOAPIC IRQ routing, IRQ-to-IPC pulses, and PCI config
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
#include <kernel/spinlock.h>
#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include <sys/intr.h>
#include "channel.h"

/* need:: COMP_IPC_0003 COMP_IPC_0015 COMP_PROC_0008 */

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

static spinlock_t g_intr_lock = SPINLOCK_INIT;
static volatile uint32_t *g_ioapic;
static uint32_t g_ioapic_max_irq;
static uint32_t g_bsp_lapic_id;
static irq_slot_t g_irq_slots[MAX_IRQS];

static uint32_t ioapic_read_locked(uint32_t reg) {
    g_ioapic[IOAPIC_REGSEL / 4] = reg;
    return g_ioapic[IOAPIC_IOWIN / 4];
}

static void ioapic_write_locked(uint32_t reg, uint32_t val) {
    g_ioapic[IOAPIC_REGSEL / 4] = reg;
    g_ioapic[IOAPIC_IOWIN / 4] = val;
}

static void ioapic_set_entry_locked(uint32_t irq, uint32_t vector, int masked) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t lo = vector & 0xFFu;
    uint16_t iso_flags = acpi_gsi_flags(irq);
    if ((iso_flags & 0x03u) == 0x03u || (iso_flags == 0u && irq >= 16u)) {
        lo |= IOAPIC_POLARITY_LOW;
    }
    if (((iso_flags >> 2) & 0x03u) == 0x03u ||
        (iso_flags == 0u && irq >= 16u)) {
        lo |= IOAPIC_TRIGGER_LEVEL;
    }
    if (masked) {
        lo |= IOAPIC_MASK_BIT;
    }
    uint32_t hi = (g_bsp_lapic_id & 0xFFu) << 24;
    ioapic_write_locked(IOAPIC_REDTBL_BASE + 2u * irq + 1u, hi);
    ioapic_write_locked(IOAPIC_REDTBL_BASE + 2u * irq, lo);
}

static void ioapic_mask_irq_locked(uint32_t irq) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t reg = IOAPIC_REDTBL_BASE + 2u * irq;
    ioapic_write_locked(reg, ioapic_read_locked(reg) | IOAPIC_MASK_BIT);
}

static void ioapic_unmask_irq_locked(uint32_t irq) {
    if (!g_ioapic || irq >= g_ioapic_max_irq) {
        return;
    }
    uint32_t reg = IOAPIC_REDTBL_BASE + 2u * irq;
    ioapic_write_locked(reg, ioapic_read_locked(reg) & ~IOAPIC_MASK_BIT);
}

void ioapic_init(void) {
    uint64_t ioapic_phys = acpi_info()->ioapic_phys;
    if (ioapic_phys == 0ULL) {
        ioapic_phys = IOAPIC_PHYS_BASE;
    }
    vmm_map_page(IOAPIC_VA, ioapic_phys,
                 VMM_PRESENT | VMM_WRITE | VMM_PCD | VMM_NX);
    g_ioapic = (volatile uint32_t *)IOAPIC_VA;
    uint32_t ver = ioapic_read_locked(IOAPIC_REG_VER);
    uint32_t pins = ((ver >> 16) & 0xFFu) + 1u;
    if (pins > MAX_IRQS) {
        pins = MAX_IRQS;
    }
    g_ioapic_max_irq = pins;
    g_bsp_lapic_id = arch_bsp_cpu_id();

    /* Mask all IOAPIC lines until a privileged Ring 3 driver attaches them. */
    for (uint32_t i = 0; i < g_ioapic_max_irq; i++) {
        ioapic_set_entry_locked(i, IRQ_VECTOR_BASE + i, 1);
    }
    printk("[ioapic] mapped @ %llx, %u redirection entries (bsp id=%u)\n",
           (unsigned long long)ioapic_phys, g_ioapic_max_irq, g_bsp_lapic_id);
}

int intr_dispatch_irq(uint64_t vec) {
    if (vec <= IRQ_VECTOR_BASE || vec >= IRQ_VECTOR_BASE + g_ioapic_max_irq) {
        return 0;
    }
    uint32_t irq = (uint32_t)(vec - IRQ_VECTOR_BASE);
    uint64_t flags = spin_lock_irqsave(&g_intr_lock);
    irq_slot_t *slot = &g_irq_slots[irq];
    if (!slot->attached) {
        spin_unlock_irqrestore(&g_intr_lock, flags);
        return 0;
    }
    int chanid = slot->chanid;
    uint32_t code = slot->pulse_code;
    uint32_t val = slot->pulse_val;
    chan_msg_t *node = &slot->pulse_node;
    ioapic_mask_irq_locked(irq);
    spin_unlock_irqrestore(&g_intr_lock, flags);

    arch_irq_eoi();
    ipc_deliver_irq_pulse(chanid, node, code, val);
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

    uint64_t flags = spin_lock_irqsave(&g_intr_lock);
    irq_slot_t *slot = &g_irq_slots[irq];
    if (slot->attached && slot->owner_pid != p->pid) {
        spin_unlock_irqrestore(&g_intr_lock, flags);
        return -EBUSY;
    }
    slot->attached = 1;
    slot->owner_pid = p->pid;
    slot->chanid = (int)chanid;
    slot->pulse_code = (uint32_t)code;
    slot->pulse_val = (uint32_t)value;
    slot->pulse_node.is_static = 1;
    slot->pulse_node.in_use = 0;

    ioapic_set_entry_locked((uint32_t)irq, IRQ_VECTOR_BASE + (uint32_t)irq, 0);
    spin_unlock_irqrestore(&g_intr_lock, flags);
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
    uint64_t flags = spin_lock_irqsave(&g_intr_lock);
    irq_slot_t *slot = &g_irq_slots[irq];
    if (!slot->attached || slot->owner_pid != p->pid) {
        spin_unlock_irqrestore(&g_intr_lock, flags);
        return -EPERM;
    }
    ioapic_unmask_irq_locked((uint32_t)irq);
    spin_unlock_irqrestore(&g_intr_lock, flags);
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

long sys_pci_cfg(uint64_t op, uint64_t bdf, uint64_t offset, uint64_t arg) {
    pcb_t *p = proc_current();
    if (!p || !(p->caps & CAP_SYS_INTR)) {
        return -EPERM;
    }
    if (bdf > 0xFFFFULL || offset >= 256ULL || (offset & 3ULL) != 0ULL) {
        return -EINVAL;
    }
    uint8_t bus = (uint8_t)((bdf >> 8) & 0xFFu);
    uint8_t slot = (uint8_t)((bdf >> 3) & 0x1Fu);
    uint8_t func = (uint8_t)(bdf & 0x7u);
    uint8_t off = (uint8_t)offset;

    if (op == PCI_CFG_OP_READ32) {
        if (arg == 0ULL) {
            return -EINVAL;
        }
        uint64_t flags = spin_lock_irqsave(&g_intr_lock);
        uint32_t v = arch_pci_cfg_read32(bus, slot, func, off);
        spin_unlock_irqrestore(&g_intr_lock, flags);
        if (copy_to_user(arg, &v, sizeof(v)) < 0) {
            return -EFAULT;
        }
        return 0;
    }
    if (op == PCI_CFG_OP_WRITE32) {
        uint64_t flags = spin_lock_irqsave(&g_intr_lock);
        arch_pci_cfg_write32(bus, slot, func, off, (uint32_t)arg);
        spin_unlock_irqrestore(&g_intr_lock, flags);
        return 0;
    }
    return -EINVAL;
}
