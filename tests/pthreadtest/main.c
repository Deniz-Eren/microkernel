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
#include <stdlib.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define NWORKERS 3
#define ITERS 500

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t g_spin;
static pthread_rwlock_t g_rw = PTHREAD_RWLOCK_INITIALIZER;
static pthread_barrier_t g_bar;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static pthread_key_t g_key;
static sem_t g_sem;
static mtx_t g_c11_mtx;
static cnd_t g_c11_cnd;

static int g_counter;
static int g_spin_counter;
static int g_once_runs;
static int g_serial_hits;
static int g_dtor_hits;

static volatile int g_c11_done;
static volatile int g_atfork_prepare;
static volatile int g_atfork_parent;
static volatile int g_atfork_child;
static pthread_mutex_t g_robust_mtx;

static void once_init(void) {
    g_once_runs++;
}

static void atfork_prepare1(void) {
    g_atfork_prepare = g_atfork_prepare * 10 + 1;
}

static void atfork_prepare2(void) {
    g_atfork_prepare = g_atfork_prepare * 10 + 2;
}

static void atfork_parent1(void) {
    g_atfork_parent = g_atfork_parent * 10 + 1;
}

static void atfork_parent2(void) {
    g_atfork_parent = g_atfork_parent * 10 + 2;
}

static void atfork_child1(void) {
    g_atfork_child = g_atfork_child * 10 + 1;
}

static void atfork_child2(void) {
    g_atfork_child = g_atfork_child * 10 + 2;
}

static void *robust_owner_dies(void *arg) {
    pthread_mutex_t *m = (pthread_mutex_t *)arg;
    (void)pthread_mutex_lock(m);
    pthread_exit((void *)0);
}

static int c11_worker(void *arg) {
    return (int)(uintptr_t)arg;
}

static int c11_cond_worker(void *arg) {
    (void)arg;
    (void)mtx_lock(&g_c11_mtx);
    g_c11_done = 1;
    (void)cnd_signal(&g_c11_cnd);
    (void)mtx_unlock(&g_c11_mtx);
    return 0;
}

