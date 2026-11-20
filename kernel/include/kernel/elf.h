// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stddef.h>
#include <stdint.h>

/* Load a freestanding ELF64 executable into a user address space.
 * Enforces W^X per PT_LOAD program header flags. */
int elf_load_user(uint64_t pml4_phys, const void *image, size_t size,
                  uint64_t *out_entry, uint64_t *out_brk);
