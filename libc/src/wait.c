/**
 * @file     wait.c
 * @brief    POSIX.1-2024 async-signal-safe wait, waitpid, and waitid wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/proc.h>
#include <sys/wait.h>
#include "tls_internal.h"

/* need:: COMP_PROC_0003 */

pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    __libc_cancel_point();
    if ((options & ~(WNOHANG | WUNTRACED | WCONTINUED)) != 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned kopts = PROC_WAIT_EXITED;
    if (options & WNOHANG) {
        kopts |= PROC_WAIT_NOHANG;
    }
    if (options & WUNTRACED) {
        kopts |= PROC_WAIT_STOP;
    }
    if (options & WCONTINUED) {
        kopts |= PROC_WAIT_CONT;
    }
    long r = ProcWait((long)pid, stat_loc, kopts);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (pid_t)r;
}

pid_t wait(int *stat_loc) {
    return waitpid((pid_t)-1, stat_loc, 0);
}

int waitid(idtype_t idtype, id_t id, siginfo_t *restrict infop, int options) {
    __libc_cancel_point();
    if (!infop) {
        errno = EINVAL;
        return -1;
    }
    if ((options & (WEXITED | WSTOPPED | WCONTINUED)) == 0) {
        errno = EINVAL;
        return -1;
    }
    if ((options & ~(WEXITED | WSTOPPED | WCONTINUED | WNOHANG | WNOWAIT)) !=
        0) {
        errno = EINVAL;
        return -1;
    }

    long kpid;
    switch (idtype) {
    case P_ALL:
        kpid = -1;
        break;
    case P_PID:
        if (id <= 0) {
            errno = EINVAL;
            return -1;
        }
        kpid = (long)id;
        break;
    case P_PGID:
        if (id <= 0) {
            errno = EINVAL;
            return -1;
        }
        kpid = -(long)id;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    unsigned kopts = PROC_WAIT_EXT;
    if (options & WEXITED) {
        kopts |= PROC_WAIT_EXITED;
    }
    if (options & WNOHANG) {
        kopts |= PROC_WAIT_NOHANG;
    }
    if (options & WNOWAIT) {
        kopts |= PROC_WAIT_NOWAIT;
    }
    if (options & WSTOPPED) {
        kopts |= PROC_WAIT_STOP;
    }
    if (options & WCONTINUED) {
        kopts |= PROC_WAIT_CONT;
    }

    struct __k_wait_ext ext = {0, 0, 0, 0};
    long r = __sys3(SYS_PROC_WAIT, kpid, (long)(uintptr_t)&ext, kopts);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (r == 0 && (options & WNOHANG)) {
        memset(infop, 0, sizeof(*infop));
        return 0;
    }

    memset(infop, 0, sizeof(*infop));
    infop->si_signo = SIGCHLD;
    infop->si_errno = 0;
    infop->si_pid = (pid_t)ext.pid;
    infop->si_uid = (uid_t)ext.uid;
    if (WIFEXITED(ext.status)) {
        infop->si_code = CLD_EXITED;
        infop->si_status = WEXITSTATUS(ext.status);
    } else if (WIFCONTINUED(ext.status)) {
        infop->si_code = CLD_CONTINUED;
        infop->si_status = SIGCONT;
    } else if (WIFSTOPPED(ext.status)) {
        infop->si_code = CLD_STOPPED;
        infop->si_status = WSTOPSIG(ext.status);
    } else if (WIFSIGNALED(ext.status)) {
        infop->si_code = CLD_KILLED;
        infop->si_status = WTERMSIG(ext.status);
    } else {
        infop->si_code = CLD_EXITED;
        infop->si_status = WEXITSTATUS(ext.status);
    }
    return 0;
}
