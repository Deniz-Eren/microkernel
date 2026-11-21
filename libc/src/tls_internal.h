/**
 * @file     tls_internal.h
 * @brief    Internal x86-64 FS.base per-thread TLS block layout for libc
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

#define PTHREAD_KEYS_MAX_INT 32

typedef struct __pthread_tls {
    struct __pthread_tls *self; /* %fs:0 — NULL until TLS is installed */
    unsigned int tid;           /* kernel TID (also pthread_t) */
    int errno_val;              /* O(1) per-thread errno slot */
    unsigned int used;          /* 1 = slot allocated */
    int detached;               /* 1 = detached, auto-reclaim on exit */
    unsigned int done;          /* futex word: 0 = running, 1 = exited */
    void *retval;
    void *(*start_fn)(void *);
    void *start_arg;
    void *tsd[PTHREAD_KEYS_MAX_INT];
} __pthread_tls_t;

static inline __pthread_tls_t *__libc_tls_self(void) {
    __pthread_tls_t *self;
    __asm__ volatile("movq %%fs:0, %0" : "=r"(self));
    return self;
}

void __libc_init_main_tls(void);
