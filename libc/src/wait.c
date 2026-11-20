/**
 * @file     wait.c
 * @brief    POSIX.1-2024 async-signal-safe wait and waitpid wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <sys/proc.h>
#include <sys/wait.h>

/* need:: COMP_PROC_0003 */

pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    if ((options & ~(WNOHANG | WUNTRACED)) != 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned kopts = (options & WNOHANG) ? PROC_WAIT_NOHANG : 0u;
    long r = ProcWait((long)pid, stat_loc, kopts);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (pid_t)r;
}

pid_t wait(int *stat_loc) {
    return waitpid((pid_t)-1, stat_loc, 0);
}
