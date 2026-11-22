/**
 * @file     proc.h
 * @brief    Ring 3 native process creation, child wait, and snapshot wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_PROC_0003 ICD_PROC_0009 ICD_PROC_0010 */
#include <stddef.h>
#include <stdint.h>
#include <sys/fd.h>
#include <sys/microkernel.h>

#define PROC_WAIT_NOHANG 1

#define PROC_COMM_MAX 32u
#define PROC_ARGS_MAX 64u
#define PROC_LIST_MAX 64u

#define PROC_INFO_ALIVE 0u
#define PROC_INFO_ZOMBIE 1u

struct __k_proc_info {
    uint32_t pid;
    uint32_t ppid;
    uint32_t pgid;
    uint32_t sid;
    uint32_t uid;
    uint32_t gid;
    int32_t nice;
    uint32_t state;
    uint32_t nthreads;
    uint32_t prio;
    uint32_t tty_kind;
    uint64_t utime_ticks;
    uint64_t stime_ticks;
    char comm[PROC_COMM_MAX];
    char args[PROC_ARGS_MAX];
};

/* Spawn a bootfs executable as a child of the caller. prio == 0 inherits the
 * caller's base priority. Returns the child PID (> 0) or negative errno. */
static inline long ProcCreate(const char *name, unsigned prio, unsigned flags) {
    return __sys3(SYS_PROC_CREATE, (long)(uintptr_t)name, (long)prio,
                  (long)flags);
}

/* Wait for a child process (pid > 0 specific child, pid == -1 any child).
 * Writes encoded wait status to *status if non-NULL. Returns child PID, 0 on
 * PROC_WAIT_NOHANG when still alive, or negative errno. */
static inline long ProcWait(long pid, int *status, unsigned options) {
    return __sys3(SYS_PROC_WAIT, pid, (long)(uintptr_t)status, (long)options);
}

/* Read a bounded snapshot of active and zombie processes into buf. Passing
 * buf == NULL and max_entries == 0 returns the total active+zombie count. */
static inline long ProcList(struct __k_proc_info *buf, size_t max_entries) {
    return __sys5(SYS_FD_OP, FD_OP_PROC_LIST, (long)(uintptr_t)buf,
                  (long)max_entries, 0, 0);
}

/* Query nice value (-20..19) for pid (0 = current process). Returns nice + 20
 * (>= 0) on success or negative errno on failure. */
static inline long ProcGetNice(int pid) {
    return __sys4(SYS_PROC_CRED, CRED_OP_GETNICE, (long)pid, 0, 0);
}

/* Set or adjust nice value (-20..19) for pid (0 = current process). When
 * is_delta != 0, val is added to current nice; otherwise nice is set to val.
 * Returns new nice + 20 (>= 0) on success or negative errno on failure. */
static inline long ProcSetNice(int pid, int val, int is_delta) {
    return __sys4(SYS_PROC_CRED, CRED_OP_SETNICE, (long)pid, (long)val,
                  (long)is_delta);
}
