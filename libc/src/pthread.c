/**
 * @file     pthread.c
 * @brief    POSIX.1-2024 pthreads, FS.base TLS, mutexes, condvars, and locks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/signal.h>
#include <sys/thread.h>
#include "tls_internal.h"

/* need:: COMP_PROC_0005 */

extern void __sigreturn_trampoline(void);

#define PTHREAD_POOL_MAX 64
#define RWLOCK_WRITER_BIT 0x80000000u

static __pthread_tls_t g_tls_pool[PTHREAD_POOL_MAX];
static __pthread_tls_t *find_tls_by_tid(pthread_t thread);

static void sigcancel_handler(int sig) {
    (void)sig;
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls) {
        return;
    }
    if (__atomic_load_n(&tls->cancel_pending, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&tls->cancel_disabled, __ATOMIC_ACQUIRE) &&
        __atomic_load_n(&tls->cancel_async, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&tls->canceling, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&tls->canceling, 1u, __ATOMIC_RELEASE);
        __atomic_store_n(&tls->cancel_disabled, 1u, __ATOMIC_RELEASE);
        pthread_exit(PTHREAD_CANCELED);
    }
}

typedef struct {
    unsigned int used;
    void (*dtor)(void *);
} pthread_key_slot_t;

static pthread_key_slot_t g_keys[PTHREAD_KEYS_MAX_INT];

static inline long futex_wait(unsigned int *addr, unsigned int val,
                              const struct timespec *abstime,
                              clockid_t clock_id) {
    return __sys5(SYS_FUTEX, (long)(uintptr_t)addr, FUTEX_WAIT, (long)val,
                  (long)(uintptr_t)abstime, (long)clock_id);
}

static inline long futex_wake(unsigned int *addr, unsigned int count) {
    return __sys5(SYS_FUTEX, (long)(uintptr_t)addr, FUTEX_WAKE, (long)count, 0,
                  0);
}

static inline void cpu_pause(void) {
    arch_cpu_relax();
}

void __libc_init_main_tls(void) {
    __pthread_tls_t *tls = &g_tls_pool[0];
    memset(tls, 0, sizeof(*tls));
    long tid = ThreadGetTid();
    tls->self = tls;
    tls->tid = (tid > 0) ? (unsigned int)tid : 1u;
    tls->errno_val = 0;
    tls->used = 1u;
    (void)__sys1(SYS_THREAD_SET_FSBASE, (long)(uintptr_t)tls);

    struct __k_sigaction kact = {
        .handler = (uint64_t)(uintptr_t)sigcancel_handler,
        .mask = 0u,
        .flags = SA_NODEFER,
    };
    (void)__sys4(SYS_SIGACTION, (long)LIBC_SIGCANCEL, (long)(uintptr_t)&kact, 0,
                 (long)(uintptr_t)__sigreturn_trampoline);
}

void __libc_cancel_point(void) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls) {
        return;
    }
    if (__atomic_load_n(&tls->cancel_pending, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&tls->cancel_disabled, __ATOMIC_ACQUIRE) &&
        !__atomic_load_n(&tls->canceling, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&tls->canceling, 1u, __ATOMIC_RELEASE);
        __atomic_store_n(&tls->cancel_disabled, 1u, __ATOMIC_RELEASE);
        pthread_exit(PTHREAD_CANCELED);
    }
}

void __pthread_cleanup_push(struct __pthread_cleanup_frame *frame,
                            void (*routine)(void *), void *arg) {
    __pthread_tls_t *tls = __libc_tls_self();
    frame->__routine = routine;
    frame->__arg = arg;
    if (tls) {
        frame->__prev = tls->cleanup_head;
        tls->cleanup_head = frame;
    } else {
        frame->__prev = (struct __pthread_cleanup_frame *)0;
    }
}

void __pthread_cleanup_pop(struct __pthread_cleanup_frame *frame, int execute) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (tls && tls->cleanup_head == frame) {
        tls->cleanup_head = frame->__prev;
    }
    if (execute && frame->__routine) {
        frame->__routine(frame->__arg);
    }
}

