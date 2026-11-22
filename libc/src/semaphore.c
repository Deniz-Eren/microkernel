/**
 * @file     semaphore.c
 * @brief    POSIX.1-2024 Base unnamed and MAP_SHARED named semaphores
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <unistd.h>
#include "tls_internal.h"

/* need:: COMP_PROC_0006 */

#define NAMED_SEM_PAGE_SIZE 4096u
#define NAMED_SEM_MAGIC 0x53454D31u
#define NAMED_SEM_MAX 32
#define NAMED_SEM_SHM_PREFIX "/__sem_"
#define NAMED_SEM_NAME_MAX 48u

struct __named_sem_page {
    sem_t sem;
    unsigned int magic;
};

struct __open_sem_slot {
    uint32_t shm_id;
    unsigned int refcount;
    unsigned long long uid;
    sem_t *addr;
};

static pthread_mutex_t g_open_sem_lock = PTHREAD_MUTEX_INITIALIZER;
static struct __open_sem_slot g_open_sems[NAMED_SEM_MAX];
static unsigned int g_uid_seq;

static int sem_build_shm_name(const char *name, char *out, size_t cap) {
    if (!name || name[0] != '/' || name[1] == '\0') {
        return -EINVAL;
    }
    size_t nlen = strlen(name);
    if (nlen > NAMED_SEM_NAME_MAX) {
        return -ENAMETOOLONG;
    }
    for (size_t i = 1u; i < nlen; i++) {
        if (name[i] == '/') {
            return -EINVAL;
        }
    }
    size_t plen = sizeof(NAMED_SEM_SHM_PREFIX) - 1u;
    if (plen + (nlen - 1u) + 1u > cap) {
        return -ENAMETOOLONG;
    }
    memcpy(out, NAMED_SEM_SHM_PREFIX, plen);
    memcpy(out + plen, name + 1, nlen); /* includes NUL */
    return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value) {
    if (!sem || value > (unsigned)SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    (void)pshared;
    __atomic_store_n(&sem->__val, value, __ATOMIC_RELAXED);
    __atomic_store_n(&sem->__waiters, 0u, __ATOMIC_RELAXED);
    sem->__uid = 0ULL;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem || sem->__uid != 0ULL) {
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
            errno = EINTR;
            return -1;
        }
        if (r == -ETIMEDOUT || r == -EINVAL || r == -EFAULT) {
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

sem_t *sem_open(const char *name, int oflag, ...) {
    char shm_name[64];
    int nr = sem_build_shm_name(name, shm_name, sizeof(shm_name));
    if (nr < 0) {
        errno = -nr;
        return SEM_FAILED;
    }
    if (oflag & ~(O_CREAT | O_EXCL)) {
        errno = EINVAL;
        return SEM_FAILED;
    }
    mode_t mode = 0666u;
    unsigned int value = 0u;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = (mode_t)va_arg(ap, unsigned int);
        value = (unsigned int)va_arg(ap, unsigned int);
        va_end(ap);
        if (value > (unsigned int)SEM_VALUE_MAX) {
            errno = EINVAL;
            return SEM_FAILED;
        }
    }

    int created = 0;
    long fd = -1;
    if (oflag & O_CREAT) {
        fd = __sys4(SYS_SHM_OP, SHM_OP_OPEN, (long)(uintptr_t)shm_name,
                    (long)(O_RDWR | O_CREAT | O_EXCL), (long)(mode & 0777u));
        if (fd >= 0) {
            created = 1;
        } else if (fd == -EEXIST && !(oflag & O_EXCL)) {
            fd = __sys4(SYS_SHM_OP, SHM_OP_OPEN, (long)(uintptr_t)shm_name,
                        (long)O_RDWR, 0);
        }
    } else {
        fd = __sys4(SYS_SHM_OP, SHM_OP_OPEN, (long)(uintptr_t)shm_name,
                    (long)O_RDWR, 0);
    }
    if (fd < 0) {
        errno = (int)(-fd);
        return SEM_FAILED;
    }

    if (created) {
        long tr = __sys4(SYS_SHM_OP, SHM_OP_TRUNCATE, fd,
                         (long)NAMED_SEM_PAGE_SIZE, 0);
        if (tr < 0) {
            (void)close((int)fd);
            (void)__sys4(SYS_SHM_OP, SHM_OP_UNLINK, (long)(uintptr_t)shm_name,
                         0, 0);
            errno = (int)(-tr);
            return SEM_FAILED;
        }
    }

    struct __k_shm_stat ks = {0};
    for (int tries = 0; tries < 200; tries++) {
        long sr = __sys4(SYS_SHM_OP, SHM_OP_STAT, fd, (long)(uintptr_t)&ks, 0);
        if (sr < 0) {
            (void)close((int)fd);
            errno = (int)(-sr);
            return SEM_FAILED;
        }
        if (ks.size >= NAMED_SEM_PAGE_SIZE) {
            break;
        }
        struct timespec pause_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
        (void)__sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0,
                     (long)(uintptr_t)&pause_ts, 0);
    }
    if (ks.size < NAMED_SEM_PAGE_SIZE) {
        (void)close((int)fd);
        errno = EAGAIN;
        return SEM_FAILED;
    }

    long va =
        __sys6(SYS_SHM_MAP, 0, (long)NAMED_SEM_PAGE_SIZE,
               (long)(VM_PROT_READ | VM_PROT_WRITE), (long)MAP_SHARED, fd, 0);
    (void)close((int)fd);
    if (va < 0) {
        if (created) {
            (void)__sys4(SYS_SHM_OP, SHM_OP_UNLINK, (long)(uintptr_t)shm_name,
                         0, 0);
        }
        errno = (int)(-va);
        return SEM_FAILED;
    }

    struct __named_sem_page *pg = (struct __named_sem_page *)(uintptr_t)va;
    if (created) {
        unsigned int seq = __atomic_add_fetch(&g_uid_seq, 1u, __ATOMIC_RELAXED);
        unsigned long long uid =
            ((unsigned long long)(uint32_t)getpid() << 32) | (seq ? seq : 1u);
        __atomic_store_n(&pg->sem.__val, value, __ATOMIC_RELAXED);
        __atomic_store_n(&pg->sem.__waiters, 0u, __ATOMIC_RELAXED);
        pg->sem.__uid = uid;
        __atomic_store_n(&pg->magic, NAMED_SEM_MAGIC, __ATOMIC_RELEASE);
    } else {
        for (int tries = 0; tries < 200; tries++) {
            if (__atomic_load_n(&pg->magic, __ATOMIC_ACQUIRE) ==
                NAMED_SEM_MAGIC) {
                break;
            }
            struct timespec pause_ts = {.tv_sec = 0, .tv_nsec = 1000000L};
            (void)__sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0,
                         (long)(uintptr_t)&pause_ts, 0);
        }
    }

    unsigned long long uid = pg->sem.__uid;
    (void)pthread_mutex_lock(&g_open_sem_lock);
    for (int i = 0; i < NAMED_SEM_MAX; i++) {
        if (g_open_sems[i].addr && g_open_sems[i].shm_id == ks.id &&
            g_open_sems[i].uid == uid) {
            g_open_sems[i].refcount++;
            sem_t *existing = g_open_sems[i].addr;
            (void)pthread_mutex_unlock(&g_open_sem_lock);
            (void)munmap(pg, NAMED_SEM_PAGE_SIZE);
            return existing;
        }
    }
    int slot = -1;
    for (int i = 0; i < NAMED_SEM_MAX; i++) {
        if (!g_open_sems[i].addr) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        (void)pthread_mutex_unlock(&g_open_sem_lock);
        (void)munmap(pg, NAMED_SEM_PAGE_SIZE);
        errno = EMFILE;
        return SEM_FAILED;
    }
    g_open_sems[slot].shm_id = ks.id;
    g_open_sems[slot].refcount = 1u;
    g_open_sems[slot].uid = uid;
    g_open_sems[slot].addr = &pg->sem;
    (void)pthread_mutex_unlock(&g_open_sem_lock);
    return &pg->sem;
}

