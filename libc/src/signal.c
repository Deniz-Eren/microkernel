/**
 * @file     signal.c
 * @brief    POSIX.1-2024 async-signal-safe signal wrappers and sigset helpers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include "tls_internal.h"

/* need:: COMP_SIG_0002 */

extern void __sigreturn_trampoline(void);

int sigemptyset(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = 0u;
    return 0;
}

int sigfillset(sigset_t *set) {
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    *set = ~0u;
    return 0;
}

int sigaddset(sigset_t *set, int signo) {
    if (!set || signo < 1 || signo >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set |= (1u << (unsigned int)signo);
    return 0;
}

int sigdelset(sigset_t *set, int signo) {
    if (!set || signo < 1 || signo >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~(1u << (unsigned int)signo);
    return 0;
}

int sigismember(const sigset_t *set, int signo) {
    if (!set || signo < 1 || signo >= NSIG) {
        errno = EINVAL;
        return -1;
    }
    return (*set & (1u << (unsigned int)signo)) ? 1 : 0;
}

int sigaction(int sig, const struct sigaction *restrict act,
              struct sigaction *restrict oact) {
    struct __k_sigaction kact = {0};
    struct __k_sigaction koact = {0};
    const struct __k_sigaction *act_ptr = (const struct __k_sigaction *)0;
    struct __k_sigaction *oact_ptr = (struct __k_sigaction *)0;

    if (act) {
        kact.handler = (uint64_t)(uintptr_t)act->sa_handler;
        kact.mask = (uint32_t)act->sa_mask;
        kact.flags = (uint32_t)act->sa_flags;
        act_ptr = &kact;
    }
    if (oact) {
        oact_ptr = &koact;
    }

    long r = __sys4(SYS_SIGACTION, (long)sig, (long)(uintptr_t)act_ptr,
                    (long)(uintptr_t)oact_ptr,
                    (long)(uintptr_t)__sigreturn_trampoline);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (oact) {
        oact->sa_handler = (void (*)(int))(uintptr_t)koact.handler;
        oact->sa_mask = (sigset_t)koact.mask;
        oact->sa_flags = (int)koact.flags;
    }
    return 0;
}

int pthread_sigmask(int how, const sigset_t *restrict set,
                    sigset_t *restrict oset) {
    uint32_t kset = 0;
    uint32_t koset = 0;
    const uint32_t *set_ptr = (const uint32_t *)0;
    uint32_t *oset_ptr = (uint32_t *)0;

    if (set) {
        kset = (uint32_t)*set;
        if (how == SIG_BLOCK || how == SIG_SETMASK) {
            kset &= ~(1u << LIBC_SIGCANCEL);
        }
        set_ptr = &kset;
    }
    if (oset) {
        oset_ptr = &koset;
    }

    long r = __sys3(SYS_SIGPROCMASK, (long)how, (long)(uintptr_t)set_ptr,
                    (long)(uintptr_t)oset_ptr);
    if (r < 0) {
        return (int)(-r);
    }
    if (oset) {
        *oset = (sigset_t)koset;
    }
    return 0;
}

int sigpending(sigset_t *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }
    uint32_t kset = 0;
    long r = __sys1(SYS_SIGPENDING, (long)(uintptr_t)&kset);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    *set = (sigset_t)kset;
    return 0;
}

int sigprocmask(int how, const sigset_t *restrict set,
                sigset_t *restrict oset) {
    int err = pthread_sigmask(how, set, oset);
    if (err != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int kill(pid_t pid, int sig) {
    if (pid < 0) {
        errno = ENOSYS;
        return -1;
    }
    long r = __sys3(SYS_THREAD_INJECT_SIG, (long)pid, 0, (long)sig);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int pthread_kill(pthread_t thread, int sig) {
    long r = __sys3(SYS_THREAD_INJECT_SIG, 0, (long)thread, (long)sig);
    if (r < 0) {
        return (int)(-r);
    }
    return 0;
}

int raise(int sig) {
    thread_info_t ti = {0};
    long r = ThreadGetCtx(0, &ti);
    unsigned int tid = (r >= 0) ? ti.tid : 0u;
    int err = pthread_kill((pthread_t)tid, sig);
    if (err != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

void (*signal(int sig, void (*func)(int)))(int) {
    struct sigaction act = {0};
    struct sigaction oact = {0};
    act.sa_handler = func;
    if (sigaction(sig, &act, &oact) < 0) {
        return SIG_ERR;
    }
    return oact.sa_handler;
}