int pthread_cancel(pthread_t thread) {
    __pthread_tls_t *tls = find_tls_by_tid(thread);
    if (!tls) {
        return ESRCH;
    }
    __atomic_store_n(&tls->cancel_pending, 1u, __ATOMIC_RELEASE);
    (void)__sys3(SYS_THREAD_INJECT_SIG, 0, (long)tls->tid,
                 (long)LIBC_SIGCANCEL);
    return 0;
}

int pthread_setcancelstate(int state, int *oldstate) {
    if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
        return EINVAL;
    }
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls) {
        return EINVAL;
    }
    unsigned int next = (state == PTHREAD_CANCEL_DISABLE) ? 1u : 0u;
    unsigned int prev =
        __atomic_exchange_n(&tls->cancel_disabled, next, __ATOMIC_ACQ_REL);
    if (oldstate) {
        *oldstate = prev ? PTHREAD_CANCEL_DISABLE : PTHREAD_CANCEL_ENABLE;
    }
    if (next == 0u && __atomic_load_n(&tls->cancel_async, __ATOMIC_ACQUIRE)) {
        __libc_cancel_point();
    }
    return 0;
}

int pthread_setcanceltype(int type, int *oldtype) {
    if (type != PTHREAD_CANCEL_DEFERRED &&
        type != PTHREAD_CANCEL_ASYNCHRONOUS) {
        return EINVAL;
    }
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls) {
        return EINVAL;
    }
    unsigned int next = (type == PTHREAD_CANCEL_ASYNCHRONOUS) ? 1u : 0u;
    unsigned int prev =
        __atomic_exchange_n(&tls->cancel_async, next, __ATOMIC_ACQ_REL);
    if (oldtype) {
        *oldtype = prev ? PTHREAD_CANCEL_ASYNCHRONOUS : PTHREAD_CANCEL_DEFERRED;
    }
    if (next != 0u) {
        __libc_cancel_point();
    }
    return 0;
}

void pthread_testcancel(void) {
    __libc_cancel_point();
}

static __pthread_tls_t *alloc_tls_slot(void) {
    for (int i = 1; i < PTHREAD_POOL_MAX; i++) {
        unsigned int expected = 0u;
        if (__atomic_compare_exchange_n(&g_tls_pool[i].used, &expected, 1u, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            __pthread_tls_t *tls = &g_tls_pool[i];
            memset(tls, 0, sizeof(*tls));
            tls->used = 1u;
            tls->self = tls;
            return tls;
        }
    }
    return (__pthread_tls_t *)0;
}

static __pthread_tls_t *find_tls_by_tid(pthread_t thread) {
    for (int i = 0; i < PTHREAD_POOL_MAX; i++) {
        if (__atomic_load_n(&g_tls_pool[i].used, __ATOMIC_ACQUIRE) &&
            g_tls_pool[i].tid == (unsigned int)thread) {
            return &g_tls_pool[i];
        }
    }
    return (__pthread_tls_t *)0;
}

static void run_tsd_destructors(__pthread_tls_t *tls) {
    for (int pass = 0; pass < 4; pass++) {
        int any = 0;
        for (int k = 0; k < PTHREAD_KEYS_MAX_INT; k++) {
            void *val = tls->tsd[k];
            if (!val) {
                continue;
            }
            if (__atomic_load_n(&g_keys[k].used, __ATOMIC_ACQUIRE) &&
                g_keys[k].dtor) {
                tls->tsd[k] = (void *)0;
                g_keys[k].dtor(val);
                any = 1;
            }
        }
        if (!any) {
            break;
        }
    }
}

static void pthread_trampoline(void *arg) {
    __pthread_tls_t *tls = (__pthread_tls_t *)arg;
    (void)__sys1(SYS_THREAD_SET_FSBASE, (long)(uintptr_t)tls);
    void *res = tls->start_fn(tls->start_arg);
    pthread_exit(res);
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    attr->__detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    return attr ? 0 : EINVAL;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    if (!attr || !detachstate) {
        return EINVAL;
    }
    *detachstate = attr->__detachstate;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (!attr || (detachstate != PTHREAD_CREATE_JOINABLE &&
                  detachstate != PTHREAD_CREATE_DETACHED)) {
        return EINVAL;
    }
    attr->__detachstate = detachstate;
    return 0;
}

int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg) {
    if (!thread || !start_routine) {
        return EINVAL;
    }
    __pthread_tls_t *tls = alloc_tls_slot();
    if (!tls) {
        return EAGAIN;
    }
    tls->start_fn = start_routine;
    tls->start_arg = arg;
    tls->detached =
        (attr && attr->__detachstate == PTHREAD_CREATE_DETACHED) ? 1 : 0;

    thread_info_t self_ti = {0};
    unsigned prio = 100u;
    if (ThreadGetCtx(0, &self_ti) >= 0) {
        prio = self_ti.prio;
    }

    long tid = NewThread(prio);
    if (tid < 0) {
        __atomic_store_n(&tls->used, 0u, __ATOMIC_RELEASE);
        return (int)(-tid);
    }
    tls->tid = (unsigned int)tid;
    *thread = (pthread_t)tid;

    long rc = ThreadReset((unsigned)tid, pthread_trampoline, tls, 0);
    if (rc < 0) {
        __atomic_store_n(&tls->used, 0u, __ATOMIC_RELEASE);
        return (int)(-rc);
    }
    return 0;
}

