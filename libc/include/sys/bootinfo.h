// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
/* Boot information handed to PID 1 (sysproc) by the kernel. The structure is
 * copied read-only into the process address space at BOOTINFO_VA and passed
 * through the auxiliary vector (AT_MICROKERNEL_BOOTINFO). */

#include <stdint.h>

#define BOOTINFO_MAGIC  0x3130304954424B4DULL /* "MKBTI001" */
#define BOOTINFO_VA     0x10000000ULL

typedef struct boot_info {
    uint64_t magic;
    uint64_t boot_tar_phys;   /* physical address of the ustar bootfs module */
    uint64_t boot_tar_size;
    uint64_t rsdp_phys;       /* ACPI RSDP physical address (0 if not found) */
    uint64_t mem_total_bytes;
    char cmdline[128];
} boot_info_t;

/* Custom auxiliary vector tag. */
#define AT_MICROKERNEL_BOOTINFO 0x1000
