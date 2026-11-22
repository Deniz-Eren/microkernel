/**
 * @file     arch.h
 * @brief    Portable kernel Hardware Abstraction Layer (HAL) interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_BOOT_0002 */

#include <stddef.h>
#include <stdint.h>

#ifndef MICROKERNEL_ARCH
#define MICROKERNEL_ARCH "x86-64"
#endif

#ifndef ARCH_ELF_MACHINE
#define ARCH_ELF_MACHINE 62 /* EM_X86_64 */
#endif

void arch_early_init(void);
void arch_cpu_init(void);
void arch_timer_init(void);
void arch_user_entry_init(void);

void arch_console_putc(char c);
void arch_console_write(const char *s, size_t n);

void arch_set_kernel_stack(uint64_t kstack_top);
void arch_set_tls_base(uint64_t base);
uint64_t arch_entropy_hw_sample(void);
uint32_t arch_cpu_features(void);
size_t arch_fpu_state_size(void);
size_t arch_fpu_state_align(void);
void arch_fpu_init_template(uint8_t *fpu_area);
void arch_fpu_save_current(uint8_t *fpu_area);
void arch_fpu_restore_current(const uint8_t *fpu_area);
void arch_irq_eoi(void);
uint32_t arch_bsp_cpu_id(void);
uint32_t arch_pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func,
                             uint8_t offset);
void arch_pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func,
                          uint8_t offset, uint32_t val);
void arch_cpu_idle_wait(void);
void arch_first_switch(uint64_t kernel_rsp) __attribute__((noreturn));
void arch_cpu_halt(void) __attribute__((noreturn));
