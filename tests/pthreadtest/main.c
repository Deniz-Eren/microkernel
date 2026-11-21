/**
 * @file     main.c
 * @brief    Ring 3 POSIX.1-2024 pthreads, semaphores, and FS.base TLS test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define NWORKERS 3
#define ITERS 500

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t g_spin;
static pthread_rwlock_t g_rw = PTHREAD_RWLOCK_INITIALIZER;
static pthread_barrier_t g_bar;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static pthread_key_t g_key;
static sem_t g_sem;

static int g_counter;
static int g_spin_counter;
static int g_once_runs;
static int g_serial_hits;
static int g_dtor_hits;

static void once_init(void) {
    g_once_runs++;
}

static void key_dtor(void *val) {
    if (val) {
        (void)__atomic_fetch_add(&g_dtor_hits, 1, __ATOMIC_ACQ_REL);
    }
}

static void *worker_fn(void *arg) {
    uintptr_t id = (uintptr_t)arg;

    /* Verify FS.base TLS isolation for errno and TSD keys. */
    errno = (int)(100u + id);
    (void)pthread_setspecific(g_key, (void *)(id + 1u));
    (void)pthread_once(&g_once, once_init);

    for (int i = 0; i < ITERS; i++) {
        (void)pthread_mutex_lock(&g_mtx);
        g_counter++;
        (void)pthread_mutex_unlock(&g_mtx);

        (void)pthread_spin_lock(&g_spin);
        g_spin_counter++;
        (void)pthread_spin_unlock(&g_spin);
    }

    int brc = pthread_barrier_wait(&g_bar);
    if (brc == PTHREAD_BARRIER_SERIAL_THREAD) {
        (void)__atomic_fetch_add(&g_serial_hits, 1, __ATOMIC_ACQ_REL);
    }

    (void)pthread_rwlock_rdlock(&g_rw);
    int snap = g_counter;
    (void)pthread_rwlock_unlock(&g_rw);
    (void)snap;

    if (errno != (int)(100u + id) ||
        pthread_getspecific(g_key) != (void *)(id + 1u)) {
        return (void *)0;
    }

    (void)sem_post(&g_sem);
    return (void *)(uintptr_t)(0x1000u + id);
}

static pthread_mutex_t g_cv_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static int g_cv_ready;

static void *cond_signaler(void *arg) {
    (void)arg;
    struct timespec sl = {.tv_sec = 0, .tv_nsec = 15000000L};
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &sl, (struct timespec *)0);
    (void)pthread_mutex_lock(&g_cv_mtx);
    g_cv_ready = 1;
    (void)pthread_cond_signal(&g_cv);
    (void)pthread_mutex_unlock(&g_cv_mtx);
    return (void *)1;
}