static void *stack_worker(void *arg) {
    (void)arg;
    return NULL;
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

    thrd_t c11_tid;
    if (thrd_create(&c11_tid, c11_worker, (void *)(uintptr_t)77) !=
        thrd_success) {
        printf("[FAIL] thrd_create\n");
        return 1;
    }
    int c11_res = -1;
    if (thrd_join(c11_tid, &c11_res) != thrd_success || c11_res != 77) {
        printf("[FAIL] thrd_join res=%d\n", c11_res);
        return 1;
    }
    if (!thrd_equal(thrd_current(), thrd_current()) ||
        thrd_equal(c11_tid, thrd_current())) {
        printf("[FAIL] thrd_equal/thrd_current\n");
        return 1;
    }
    printf("[OK] threads.h thrd_create, thrd_join, and thrd_equal verified\n");

    if (mtx_init(&g_c11_mtx, mtx_plain) != thrd_success ||
        cnd_init(&g_c11_cnd) != thrd_success) {
        printf("[FAIL] mtx_init/cnd_init\n");
        return 1;
    }
    g_c11_done = 0;
    thrd_t c11_waiter;
    if (thrd_create(&c11_waiter, c11_cond_worker, NULL) != thrd_success) {
        printf("[FAIL] c11 cond worker create\n");
        return 1;
    }
    (void)mtx_lock(&g_c11_mtx);
    while (!g_c11_done) {
        if (cnd_wait(&g_c11_cnd, &g_c11_mtx) != thrd_success) {
            printf("[FAIL] cnd_wait\n");
            return 1;
        }
    }
    (void)mtx_unlock(&g_c11_mtx);
    (void)thrd_join(c11_waiter, NULL);
    if (mtx_trylock(&g_c11_mtx) != thrd_success) {
        printf("[FAIL] mtx_trylock on unlocked mutex\n");
        return 1;
    }
    (void)mtx_unlock(&g_c11_mtx);
    mtx_destroy(&g_c11_mtx);
    cnd_destroy(&g_c11_cnd);
    printf("[OK] threads.h mtx and cnd wait/signal verified\n");

    static once_flag c11_flag = ONCE_FLAG_INIT;
    g_once_runs = 0;
    call_once(&c11_flag, once_init);
    call_once(&c11_flag, once_init);
    if (g_once_runs != 1) {
        printf("[FAIL] call_once ran %d times\n", g_once_runs);
        return 1;
    }
    tss_t c11_key;
    if (tss_create(&c11_key, NULL) != thrd_success ||
        tss_set(c11_key, (void *)(uintptr_t)0x5A5A) != thrd_success ||
        (uintptr_t)tss_get(c11_key) != 0x5A5Au) {
        printf("[FAIL] tss_create/tss_set/tss_get\n");
        return 1;
    }
    tss_delete(c11_key);
    printf("[OK] threads.h call_once and tss verified\n");

    g_atfork_prepare = g_atfork_parent = g_atfork_child = 0;
    if (pthread_atfork(atfork_prepare1, atfork_parent1, atfork_child1) != 0 ||
        pthread_atfork(atfork_prepare2, atfork_parent2, atfork_child2) != 0) {
        printf("[FAIL] pthread_atfork register\n");
        return 1;
    }
    pid_t af = fork();
    if (af == 0) {
        _exit(g_atfork_prepare == 21 && g_atfork_child == 12 &&
                      g_atfork_parent == 0
                  ? 0
                  : 5);
    }
    int afst = 0;
    if (waitpid(af, &afst, 0) != af || WEXITSTATUS(afst) != 0) {
        printf("[FAIL] pthread_atfork child ordering st=%d\n", afst);
        return 1;
    }
    if (g_atfork_prepare != 21 || g_atfork_parent != 12) {
        printf("[FAIL] pthread_atfork parent order prep=%d par=%d\n",
               (int)g_atfork_prepare, (int)g_atfork_parent);
        return 1;
    }
    printf("[OK] pthread_atfork LIFO prepare and FIFO parent/child verified\n");

    pthread_mutexattr_t rmattr;
    if (pthread_mutexattr_init(&rmattr) != 0 ||
        pthread_mutexattr_setrobust(&rmattr, PTHREAD_MUTEX_ROBUST) != 0 ||
        pthread_mutex_init(&g_robust_mtx, &rmattr) != 0) {
        printf("[FAIL] robust mutex init\n");
        return 1;
    }
    pthread_t rt1;
    if (pthread_create(&rt1, NULL, robust_owner_dies, &g_robust_mtx) != 0) {
        printf("[FAIL] robust worker 1\n");
        return 1;
    }
    (void)pthread_join(rt1, NULL);
    if (pthread_mutex_lock(&g_robust_mtx) != EOWNERDEAD ||
        pthread_mutex_consistent(&g_robust_mtx) != 0 ||
        pthread_mutex_unlock(&g_robust_mtx) != 0 ||
        pthread_mutex_lock(&g_robust_mtx) != 0 ||
        pthread_mutex_unlock(&g_robust_mtx) != 0) {
        printf("[FAIL] robust mutex recovery failed\n");
        return 1;
    }
    pthread_t rt2;
    if (pthread_create(&rt2, NULL, robust_owner_dies, &g_robust_mtx) != 0) {
        printf("[FAIL] robust worker 2\n");
        return 1;
    }
    (void)pthread_join(rt2, NULL);
    if (pthread_mutex_lock(&g_robust_mtx) != EOWNERDEAD ||
        pthread_mutex_unlock(&g_robust_mtx) != 0 ||
        pthread_mutex_lock(&g_robust_mtx) != ENOTRECOVERABLE) {
        printf("[FAIL] robust mutex ENOTRECOVERABLE failed\n");
        return 1;
    }
    (void)pthread_mutex_destroy(&g_robust_mtx);
    (void)pthread_mutexattr_destroy(&rmattr);
    printf(
        "[OK] PTHREAD_MUTEX_ROBUST EOWNERDEAD and ENOTRECOVERABLE verified\n");

    void *stack_mem = malloc(32u * 1024u);
    if (!stack_mem) {
        printf("[FAIL] attr stack malloc\n");
        return 1;
    }
    pthread_attr_t sattr;
    if (pthread_attr_init(&sattr) != 0 ||
        pthread_attr_setstack(&sattr, stack_mem, 32u * 1024u) != 0) {
        printf("[FAIL] pthread_attr_setstack\n");
        return 1;
    }
    void *got_addr = NULL;
    size_t got_size = 0;
    if (pthread_attr_getstack(&sattr, &got_addr, &got_size) != 0 ||
        got_addr != stack_mem || got_size != 32u * 1024u) {
        printf("[FAIL] pthread_attr_getstack\n");
        return 1;
    }
    pthread_t stack_tid;
    if (pthread_create(&stack_tid, &sattr, stack_worker, NULL) != 0) {
        printf("[FAIL] pthread_create with custom stack errno=%d\n", errno);
        return 1;
    }
    (void)pthread_join(stack_tid, NULL);
    size_t gsize = 0;
    if (pthread_attr_getguardsize(&sattr, &gsize) != 0 || gsize != 4096u) {
        printf("[FAIL] pthread_attr_getguardsize %zu\n", gsize);
        return 1;
    }
    (void)pthread_attr_destroy(&sattr);
    free(stack_mem);
    printf("[OK] pthread_attr stack, guardsize, and custom-stack thread\n");

    sem_t *named = sem_open("/pthreadtest", O_CREAT | O_EXCL, 0666u, 0u);
    if (named == SEM_FAILED) {
        (void)sem_unlink("/pthreadtest");
        named = sem_open("/pthreadtest", O_CREAT | O_EXCL, 0666u, 0u);
    }
    if (named == SEM_FAILED) {
        printf("[FAIL] sem_open errno=%d\n", errno);
        return 1;
    }
    sem_t *named_dup = sem_open("/pthreadtest", 0);
    if (named_dup != named) {
        printf("[FAIL] repeated sem_open returned different pointer\n");
        return 1;
    }
    int nval = -1;
    if (sem_getvalue(named, &nval) != 0 || nval != 0) {
        printf("[FAIL] sem_open initial value %d\n", nval);
        return 1;
    }
    pid_t nsem = fork();
    if (nsem == 0) {
        struct timespec ts2 = {0, 100000000L};
        (void)nanosleep(&ts2, NULL);
        if (sem_post(named) != 0) {
            _exit(6);
        }
        (void)sem_close(named_dup);
        (void)sem_close(named);
        _exit(0);
    }
    if (sem_wait(named) != 0) {
        printf("[FAIL] sem_wait named errno=%d\n", errno);
        return 1;
    }
    int nwst = 0;
    if (waitpid(nsem, &nwst, 0) != nsem || WEXITSTATUS(nwst) != 0) {
        printf("[FAIL] named sem child st=0x%x\n", nwst);
        return 1;
    }
    if (sem_trywait(named) == 0 || errno != EAGAIN) {
        printf("[FAIL] named sem value after post\n");
        return 1;
    }
    if (sem_close(named_dup) != 0 || sem_close(named) != 0) {
        printf("[FAIL] sem_close errno=%d\n", errno);
        return 1;
    }
    if (sem_unlink("/pthreadtest") != 0) {
        printf("[FAIL] sem_unlink errno=%d\n", errno);
        return 1;
    }
    printf("[OK] named sem_open/sem_wait/sem_post/sem_close/sem_unlink\n");
    return 0;
}
