/**
 * @file     thread.h
 * @brief    Ring 3 two-phase thread management wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_PROC_0002 */
/* Microkernel thread management (Milestone 4): two-phase creation — NewThread
 * makes an unarmed embryo thread, ThreadReset arms it with a start context
 * and makes it runnable. */
#include <stddef.h>
#include <stdint.h>
#include <sys/microkernel.h>

typedef struct {
    uint32_t tid;
    uint32_t state; /* thread state (kernel numbering) */
    uint32_t prio;  /* effective priority */
    uint32_t reserved;
    uint64_t entry; /* start RIP */
    uint64_t arg;   /* start argument (RDI) */
    uint64_t stack_top;
} thread_info_t;

/* Create an embryo thread at the given priority; returns tid or -errno. */
static inline long NewThread(unsigned prio) {
    return __sys1(SYS_THREAD_CREATE, (long)prio);
}

/* Arm an embryo thread: it starts at entry(arg) on stack_top (0 = kernel
 * allocates a fresh 64 KiB user stack). Returns 0 or -errno. */
static inline long ThreadReset(unsigned tid, void (*entry)(void *), void *arg,
                               unsigned long stack_top) {
    return __sys4(SYS_THREAD_RESET, (long)tid, (long)entry, (long)arg,
                  (long)stack_top);
}

/* Fetch a thread's info/context (tid 0 = caller). */
static inline long ThreadGetCtx(unsigned tid, thread_info_t *ti) {
    return __sys2(SYS_THREAD_GET_CTX, (long)tid, (long)ti);
}

/* Caller's thread id. */
static inline long ThreadGetTid(void) {
    thread_info_t ti;
    long r = ThreadGetCtx(0, &ti);
    return r < 0 ? r : (long)ti.tid;
}

/* Convenience: create + arm with a kernel-allocated stack. */
static inline long ThreadSpawn(void (*entry)(void *), void *arg,
                               unsigned prio) {
    long tid = NewThread(prio);
    if (tid < 0) {
        return tid;
    }
    long r = ThreadReset((unsigned)tid, entry, arg, 0);
    return r < 0 ? r : tid;
}