void pthread_exit(void *value_ptr) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (tls) {
        __atomic_store_n(&tls->cancel_disabled, 1u, __ATOMIC_RELEASE);
        while (tls->cleanup_head) {
            struct __pthread_cleanup_frame *f = tls->cleanup_head;
            tls->cleanup_head = f->__prev;
            if (f->__routine) {
                f->__routine(f->__arg);
            }
        }
        run_tsd_destructors(tls);
        tls->retval = value_ptr;
        if (__atomic_load_n(&tls->detached, __ATOMIC_ACQUIRE)) {
            __atomic_store_n(&tls->used, 0u, __ATOMIC_RELEASE);
        } else {
            __atomic_store_n(&tls->done, 1u, __ATOMIC_RELEASE);
            (void)futex_wake(&tls->done, (unsigned int)INT_MAX);
        }
    }
    (void)__sys1(SYS_THREAD_EXIT, 0);
    for (;;) {
    }
}

int pthread_join(pthread_t thread, void **value_ptr) {
    __libc_cancel_point();
    if (pthread_equal(thread, pthread_self())) {
        return EDEADLK;
    }
    __pthread_tls_t *tls = find_tls_by_tid(thread);
    if (!tls) {
        return ESRCH;
    }
    if (__atomic_load_n(&tls->detached, __ATOMIC_ACQUIRE)) {
        return EINVAL;
    }
    while (__atomic_load_n(&tls->done, __ATOMIC_ACQUIRE) == 0u) {
        long r = futex_wait(&tls->done, 0u, (const struct timespec *)0,
                            CLOCK_MONOTONIC);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
    }
    if (value_ptr) {
        *value_ptr = tls->retval;
    }
    __atomic_store_n(&tls->used, 0u, __ATOMIC_RELEASE);
    return 0;
}

int pthread_detach(pthread_t thread) {
    __pthread_tls_t *tls = find_tls_by_tid(thread);
    if (!tls) {
        return ESRCH;
    }
    if (__atomic_exchange_n(&tls->detached, 1, __ATOMIC_ACQ_REL)) {
        return EINVAL;
    }
    if (__atomic_load_n(&tls->done, __ATOMIC_ACQUIRE)) {
        __atomic_store_n(&tls->used, 0u, __ATOMIC_RELEASE);
    }
    return 0;
}

