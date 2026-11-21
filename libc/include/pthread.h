/**
 * @file     pthread.h
 * @brief    POSIX.1-2024 Base threads, mutexes, condvars, barriers, and locks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

/* need:: ICD_PROC_0006 */

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED 1

#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_ERRORCHECK 1
#define PTHREAD_MUTEX_RECURSIVE 2
#define PTHREAD_MUTEX_DEFAULT PTHREAD_MUTEX_NORMAL

#define PTHREAD_BARRIER_SERIAL_THREAD (-1)

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void *)(-1))

struct __pthread_cleanup_frame {
    void (*__routine)(void *);
    void *__arg;
    struct __pthread_cleanup_frame *__prev;
};

typedef struct {
    int __detachstate;
} pthread_attr_t;

typedef struct {
    int __type;
} pthread_mutexattr_t;

typedef struct {
    unsigned int __state;
    int __type;
    pthread_t __owner;
    unsigned int __count;
} pthread_mutex_t;

typedef struct {
    clockid_t __clock_id;
} pthread_condattr_t;

typedef struct {
    unsigned int __seq;
    clockid_t __clock_id;
} pthread_cond_t;

typedef struct {
    int __pshared;
} pthread_barrierattr_t;

typedef struct {
    unsigned int __threshold;
    unsigned int __count;
    unsigned int __gen;
    unsigned int __lock;
} pthread_barrier_t;

typedef unsigned int pthread_spinlock_t;

typedef struct {
    int __pshared;
} pthread_rwlockattr_t;

typedef struct {
    unsigned int __state; /* 0 = free, >0 = readers, 0x80000000 = writer */
    unsigned int __waiters;
} pthread_rwlock_t;

typedef unsigned int pthread_once_t;
typedef unsigned int pthread_key_t;

#define PTHREAD_MUTEX_INITIALIZER                                              \
    { 0u, PTHREAD_MUTEX_NORMAL, 0u, 0u }
#define PTHREAD_COND_INITIALIZER                                               \
    { 0u, CLOCK_REALTIME }
#define PTHREAD_RWLOCK_INITIALIZER                                             \
    { 0u, 0u }
#define PTHREAD_ONCE_INIT 0u

int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);

int pthread_create(pthread_t *restrict thread,
                   const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_detach(pthread_t thread);
_Noreturn void pthread_exit(void *value_ptr);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

int pthread_cancel(pthread_t thread);
int pthread_setcancelstate(int state, int *oldstate);
int pthread_setcanceltype(int type, int *oldtype);
void pthread_testcancel(void);
void __pthread_cleanup_push(struct __pthread_cleanup_frame *__frame,
                            void (*__routine)(void *), void *__arg);
void __pthread_cleanup_pop(struct __pthread_cleanup_frame *__frame,
                           int __execute);

#define pthread_cleanup_push(routine, arg)                                     \
    do {                                                                       \
        struct __pthread_cleanup_frame __cl_frame;                             \
    __pthread_cleanup_push(&__cl_frame, (routine), (arg))

#define pthread_cleanup_pop(execute)                                           \
    __pthread_cleanup_pop(&__cl_frame, (execute));                             \
    }                                                                          \
    while (0)

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *restrict attr,
                              int *restrict type);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

int pthread_mutex_init(pthread_mutex_t *restrict mutex,
                       const pthread_mutexattr_t *restrict attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_timedlock(pthread_mutex_t *restrict mutex,
                            const struct timespec *restrict abstime);
int pthread_mutex_clocklock(pthread_mutex_t *restrict mutex, clockid_t clock_id,
                            const struct timespec *restrict abstime);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_condattr_init(pthread_condattr_t *attr);
int pthread_condattr_destroy(pthread_condattr_t *attr);
int pthread_condattr_getclock(const pthread_condattr_t *restrict attr,
                              clockid_t *restrict clock_id);
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id);

int pthread_cond_init(pthread_cond_t *restrict cond,
                      const pthread_condattr_t *restrict attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *restrict cond,
                      pthread_mutex_t *restrict mutex);
int pthread_cond_timedwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex,
                           const struct timespec *restrict abstime);
int pthread_cond_clockwait(pthread_cond_t *restrict cond,
                           pthread_mutex_t *restrict mutex, clockid_t clock_id,
                           const struct timespec *restrict abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_barrierattr_init(pthread_barrierattr_t *attr);
int pthread_barrierattr_destroy(pthread_barrierattr_t *attr);
int pthread_barrier_init(pthread_barrier_t *restrict barrier,
                         const pthread_barrierattr_t *restrict attr,
                         unsigned count);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
int pthread_barrier_wait(pthread_barrier_t *barrier);

int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int pthread_spin_destroy(pthread_spinlock_t *lock);
int pthread_spin_lock(pthread_spinlock_t *lock);
int pthread_spin_trylock(pthread_spinlock_t *lock);
int pthread_spin_unlock(pthread_spinlock_t *lock);

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
                        const pthread_rwlockattr_t *restrict attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime);
int pthread_rwlock_clockrdlock(pthread_rwlock_t *restrict rwlock,
                               clockid_t clock_id,
                               const struct timespec *restrict abstime);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,
                               const struct timespec *restrict abstime);
int pthread_rwlock_clockwrlock(pthread_rwlock_t *restrict rwlock,
                               clockid_t clock_id,
                               const struct timespec *restrict abstime);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

#endif /* _PTHREAD_H */
