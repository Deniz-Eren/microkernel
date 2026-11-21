/**
 * @file     errno.c
 * @brief    Lock-free thread-local errno storage indexed by kernel thread ID
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <sys/thread.h>
#include "tls_internal.h"

/* need:: COMP_LIBC_0002 */

#define ERRNO_SLOTS_MAX 64

struct __errno_slot {
    unsigned int tid;
    int value;
};

static struct __errno_slot g_errno_slots[ERRNO_SLOTS_MAX];
static int g_errno_fallback;

static int claim_slot(unsigned int *p, unsigned int tid) {
    unsigned int z = 0;
    const int ok = __ATOMIC_ACQ_REL;
    const int fail = __ATOMIC_RELAXED;
    return __atomic_compare_exchange_n(p, &z, tid, 0, ok, fail);
}

/*
 * Return a thread-local pointer to errno. Fast path reads O(1) x86-64 FS.base
 * TLS (%fs:0) with zero syscalls; raw ThreadSpawn threads without TLS fall back
 * to a lock-free TID-indexed atomic table (both paths are async-signal-safe).
 */
int *__errno_location(void) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (tls) {
        return &tls->errno_val;
    }

    thread_info_t ti = {0};
    long r = ThreadGetCtx(0, &ti);
    if (r < 0 || ti.tid == 0) {
        return &g_errno_fallback;
    }

    unsigned int tid = ti.tid;
    for (int i = 0; i < ERRNO_SLOTS_MAX; i++) {
        if (__atomic_load_n(&g_errno_slots[i].tid, __ATOMIC_ACQUIRE) == tid) {
            return &g_errno_slots[i].value;
        }
    }

    for (int i = 0; i < ERRNO_SLOTS_MAX; i++) {
        if (claim_slot(&g_errno_slots[i].tid, tid)) {
            g_errno_slots[i].value = 0;
            return &g_errno_slots[i].value;
        }
        if (__atomic_load_n(&g_errno_slots[i].tid, __ATOMIC_ACQUIRE) == tid) {
            return &g_errno_slots[i].value;
        }
    }

    return &g_errno_fallback;
}
