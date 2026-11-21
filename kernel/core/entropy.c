/**
 * @file     entropy.c
 * @brief    Kernel entropy mixer (RDRAND/RDTSC/clock) for SYS_GETENTROPY
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/entropy.h>
#include <kernel/sched.h>
#include <kernel/time.h>
#include <kernel/uaccess.h>
#include <sys/errno.h>
#include <time.h>

/* need:: COMP_PROC_0002 */

#define ENTROPY_MAX_LEN 256u

static uint64_t g_state[4];
static int g_seeded;
static int g_has_rdrand;

static inline uint64_t read_tsc(void) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static int detect_rdrand(void) {
    uint32_t eax = 1u;
    uint32_t ebx = 0u;
    uint32_t ecx = 0u;
    uint32_t edx = 0u;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    return (ecx & (1u << 30)) != 0u;
}

static int try_rdrand64(uint64_t *out) {
    if (!g_has_rdrand) {
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        uint64_t v = 0;
        unsigned char ok = 0;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
        if (ok) {
            *out = v;
            return 1;
        }
    }
    return 0;
}

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
    g_has_rdrand = detect_rdrand();
    uint64_t seed =
        read_tsc() ^ time_clock_now_ns(CLOCK_MONOTONIC) ^ 0x243F6A8885A308D3ULL;
    uint64_t hw = 0;
    if (try_rdrand64(&hw)) {
        seed ^= hw;
    }
    for (int i = 0; i < 4; i++) {
        uint64_t extra = read_tsc();
        if (try_rdrand64(&hw)) {
            extra ^= hw;
        }
        g_state[i] = splitmix64(&seed) ^ extra;
    }
    if ((g_state[0] | g_state[1] | g_state[2] | g_state[3]) == 0ULL) {
        g_state[0] = 0x13198A2E03707344ULL;
    }
    g_seeded = 1;
}

static uint64_t entropy_next64(void) {
    uint64_t hw = 0;
    if (try_rdrand64(&hw)) {
        g_state[0] ^= hw;
    }
    g_state[1] ^= read_tsc();

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

    entropy_seed_if_needed();
    tcb_t *cur = sched_current();
    if (cur) {
        g_state[2] ^= ((uint64_t)cur->pid << 32) | (uint64_t)cur->tid;
    }
    g_state[3] ^= time_clock_now_ns(CLOCK_MONOTONIC);

    uint8_t kbuf[ENTROPY_MAX_LEN];
    size_t done = 0;
    while (done < (size_t)len) {
        uint64_t w = entropy_next64();
        for (int b = 0; b < 8 && done < (size_t)len; b++) {
            kbuf[done++] = (uint8_t)(w >> (b * 8));
        }
    }

    if (copy_to_user(buf_va, kbuf, (size_t)len) < 0) {
        return -EFAULT;
    }
    return 0;
}
