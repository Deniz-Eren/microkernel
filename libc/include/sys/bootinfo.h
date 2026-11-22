/**
 * @file     bootinfo.h
 * @brief    Kernel-to-PID-1 boot information handoff contract
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_BOOT_0001 */
/* Boot information handed to PID 1 (sysproc) by the kernel. The structure is
 * copied read-only into the process address space at BOOTINFO_VA and passed
 * through the auxiliary vector (AT_MICROKERNEL_BOOTINFO). */

#include <stdint.h>

#define BOOTINFO_MAGIC 0x3130304954424B4DULL /* "MKBTI001" */
#define BOOTINFO_VA 0x10000000ULL
#define BOOTFS_USER_VA 0x11000000ULL

#define BOOT_CPU_X86_64_V3 (1u << 0)
#define BOOT_CPU_AVX512 (1u << 1)

typedef struct boot_info {
    uint64_t magic;
    uint64_t boot_tar_phys; /* physical address of the ustar bootfs module */
    uint64_t boot_tar_size;
    uint64_t rsdp_phys; /* ACPI RSDP physical address (0 if not found) */
    uint64_t mem_total_bytes;
    uint64_t ioapic_phys; /* ACPI MADT IOAPIC base physical address */
    uint32_t serial_port; /* ACPI SPCR / COM1 I/O base port */
    uint32_t serial_gsi;  /* ACPI SPCR / MADT serial GSI */
    uint32_t rtc_gsi;     /* ACPI MADT RTC GSI */
    uint32_t cpu_features;
    uint32_t cpu_count; /* online CPU count reported by SMP bring-up */
    char cmdline[128];
} boot_info_t;

/* Custom auxiliary vector tag and runtime auxv pointer. */
#define AT_MICROKERNEL_BOOTINFO 0x1000

extern void *__auxv;
