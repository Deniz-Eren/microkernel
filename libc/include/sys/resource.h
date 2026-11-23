/**
 * @file     resource.h
 * @brief    POSIX.1-2024 Base resource limit types, constants, and functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

/* need:: ICD_PROC_0010 */

#include <sys/time.h>
#include <sys/types.h>

typedef unsigned long rlim_t;

#define RLIM_INFINITY (~(rlim_t)0UL)
#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY

#define RLIMIT_CORE 0
#define RLIMIT_CPU 1
#define RLIMIT_DATA 2
#define RLIMIT_FSIZE 3
#define RLIMIT_NOFILE 4
#define RLIMIT_STACK 5
#define RLIMIT_AS 6

#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};

int getrlimit(int resource, struct rlimit *rlp);
int setrlimit(int resource, const struct rlimit *rlp);
int getrusage(int who, struct rusage *r_usage);

#endif /* _SYS_RESOURCE_H */
