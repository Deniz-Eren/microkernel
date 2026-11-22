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
