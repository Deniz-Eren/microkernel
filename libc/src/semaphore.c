/**
 * @file     semaphore.c
 * @brief    POSIX.1-2024 Base unnamed semaphores with async-signal-safe post
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/microkernel.h>
#include "tls_internal.h"

/* need:: COMP_PROC_0006 */

int sem_init(sem_t *sem, int pshared, unsigned value) {
    if (!sem || value > (unsigned)SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (pshared != 0) {
        errno = ENOSYS;
        return -1;
    }
    __atomic_store_n(&sem->__val, value, __ATOMIC_RELAXED);
    __atomic_store_n(&sem->__waiters, 0u, __ATOMIC_RELAXED);
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    if (__atomic_load_n(&sem->__waiters, __ATOMIC_ACQUIRE) != 0u) {
        errno = EBUSY;
        return -1;
    }
    return 0;
}

int sem_trywait(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    unsigned int cur = __atomic_load_n(&sem->__val, __ATOMIC_RELAXED);
    while (cur > 0u) {
        if (__atomic_compare_exchange_n(&sem->__val, &cur, cur - 1u, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
    errno = EAGAIN;
    return -1;
}

int sem_clockwait(sem_t *restrict sem, clockid_t clock_id,
                  const struct timespec *restrict abstime) {
    __libc_cancel_point();
    if (!sem || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        errno = EINVAL;
        return -1;
    }
    for (;;) {
        unsigned int cur = __atomic_load_n(&sem->__val, __ATOMIC_RELAXED);
        if (cur > 0u) {
            if (__atomic_compare_exchange_n(&sem->__val, &cur, cur - 1u, 0,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED)) {
                return 0;
            }
            continue;
        }
        (void)__atomic_fetch_add(&sem->__waiters, 1u, __ATOMIC_RELAXED);
        long r = __sys5(SYS_FUTEX, (long)(uintptr_t)&sem->__val, FUTEX_WAIT, 0,
                        (long)(uintptr_t)abstime, (long)clock_id);
        (void)__atomic_fetch_sub(&sem->__waiters, 1u, __ATOMIC_RELAXED);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r == -ETIMEDOUT || r == -EINTR || r == -EINVAL) {
            errno = (int)(-r);
            return -1;
        }
    }
}

int sem_timedwait(sem_t *restrict sem,
                  const struct timespec *restrict abstime) {
    return sem_clockwait(sem, CLOCK_REALTIME, abstime);
}

int sem_wait(sem_t *sem) {
    return sem_clockwait(sem, CLOCK_MONOTONIC, (const struct timespec *)0);
}

int sem_post(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    unsigned int cur = __atomic_load_n(&sem->__val, __ATOMIC_RELAXED);
    for (;;) {
        if (cur >= (unsigned int)SEM_VALUE_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        if (__atomic_compare_exchange_n(&sem->__val, &cur, cur + 1u, 0,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            break;
        }
    }
    if (__atomic_load_n(&sem->__waiters, __ATOMIC_ACQUIRE) > 0u) {
        (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&sem->__val, FUTEX_WAKE, 1, 0,
                     0);
    }
    return 0;
}

int sem_getvalue(sem_t *restrict sem, int *restrict sval) {
    if (!sem || !sval) {
        errno = EINVAL;
        return -1;
    }
    *sval = (int)__atomic_load_n(&sem->__val, __ATOMIC_ACQUIRE);
    return 0;
}
