/**
 * @file     elf.h
 * @brief    Kernel ELF64 user loader interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Load bases for position-independent (ET_DYN) user images. The dynamic main
 * image shares the static base; the PT_INTERP interpreter sits between the
 * read-only bootfs mapping end and the mmap region floor. */
#define ELF_DYN_BASE 0x400000ULL
#define ELF_INTERP_BASE 0x18000000ULL
#define ELF_INTERP_MAX 64

/* Result of a user image load. Addresses are already biased; phdr describes the
 * mapped program headers for the SysV auxv, and interp holds the PT_INTERP path
 * when the image requests a dynamic linker. entry is the main executable entry
 * for AT_ENTRY; start is where execution begins (the interpreter entry when
 * present). */
typedef struct {
    uint64_t entry;
    uint64_t start;
    uint64_t brk;
    uint64_t bias;
    uint64_t phdr;
    uint64_t phent;
    uint64_t phnum;
    uint64_t base;
    char interp[ELF_INTERP_MAX];
} elf_load_info_t;

/* Load a freestanding ELF64 executable into a user address space. ET_DYN images
 * are mapped at `base`; ET_EXEC ignores `base`. Enforces W^X per PT_LOAD
 * program header flags. */
int elf_load_user(uint64_t pml4_phys, const void *image, size_t size,
                  uint64_t base, elf_load_info_t *out);
