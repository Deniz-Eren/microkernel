/**
 * @file     acpi.h
 * @brief    ACPI RSDP, MADT (IOAPIC/ISO), and SPCR hardware discovery API
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_BOOT_0001 */

#include <stdint.h>

#define ACPI_MAX_ISA_IRQS 16u

typedef struct {
    uint64_t rsdp_phys;
    uint64_t lapic_phys;
    uint64_t ioapic_phys;
    uint32_t ioapic_gsi_base;
    uint32_t irq_gsi[ACPI_MAX_ISA_IRQS];
    uint16_t irq_flags[ACPI_MAX_ISA_IRQS];
    uint32_t iso_count;
    uint16_t serial_port;
    uint32_t serial_gsi;
    uint32_t rtc_gsi;
    uint8_t madt_found;
    uint8_t spcr_found;
} acpi_info_t;

void acpi_init(void);
const acpi_info_t *acpi_info(void);
uint16_t acpi_gsi_flags(uint32_t gsi);
