/**
 * @file     intr.h
 * @brief    Ring 3 interrupt attachment, unmask, IOPL, and port I/O API
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0002 */
#include <stddef.h>
#include <stdint.h>
#include <sys/arch/cpu.h>
#include <sys/microkernel.h>

#define CAP_SYS_INTR (1u << 0)
#define PULSE_CODE_IRQ 0x100u

static inline long AttachIntrEvent(unsigned irq, int chanid,
                                   uint32_t pulse_code, uint32_t pulse_val) {
    return __sys4(SYS_INTR_ATTACH_EVENT, (long)irq, (long)chanid,
                  (long)pulse_code, (long)pulse_val);
}

static inline long IntrUnmask(unsigned irq) {
    return __sys1(SYS_INTR_UNMASK, (long)irq);
}

static inline long IOPLSet(unsigned level) {
    return __sys1(SYS_IOPL_SET, (long)level);
}

static inline long VMMapMMIO(uint64_t phys_addr, size_t len, uint32_t flags) {
    return __sys3(SYS_VM_MAP_MMIO, (long)phys_addr, (long)len, (long)flags);
}

static inline long DMAAlloc(size_t len, uint64_t *phys_out) {
    return __sys2(SYS_DMA_ALLOC, (long)len, (long)(uintptr_t)phys_out);
}

static inline uint32_t pci_bdf(uint8_t bus, uint8_t slot, uint8_t func) {
    return ((uint32_t)bus << 8) | (((uint32_t)slot & 0x1Fu) << 3) |
           ((uint32_t)func & 0x7u);
}

static inline long PCIConfigRead32(uint32_t bdf, uint32_t offset,
                                   uint32_t *val_out) {
    return __sys4(SYS_PCI_CFG, (long)PCI_CFG_OP_READ32, (long)bdf, (long)offset,
                  (long)(uintptr_t)val_out);
}

static inline long PCIConfigWrite32(uint32_t bdf, uint32_t offset,
                                    uint32_t val) {
    return __sys4(SYS_PCI_CFG, (long)PCI_CFG_OP_WRITE32, (long)bdf,
                  (long)offset, (long)val);
}
