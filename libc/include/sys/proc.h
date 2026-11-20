/**
 * @file     proc.h
 * @brief    Ring 3 native process creation and child wait wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_PROC_0003 */
#include <stdint.h>
#include <sys/microkernel.h>

#define PROC_WAIT_NOHANG 1

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
