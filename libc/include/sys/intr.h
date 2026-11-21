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
#include <stdint.h>
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

static inline void port_outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t port_inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline void port_outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint16_t port_inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline void port_outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint32_t port_inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