pthread_t pthread_self(void) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (tls) {
        return (pthread_t)tls->tid;
    }
    long tid = ThreadGetTid();
    return (tid > 0) ? (pthread_t)tid : (pthread_t)0;
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2 ? 1 : 0;
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine) {
        return EINVAL;
    }
    if (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == 2u) {
        return 0;
    }
    unsigned int expected = 0u;
    if (__atomic_compare_exchange_n(once_control, &expected, 1u, 0,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        init_routine();
        __atomic_store_n(once_control, 2u, __ATOMIC_RELEASE);
        (void)futex_wake(once_control, (unsigned int)INT_MAX);
        return 0;
    }
    while (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) != 2u) {
        (void)futex_wait(once_control, 1u, (const struct timespec *)0,
                         CLOCK_MONOTONIC);
    }
    return 0;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if (!key) {
        return EINVAL;
    }
    for (unsigned int i = 0; i < PTHREAD_KEYS_MAX_INT; i++) {
        unsigned int expected = 0u;
        if (__atomic_compare_exchange_n(&g_keys[i].used, &expected, 1u, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
            g_keys[i].dtor = destructor;
            *key = (pthread_key_t)i;
            return 0;
        }
    }
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX_INT ||
        !__atomic_load_n(&g_keys[key].used, __ATOMIC_ACQUIRE)) {
        return EINVAL;
    }
    g_keys[key].dtor = (void (*)(void *))0;
    __atomic_store_n(&g_keys[key].used, 0u, __ATOMIC_RELEASE);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls || key >= PTHREAD_KEYS_MAX_INT) {
        return (void *)0;
    }
    return tls->tsd[key];
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls || key >= PTHREAD_KEYS_MAX_INT ||
        !__atomic_load_n(&g_keys[key].used, __ATOMIC_ACQUIRE)) {
        return EINVAL;
    }
    tls->tsd[key] = (void *)(uintptr_t)value;
    return 0;
}

