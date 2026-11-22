/**
 * @file     spinlock.h
 * @brief    Portable IRQ-safe ticket spinlock primitives for SMP kernels
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: COMP_PROC_0008 */

#include <stdint.h>
#include <kernel/arch.h>

typedef struct spinlock {
    uint32_t next;
    uint32_t serving;
} spinlock_t;

#define SPINLOCK_INIT                                                          \
    { 0u, 0u }

static inline void spin_lock_init(spinlock_t *lk) {
    __atomic_store_n(&lk->next, 0u, __ATOMIC_RELAXED);
    __atomic_store_n(&lk->serving, 0u, __ATOMIC_RELAXED);
}

static inline void spin_lock(spinlock_t *lk) {
    uint32_t ticket = __atomic_fetch_add(&lk->next, 1u, __ATOMIC_RELAXED);
    while (__atomic_load_n(&lk->serving, __ATOMIC_ACQUIRE) != ticket) {
        arch_cpu_relax();
    }
}

static inline void spin_unlock(spinlock_t *lk) {
    uint32_t cur = __atomic_load_n(&lk->serving, __ATOMIC_RELAXED);
    __atomic_store_n(&lk->serving, cur + 1u, __ATOMIC_RELEASE);
}

static inline uint64_t spin_lock_irqsave(spinlock_t *lk) {
    uint64_t flags = arch_irq_save_disable();
    spin_lock(lk);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *lk, uint64_t flags) {
    spin_unlock(lk);
    arch_irq_restore(flags);
}
