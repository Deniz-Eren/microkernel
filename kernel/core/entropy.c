/**
 * @file     entropy.c
 * @brief    Kernel entropy mixer (RDRAND/RDTSC/clock) for SYS_GETENTROPY
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/arch.h>
#include <kernel/entropy.h>
#include <kernel/sched.h>
#include <kernel/spinlock.h>
#include <kernel/time.h>
#include <kernel/uaccess.h>
#include <sys/errno.h>
#include <time.h>

/* need:: COMP_PROC_0002 COMP_PROC_0008 */

#define ENTROPY_MAX_LEN 256u

static spinlock_t g_entropy_lock = SPINLOCK_INIT;
static uint64_t g_state[4];
static int g_seeded;

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static void entropy_seed_if_needed(void) {
    if (g_seeded) {
        return;
    }
    uint64_t seed = arch_entropy_hw_sample() ^
                    time_clock_now_ns(CLOCK_MONOTONIC) ^ 0x243F6A8885A308D3ULL;
    for (int i = 0; i < 4; i++) {
        uint64_t extra = arch_entropy_hw_sample();
        g_state[i] = splitmix64(&seed) ^ extra;
    }
    if ((g_state[0] | g_state[1] | g_state[2] | g_state[3]) == 0ULL) {
        g_state[0] = 0x13198A2E03707344ULL;
    }
    g_seeded = 1;
}

static uint64_t entropy_next64(void) {
    g_state[0] ^= arch_entropy_hw_sample();
    g_state[1] ^= arch_entropy_hw_sample();

    /* xoshiro256** step */
    uint64_t result = rotl64(g_state[1] * 5ULL, 7) * 9ULL;
    uint64_t t = g_state[1] << 17;

    g_state[2] ^= g_state[0];
    g_state[3] ^= g_state[1];
    g_state[1] ^= g_state[2];
    g_state[0] ^= g_state[3];
    g_state[2] ^= t;
    g_state[3] = rotl64(g_state[3], 45);
    return result;
}

long sys_getentropy(uint64_t buf_va, uint64_t len) {
    if (len > ENTROPY_MAX_LEN) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }
    if (!buf_va) {
        return -EFAULT;
    }

    uint8_t kbuf[ENTROPY_MAX_LEN];
    tcb_t *cur = sched_current();
    uint64_t flags = spin_lock_irqsave(&g_entropy_lock);
    entropy_seed_if_needed();
    if (cur) {
        g_state[2] ^= ((uint64_t)cur->pid << 32) | (uint64_t)cur->tid;
    }
    g_state[3] ^= time_clock_now_ns(CLOCK_MONOTONIC);

    size_t done = 0;
    while (done < (size_t)len) {
        uint64_t w = entropy_next64();
        for (int b = 0; b < 8 && done < (size_t)len; b++) {
            kbuf[done++] = (uint8_t)(w >> (b * 8));
        }
    }
    spin_unlock_irqrestore(&g_entropy_lock, flags);

    if (copy_to_user(buf_va, kbuf, (size_t)len) < 0) {
        return -EFAULT;
    }
    return 0;
}
