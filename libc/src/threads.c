/**
 * @file     threads.c
 * @brief    ISO C17 <threads.h> implementation over POSIX.1-2024 pthreads
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

/* need:: COMP_LIBC_0008 */

typedef struct {
    thrd_start_t func;
    void *arg;
} thrd_start_ctx_t;

static void *thrd_trampoline(void *arg) {
    thrd_start_ctx_t ctx = *(thrd_start_ctx_t *)arg;
    free(arg);
    return (void *)(intptr_t)ctx.func(ctx.arg);
}

static int map_errno(int err) {
    switch (err) {
    case 0:
        return thrd_success;
    case EAGAIN:
    case ENOMEM:
        return thrd_nomem;
    case EBUSY:
        return thrd_busy;
    case ETIMEDOUT:
        return thrd_timedout;
    default:
        return thrd_error;
    }
}

int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    if (!thr || !func) {
        return thrd_error;
    }
    thrd_start_ctx_t *ctx = (thrd_start_ctx_t *)malloc(sizeof(*ctx));
    if (!ctx) {
        return thrd_nomem;
    }
    ctx->func = func;
    ctx->arg = arg;
    int rc = pthread_create(thr, NULL, thrd_trampoline, ctx);
    if (rc != 0) {
        free(ctx);
        return map_errno(rc);
    }
    return thrd_success;
}

thrd_t thrd_current(void) {
    return pthread_self();
}

int thrd_detach(thrd_t thr) {
    return map_errno(pthread_detach(thr));
}

int thrd_equal(thrd_t lhs, thrd_t rhs) {
    return pthread_equal(lhs, rhs);
}

_Noreturn void thrd_exit(int res) {
    pthread_exit((void *)(intptr_t)res);
}

int thrd_join(thrd_t thr, int *res) {
    void *value = NULL;
    int rc = pthread_join(thr, &value);
    if (rc != 0) {
        return map_errno(rc);
    }
    if (res) {
        *res = (int)(intptr_t)value;
    }
    return thrd_success;
}

int thrd_sleep(const struct timespec *duration, struct timespec *remaining) {
    if (!duration) {
        return -2;
    }
    if (nanosleep(duration, remaining) == 0) {
        return 0;
    }
    return (errno == EINTR) ? -1 : -2;
}

void thrd_yield(void) {
    (void)sched_yield();
}

int mtx_init(mtx_t *mtx, int type) {
    if (!mtx) {
        return thrd_error;
    }
    int ptype;
    switch (type) {
    case mtx_plain:
    case mtx_timed:
        ptype = PTHREAD_MUTEX_NORMAL;
        break;
    case (mtx_plain | mtx_recursive):
    case (mtx_timed | mtx_recursive):
        ptype = PTHREAD_MUTEX_RECURSIVE;
        break;
    default:
        return thrd_error;
    }
    pthread_mutexattr_t attr;
    (void)pthread_mutexattr_init(&attr);
    (void)pthread_mutexattr_settype(&attr, ptype);
    int rc = pthread_mutex_init(mtx, &attr);
    (void)pthread_mutexattr_destroy(&attr);
    return map_errno(rc);
}

void mtx_destroy(mtx_t *mtx) {
    (void)pthread_mutex_destroy(mtx);
}

int mtx_lock(mtx_t *mtx) {
    return map_errno(pthread_mutex_lock(mtx));
}

int mtx_timedlock(mtx_t *restrict mtx, const struct timespec *restrict ts) {
    if (!ts) {
        return thrd_error;
    }
    return map_errno(pthread_mutex_timedlock(mtx, ts));
}

int mtx_trylock(mtx_t *mtx) {
    return map_errno(pthread_mutex_trylock(mtx));
}

int mtx_unlock(mtx_t *mtx) {
    return map_errno(pthread_mutex_unlock(mtx));
}

int cnd_init(cnd_t *cond) {
    if (!cond) {
        return thrd_error;
    }
    return map_errno(pthread_cond_init(cond, NULL));
}

void cnd_destroy(cnd_t *cond) {
    (void)pthread_cond_destroy(cond);
}

int cnd_signal(cnd_t *cond) {
    return map_errno(pthread_cond_signal(cond));
}

int cnd_broadcast(cnd_t *cond) {
    return map_errno(pthread_cond_broadcast(cond));
}

int cnd_wait(cnd_t *cond, mtx_t *mtx) {
    return map_errno(pthread_cond_wait(cond, mtx));
}

int cnd_timedwait(cnd_t *restrict cond, mtx_t *restrict mtx,
                  const struct timespec *restrict ts) {
    if (!ts) {
        return thrd_error;
    }
    return map_errno(pthread_cond_timedwait(cond, mtx, ts));
}

void call_once(once_flag *flag, void (*func)(void)) {
    (void)pthread_once(flag, func);
}

int tss_create(tss_t *key, tss_dtor_t dtor) {
    if (!key) {
        return thrd_error;
    }
    return map_errno(pthread_key_create(key, dtor));
}

void tss_delete(tss_t key) {
    (void)pthread_key_delete(key);
}

void *tss_get(tss_t key) {
    return pthread_getspecific(key);
}

int tss_set(tss_t key, void *val) {
    return map_errno(pthread_setspecific(key, val));
}
