/**
 * @file     semaphore.h
 * @brief    POSIX.1-2024 Base unnamed semaphore declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

/* need:: ICD_PROC_0007 */

#include <fcntl.h>
#include <sys/types.h>
#include <time.h>

#define SEM_FAILED ((sem_t *)0)
#define SEM_VALUE_MAX 0x7FFFFFFF

typedef struct {
    unsigned int __val;
    unsigned int __waiters;
    unsigned long long __uid;
} sem_t;

int sem_init(sem_t *sem, int pshared, unsigned value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_timedwait(sem_t *restrict sem, const struct timespec *restrict abstime);
int sem_clockwait(sem_t *restrict sem, clockid_t clock_id,
                  const struct timespec *restrict abstime);
int sem_post(sem_t *sem);
int sem_getvalue(sem_t *restrict sem, int *restrict sval);

sem_t *sem_open(const char *name, int oflag, ...);
int sem_close(sem_t *sem);
int sem_unlink(const char *name);

#endif /* _SEMAPHORE_H */
