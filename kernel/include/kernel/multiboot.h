/**
 * @file     multiboot.h
 * @brief    Multiboot2 memory map and boot module declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

#define MB_MMAP_USABLE 1

typedef struct {
    uint64_t base;
    uint64_t len;
    uint32_t type;
} mb_mmap_entry_t;

typedef struct {
    uint64_t start;
    uint64_t end;
    const char *cmdline;
} mb_module_t;

void multiboot_parse(uint64_t mbi_phys);
const mb_mmap_entry_t *mb_mmap(size_t *count);
const mb_module_t *mb_modules(size_t *count);
const char *mb_cmdline(void);
uint64_t multiboot_mbi_phys(void);
uint64_t mb_total_usable(void);
