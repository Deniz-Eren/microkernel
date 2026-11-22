/**
 * @file     _pthread_types.h
 * @brief    Internal shared thread and synchronization layouts for libc headers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS__PTHREAD_TYPES_H
#define _SYS__PTHREAD_TYPES_H

/* need:: ICD_PROC_0006 */

typedef unsigned int __pthread_t;
typedef int __clockid_t;
typedef unsigned int __pthread_once_t;
typedef unsigned int __pthread_key_t;

struct __pthread_mutex_s {
    unsigned int __state;
    int __type;
    __pthread_t __owner;
    unsigned int __count;
    int __robust;
    int __prioceiling;
    unsigned int __rflags;
    struct __pthread_mutex_s *__next_robust;
};

struct __pthread_cond_s {
    unsigned int __seq;
    __clockid_t __clock_id;
};

#endif /* _SYS__PTHREAD_TYPES_H */