int sem_close(sem_t *sem) {
    if (!sem) {
        errno = EINVAL;
        return -1;
    }
    int do_unmap = 0;
    (void)pthread_mutex_lock(&g_open_sem_lock);
    int found = -1;
    for (int i = 0; i < NAMED_SEM_MAX; i++) {
        if (g_open_sems[i].addr == sem) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        (void)pthread_mutex_unlock(&g_open_sem_lock);
        errno = EINVAL;
        return -1;
    }
    if (g_open_sems[found].refcount > 1u) {
        g_open_sems[found].refcount--;
    } else {
        g_open_sems[found].shm_id = 0u;
        g_open_sems[found].refcount = 0u;
        g_open_sems[found].uid = 0ULL;
        g_open_sems[found].addr = (sem_t *)0;
        do_unmap = 1;
    }
    (void)pthread_mutex_unlock(&g_open_sem_lock);
    if (do_unmap) {
        (void)munmap(sem, NAMED_SEM_PAGE_SIZE);
    }
    return 0;
}

int sem_unlink(const char *name) {
    char shm_name[64];
    int nr = sem_build_shm_name(name, shm_name, sizeof(shm_name));
    if (nr < 0) {
        errno = -nr;
        return -1;
    }
    long r = __sys4(SYS_SHM_OP, SHM_OP_UNLINK, (long)(uintptr_t)shm_name, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}