int main(void) {
    /* 1. Initialize primitives and verify recursive/errorcheck mutexes. */
    if (pthread_spin_init(&g_spin, PTHREAD_PROCESS_PRIVATE) != 0 ||
        pthread_barrier_init(&g_bar, (pthread_barrierattr_t *)0, NWORKERS) !=
            0 ||
        pthread_key_create(&g_key, key_dtor) != 0 ||
        sem_init(&g_sem, 0, 0u) != 0) {
        printf("[FAIL] pthreadtest init failed\n");
        return 1;
    }

    pthread_mutexattr_t rma;
    pthread_mutex_t rmtx;
    if (pthread_mutexattr_init(&rma) != 0 ||
        pthread_mutexattr_settype(&rma, PTHREAD_MUTEX_RECURSIVE) != 0 ||
        pthread_mutex_init(&rmtx, &rma) != 0 ||
        pthread_mutex_lock(&rmtx) != 0 || pthread_mutex_lock(&rmtx) != 0 ||
        pthread_mutex_unlock(&rmtx) != 0 || pthread_mutex_unlock(&rmtx) != 0 ||
        pthread_mutex_destroy(&rmtx) != 0) {
        printf("[FAIL] pthreadtest recursive mutex failed\n");
        return 1;
    }
    (void)pthread_mutexattr_destroy(&rma);

    pthread_mutexattr_t ema;
    pthread_mutex_t emtx;
    if (pthread_mutexattr_init(&ema) != 0 ||
        pthread_mutexattr_settype(&ema, PTHREAD_MUTEX_ERRORCHECK) != 0 ||
        pthread_mutex_init(&emtx, &ema) != 0 ||
        pthread_mutex_lock(&emtx) != 0 ||
        pthread_mutex_lock(&emtx) != EDEADLK ||
        pthread_mutex_unlock(&emtx) != 0 || pthread_mutex_destroy(&emtx) != 0) {
        printf("[FAIL] pthreadtest errorcheck mutex failed\n");
        return 1;
    }
    (void)pthread_mutexattr_destroy(&ema);

    /* 2. Spawn 3 workers exercising FS.base TLS, mutex, spinlock, barrier,
     * rwlock, pthread_once, TSD destructors, and sem_post/sem_wait. */
    errno = 77;
    pthread_t th[NWORKERS];
    for (uintptr_t i = 0; i < NWORKERS; i++) {
        if (pthread_create(&th[i], (pthread_attr_t *)0, worker_fn, (void *)i) !=
            0) {
            printf("[FAIL] pthreadtest pthread_create %lu failed\n",
                   (unsigned long)i);
            return 1;
        }
    }

    for (int i = 0; i < NWORKERS; i++) {
        if (sem_wait(&g_sem) != 0) {
            printf("[FAIL] pthreadtest sem_wait failed\n");
            return 1;
        }
    }

    for (uintptr_t i = 0; i < NWORKERS; i++) {
        void *rv = (void *)0;
        if (pthread_join(th[i], &rv) != 0 ||
            rv != (void *)(uintptr_t)(0x1000u + i)) {
            printf("[FAIL] pthreadtest pthread_join %lu mismatch\n",
                   (unsigned long)i);
            return 1;
        }
    }

    if (errno != 77 || g_counter != NWORKERS * ITERS ||
        g_spin_counter != NWORKERS * ITERS || g_once_runs != 1 ||
        g_serial_hits != 1 || g_dtor_hits != NWORKERS) {
        printf("[FAIL] pthreadtest results cnt=%d spin=%d once=%d ser=%d "
               "dtor=%d errno=%d\n",
               g_counter, g_spin_counter, g_once_runs, g_serial_hits,
               g_dtor_hits, errno);
        return 1;
    }
    printf("[OK] pthreads FS.base TLS, mutex, spinlock, barrier, rwlock, and "
           "TSD verified\n");

    /* 3. Verify pthread_cond_clockwait timeout and wakeup, plus sem_clockwait
     * timeout and sem_trywait EAGAIN. */
    struct timespec now = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    struct timespec soon = now;
    soon.tv_nsec += 15000000L;
    if (soon.tv_nsec >= 1000000000L) {
        soon.tv_sec++;
        soon.tv_nsec -= 1000000000L;
    }

    (void)pthread_mutex_lock(&g_cv_mtx);
    int tw = pthread_cond_clockwait(&g_cv, &g_cv_mtx, CLOCK_MONOTONIC, &soon);
    (void)pthread_mutex_unlock(&g_cv_mtx);
    if (tw != ETIMEDOUT) {
        printf("[FAIL] pthreadtest cond_clockwait expected ETIMEDOUT got %d\n",
               tw);
        return 1;
    }

    pthread_t sig_t;
    if (pthread_create(&sig_t, (pthread_attr_t *)0, cond_signaler, (void *)0) !=
        0) {
        printf("[FAIL] pthreadtest cond_signaler create failed\n");
        return 1;
    }
    (void)pthread_mutex_lock(&g_cv_mtx);
    while (!g_cv_ready) {
        (void)pthread_cond_wait(&g_cv, &g_cv_mtx);
    }
    (void)pthread_mutex_unlock(&g_cv_mtx);
    (void)pthread_join(sig_t, (void **)0);

    errno = 0;
    if (sem_trywait(&g_sem) == 0 || errno != EAGAIN) {
        printf("[FAIL] pthreadtest sem_trywait did not give EAGAIN\n");
        return 1;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    soon = now;
    soon.tv_nsec += 15000000L;
    if (soon.tv_nsec >= 1000000000L) {
        soon.tv_sec++;
        soon.tv_nsec -= 1000000000L;
    }
    errno = 0;
    if (sem_clockwait(&g_sem, CLOCK_MONOTONIC, &soon) == 0 ||
        errno != ETIMEDOUT) {
        printf("[FAIL] pthreadtest sem_clockwait expected ETIMEDOUT got %d\n",
               errno);
        return 1;
    }

    (void)pthread_key_delete(g_key);
    (void)pthread_barrier_destroy(&g_bar);
    (void)pthread_spin_destroy(&g_spin);
    (void)sem_destroy(&g_sem);
    printf("[OK] pthread_cond_clockwait and sem_wait/sem_clockwait verified\n");
    return 0;
}
