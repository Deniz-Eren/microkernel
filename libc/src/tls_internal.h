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
#include <sys/arch/cpu.h>

#define PTHREAD_KEYS_MAX_INT 32
#define LIBC_SIGCANCEL 30

struct __pthread_cleanup_frame;
struct __pthread_mutex_s;

typedef struct __pthread_tls {
    struct __pthread_tls *self;   /* %fs:0 — NULL until TLS is installed */
    unsigned int tid;             /* kernel TID (also pthread_t) */
    int errno_val;                /* O(1) per-thread errno slot */
    unsigned int used;            /* 1 = slot allocated */
    int detached;                 /* 1 = detached, auto-reclaim on exit */
    unsigned int done;            /* futex word: 0 = running, 1 = exited */
    unsigned int cancel_disabled; /* 0 = ENABLE, 1 = DISABLE */
    unsigned int cancel_async;    /* 0 = DEFERRED, 1 = ASYNCHRONOUS */
    unsigned int cancel_pending;  /* 1 = pthread_cancel() pending */
    unsigned int canceling;       /* 1 = running cleanup / exiting */
    struct __pthread_cleanup_frame *cleanup_head;
    struct __pthread_mutex_s *robust_head;
    void *retval;
    void *(*start_fn)(void *);
    void *start_arg;
    void *locale;
    void *tsd[PTHREAD_KEYS_MAX_INT];
} __pthread_tls_t;

static inline __pthread_tls_t *__libc_tls_self(void) {
    return (__pthread_tls_t *)arch_tls_self_ptr();
}

void __libc_init_main_tls(void);
void __libc_cancel_point(void);