/* ---- Mutexes (3-state futex: 0=free, 1=locked, 2=contended) ---- */

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    attr->__type = PTHREAD_MUTEX_NORMAL;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    return attr ? 0 : EINVAL;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *restrict attr,
                              int *restrict type) {
    if (!attr || !type) {
        return EINVAL;
    }
    *type = attr->__type;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (!attr ||
        (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_ERRORCHECK &&
         type != PTHREAD_MUTEX_RECURSIVE)) {
        return EINVAL;
    }
    attr->__type = type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
                       const pthread_mutexattr_t *restrict attr) {
    if (!mutex) {
        return EINVAL;
    }
    mutex->__state = 0u;
    mutex->__type = attr ? attr->__type : PTHREAD_MUTEX_NORMAL;
    mutex->__owner = (pthread_t)0;
    mutex->__count = 0u;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (!mutex) {
        return EINVAL;
    }
    if (__atomic_load_n(&mutex->__state, __ATOMIC_ACQUIRE) != 0u) {
        return EBUSY;
    }
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (mutex->__type == PTHREAD_MUTEX_RECURSIVE &&
        __atomic_load_n(&mutex->__state, __ATOMIC_RELAXED) != 0u &&
        mutex->__owner == self) {
        mutex->__count++;
        return 0;
    }
    unsigned int expected = 0u;
    if (__atomic_compare_exchange_n(&mutex->__state, &expected, 1u, 0,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        mutex->__owner = self;
        mutex->__count = 1u;
        return 0;
    }
    return EBUSY;
}

int pthread_mutex_clocklock(pthread_mutex_t *restrict mutex, clockid_t clock_id,
                            const struct timespec *restrict abstime) {
    if (!mutex) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&mutex->__state, __ATOMIC_RELAXED) != 0u &&
        mutex->__owner == self) {
        if (mutex->__type == PTHREAD_MUTEX_RECURSIVE) {
            mutex->__count++;
            return 0;
        }
        if (mutex->__type == PTHREAD_MUTEX_ERRORCHECK) {
            return EDEADLK;
        }
    }

    unsigned int expected = 0u;
    if (__atomic_compare_exchange_n(&mutex->__state, &expected, 1u, 0,
                                    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        mutex->__owner = self;
        mutex->__count = 1u;
        return 0;
    }

    for (;;) {
        if (expected == 2u ||
            __atomic_compare_exchange_n(&mutex->__state, &expected, 2u, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            long r = futex_wait(&mutex->__state, 2u, abstime, clock_id);
            if (r == -ETIMEDOUT) {
                return ETIMEDOUT;
            }
            if (r == -EINVAL) {
                return EINVAL;
            }
        }
        expected = 0u;
        if (__atomic_compare_exchange_n(&mutex->__state, &expected, 2u, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            mutex->__owner = self;
            mutex->__count = 1u;
            return 0;
        }
    }
}

int pthread_mutex_timedlock(pthread_mutex_t *restrict mutex,
                            const struct timespec *restrict abstime) {
    return pthread_mutex_clocklock(mutex, CLOCK_REALTIME, abstime);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    return pthread_mutex_clocklock(mutex, CLOCK_MONOTONIC,
                                   (const struct timespec *)0);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (mutex->__type != PTHREAD_MUTEX_NORMAL) {
        if (__atomic_load_n(&mutex->__state, __ATOMIC_RELAXED) == 0u ||
            mutex->__owner != self) {
            return EPERM;
        }
        if (mutex->__type == PTHREAD_MUTEX_RECURSIVE && mutex->__count > 1u) {
            mutex->__count--;
            return 0;
        }
    }
    mutex->__owner = (pthread_t)0;
    mutex->__count = 0u;
    unsigned int prev =
        __atomic_exchange_n(&mutex->__state, 0u, __ATOMIC_RELEASE);
    if (prev == 2u) {
        (void)futex_wake(&mutex->__state, 1u);
    }
    return 0;
}

/* ---- Condition variables ---- */

int pthread_condattr_init(pthread_condattr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    attr->__clock_id = CLOCK_REALTIME;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr) {
    return attr ? 0 : EINVAL;
}

int pthread_condattr_getclock(const pthread_condattr_t *restrict attr,
                              clockid_t *restrict clock_id) {
    if (!attr || !clock_id) {
        return EINVAL;
    }
    *clock_id = attr->__clock_id;
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id) {
    if (!attr || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        return EINVAL;
    }
    attr->__clock_id = clock_id;
    return 0;
}

int pthread_cond_init(pthread_cond_t *restrict cond,
                      const pthread_condattr_t *restrict attr) {
    if (!cond) {
        return EINVAL;
    }
    cond->__seq = 0u;
    cond->__clock_id = attr ? attr->__clock_id : CLOCK_REALTIME;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    return cond ? 0 : EINVAL;
}

int pthread_cond_clockwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex, clockid_t clock_id,
                           const struct timespec *restrict abstime) {
    __libc_cancel_point();
    if (!cond || !mutex ||
        (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        return EINVAL;
    }
    unsigned int seq = __atomic_load_n(&cond->__seq, __ATOMIC_ACQUIRE);
    int urc = pthread_mutex_unlock(mutex);
    if (urc != 0) {
        return urc;
    }
    long r = futex_wait(&cond->__seq, seq, abstime, clock_id);
    /* POSIX: re-acquire mutex before acting upon cancellation or returning. */
    (void)pthread_mutex_lock(mutex);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r == -ETIMEDOUT) {
        return ETIMEDOUT;
    }
    if (r == -EINVAL) {
        return EINVAL;
    }
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex,
                           const struct timespec *restrict abstime) {
    return pthread_cond_clockwait(cond, mutex, cond->__clock_id, abstime);
}

int pthread_cond_wait(pthread_cond_t *restrict cond,
                      pthread_mutex_t *restrict mutex) {
    return pthread_cond_clockwait(cond, mutex, cond->__clock_id,
                                  (const struct timespec *)0);
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (!cond) {
        return EINVAL;
    }
    (void)__atomic_fetch_add(&cond->__seq, 1u, __ATOMIC_RELEASE);
    (void)futex_wake(&cond->__seq, 1u);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (!cond) {
        return EINVAL;
    }
    (void)__atomic_fetch_add(&cond->__seq, 1u, __ATOMIC_RELEASE);
    (void)futex_wake(&cond->__seq, (unsigned int)INT_MAX);
    return 0;
}

/* ---- Barriers ---- */

int pthread_barrierattr_init(pthread_barrierattr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    attr->__pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) {
    return attr ? 0 : EINVAL;
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier,
                         const pthread_barrierattr_t *restrict attr,
                         unsigned count) {
    (void)attr;
    if (!barrier || count == 0u) {
        return EINVAL;
    }
    barrier->__threshold = count;
    barrier->__count = 0u;
    barrier->__gen = 0u;
    barrier->__lock = 0u;
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    if (!barrier) {
        return EINVAL;
    }
    if (__atomic_load_n(&barrier->__count, __ATOMIC_ACQUIRE) != 0u) {
        return EBUSY;
    }
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
    if (!barrier || barrier->__threshold == 0u) {
        return EINVAL;
    }
    while (__atomic_exchange_n(&barrier->__lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
        cpu_pause();
    }
    unsigned int gen = barrier->__gen;
    barrier->__count++;
    if (barrier->__count >= barrier->__threshold) {
        barrier->__count = 0u;
        __atomic_store_n(&barrier->__gen, gen + 1u, __ATOMIC_RELEASE);
        __atomic_store_n(&barrier->__lock, 0u, __ATOMIC_RELEASE);
        (void)futex_wake(&barrier->__gen, (unsigned int)INT_MAX);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }
    __atomic_store_n(&barrier->__lock, 0u, __ATOMIC_RELEASE);
    while (__atomic_load_n(&barrier->__gen, __ATOMIC_ACQUIRE) == gen) {
        (void)futex_wait(&barrier->__gen, gen, (const struct timespec *)0,
                         CLOCK_MONOTONIC);
    }
    return 0;
}

/* ---- Spinlocks ---- */

int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
    (void)pshared;
    if (!lock) {
        return EINVAL;
    }
    __atomic_store_n(lock, 0u, __ATOMIC_RELAXED);
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock) {
    return lock ? 0 : EINVAL;
}

int pthread_spin_trylock(pthread_spinlock_t *lock) {
    if (!lock) {
        return EINVAL;
    }
    return __atomic_exchange_n(lock, 1u, __ATOMIC_ACQUIRE) == 0u ? 0 : EBUSY;
}

int pthread_spin_lock(pthread_spinlock_t *lock) {
    if (!lock) {
        return EINVAL;
    }
    while (__atomic_exchange_n(lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
        while (__atomic_load_n(lock, __ATOMIC_RELAXED) != 0u) {
            cpu_pause();
        }
    }
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock) {
    if (!lock) {
        return EINVAL;
    }
    __atomic_store_n(lock, 0u, __ATOMIC_RELEASE);
    return 0;
}

/* ---- Read-write locks ---- */

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    attr->__pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
    return attr ? 0 : EINVAL;
}

int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
                        const pthread_rwlockattr_t *restrict attr) {
    (void)attr;
    if (!rwlock) {
        return EINVAL;
    }
    rwlock->__state = 0u;
    rwlock->__waiters = 0u;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    if (__atomic_load_n(&rwlock->__state, __ATOMIC_ACQUIRE) != 0u) {
        return EBUSY;
    }
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    unsigned int cur = __atomic_load_n(&rwlock->__state, __ATOMIC_RELAXED);
    while (!(cur & RWLOCK_WRITER_BIT)) {
        if (__atomic_compare_exchange_n(&rwlock->__state, &cur, cur + 1u, 0,
                                        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
    return EBUSY;
}

int pthread_rwlock_clockrdlock(pthread_rwlock_t *restrict rwlock,
                               clockid_t clock_id,
                               const struct timespec *restrict abstime) {
    if (!rwlock) {
        return EINVAL;
    }
    for (;;) {
        unsigned int cur = __atomic_load_n(&rwlock->__state, __ATOMIC_RELAXED);
        if (!(cur & RWLOCK_WRITER_BIT)) {
            if (__atomic_compare_exchange_n(&rwlock->__state, &cur, cur + 1u, 0,
                                            __ATOMIC_ACQUIRE,
                                            __ATOMIC_RELAXED)) {
                return 0;
            }
            continue;
        }
        (void)__atomic_fetch_add(&rwlock->__waiters, 1u, __ATOMIC_RELAXED);
        long r = futex_wait(&rwlock->__state, cur, abstime, clock_id);
        (void)__atomic_fetch_sub(&rwlock->__waiters, 1u, __ATOMIC_RELAXED);
        if (r == -ETIMEDOUT) {
            return ETIMEDOUT;
        }
        if (r == -EINVAL) {
            return EINVAL;
        }
    }
}

int pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime) {
    return pthread_rwlock_clockrdlock(rwlock, CLOCK_REALTIME, abstime);
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    return pthread_rwlock_clockrdlock(rwlock, CLOCK_MONOTONIC,
                                      (const struct timespec *)0);
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    unsigned int expected = 0u;
    if (__atomic_compare_exchange_n(&rwlock->__state, &expected,
                                    RWLOCK_WRITER_BIT, 0, __ATOMIC_ACQUIRE,
                                    __ATOMIC_RELAXED)) {
        return 0;
    }
    return EBUSY;
}

int pthread_rwlock_clockwrlock(pthread_rwlock_t *restrict rwlock,
                               clockid_t clock_id,
                               const struct timespec *restrict abstime) {
    if (!rwlock) {
        return EINVAL;
    }
    for (;;) {
        unsigned int expected = 0u;
        if (__atomic_compare_exchange_n(&rwlock->__state, &expected,
                                        RWLOCK_WRITER_BIT, 0, __ATOMIC_ACQUIRE,
                                        __ATOMIC_RELAXED)) {
            return 0;
        }
        (void)__atomic_fetch_add(&rwlock->__waiters, 1u, __ATOMIC_RELAXED);
        long r = futex_wait(&rwlock->__state, expected, abstime, clock_id);
        (void)__atomic_fetch_sub(&rwlock->__waiters, 1u, __ATOMIC_RELAXED);
        if (r == -ETIMEDOUT) {
            return ETIMEDOUT;
        }
        if (r == -EINVAL) {
            return EINVAL;
        }
    }
}

int pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime) {
    return pthread_rwlock_clockwrlock(rwlock, CLOCK_REALTIME, abstime);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    return pthread_rwlock_clockwrlock(rwlock, CLOCK_MONOTONIC,
                                      (const struct timespec *)0);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    if (!rwlock) {
        return EINVAL;
    }
    unsigned int cur = __atomic_load_n(&rwlock->__state, __ATOMIC_RELAXED);
    if (cur == RWLOCK_WRITER_BIT) {
        __atomic_store_n(&rwlock->__state, 0u, __ATOMIC_RELEASE);
        if (__atomic_load_n(&rwlock->__waiters, __ATOMIC_ACQUIRE) > 0u) {
            (void)futex_wake(&rwlock->__state, (unsigned int)INT_MAX);
        }
        return 0;
    }
    if (cur == 0u) {
        return EPERM;
    }
    unsigned int prev =
        __atomic_fetch_sub(&rwlock->__state, 1u, __ATOMIC_RELEASE);
    if (prev == 1u &&
        __atomic_load_n(&rwlock->__waiters, __ATOMIC_ACQUIRE) > 0u) {
        (void)futex_wake(&rwlock->__state, (unsigned int)INT_MAX);
    }
    return 0;
}
