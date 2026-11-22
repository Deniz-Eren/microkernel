/**
 * @file     threads.h
 * @brief    ISO C17 <threads.h> over POSIX.1-2024 pthread primitives
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _THREADS_H
#define _THREADS_H

/* need:: ICD_LIBC_0008 */

#include <sys/_pthread_types.h>
#include <time.h>

typedef __pthread_t thrd_t;
typedef __pthread_once_t once_flag;
typedef struct __pthread_mutex_s mtx_t;
typedef struct __pthread_cond_s cnd_t;
typedef __pthread_key_t tss_t;

typedef int (*thrd_start_t)(void *);
typedef void (*tss_dtor_t)(void *);

enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };

enum {
    thrd_success = 0,
    thrd_nomem = 1,
    thrd_timedout = 2,
    thrd_busy = 3,
    thrd_error = 4
};

#define ONCE_FLAG_INIT 0u
#define TSS_DTOR_ITERATIONS 4

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
thrd_t thrd_current(void);
int thrd_detach(thrd_t thr);
int thrd_equal(thrd_t lhs, thrd_t rhs);
_Noreturn void thrd_exit(int res);
int thrd_join(thrd_t thr, int *res);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);

int mtx_init(mtx_t *mtx, int type);
void mtx_destroy(mtx_t *mtx);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *restrict mtx, const struct timespec *restrict ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);

int cnd_broadcast(cnd_t *cond);
void cnd_destroy(cnd_t *cond);
int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx,
                  const struct timespec *restrict ts);
int cnd_wait(cnd_t *cond, mtx_t *mtx);

void call_once(once_flag *flag, void (*func)(void));

int tss_create(tss_t *key, tss_dtor_t dtor);
void tss_delete(tss_t key);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);

#endif /* _THREADS_H */
