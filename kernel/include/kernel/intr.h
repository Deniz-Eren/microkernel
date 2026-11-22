/**
 * @file     intr.h
 * @brief    Kernel IOAPIC routing, IRQ-to-IPC event, and IOPL interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0002 */
#include <stdint.h>
#include <kernel/trapframe.h>

void ioapic_init(void);
int intr_dispatch_irq(uint64_t vec);
long sys_intr_attach_event(uint64_t irq, uint64_t chanid, uint64_t code,
                           uint64_t value);
long sys_intr_unmask(uint64_t irq);
long sys_iopl_set(trapframe_t *tf, uint64_t level);
long sys_pci_cfg(uint64_t op, uint64_t bdf, uint64_t offset, uint64_t arg);
