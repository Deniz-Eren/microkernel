// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once

/* need:: ICD_MEM_0001 */
#include <stddef.h>
#include <stdint.h>

/* User-pointer access helpers. None of these ever dereference a user VA
 * directly: every page is translated first and the data moves through the
 * HHDM, so SMAP stays enforced and faulting pages yield -EFAULT instead of a
 * kernel-mode page fault.
 *
 * The _pml4 variants operate on an ARBITRARY address space (needed by IPC,
 * where the server touches client buffers while the client is not running).
 * They validate PTE permissions, so a write into a read-only user page fails
 * with -EFAULT instead of silently succeeding through the HHDM. */

long copy_from_user(void *kdst, uint64_t usrc, size_t n);
long copy_to_user(uint64_t udst, const void *ksrc, size_t n);
long copy_from_user_pml4(uint64_t pml4_phys, void *kdst, uint64_t usrc,
                         size_t n);
long copy_to_user_pml4(uint64_t pml4_phys, uint64_t udst, const void *ksrc,
                       size_t n);
