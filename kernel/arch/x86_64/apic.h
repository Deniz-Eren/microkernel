// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stdint.h>

void apic_init(void);
void apic_eoi(void);
uint64_t timer_ticks(void);
void apic_irq_dispatch(uint64_t vec);
int acpi_poweroff(void);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);
