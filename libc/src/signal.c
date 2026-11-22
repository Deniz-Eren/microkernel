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
#include <stdio.h>
#include <sys/microkernel.h>
#include <sys/signal.h>
#include <sys/thread.h>
#include <string.h>
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
    long r = __sys2(SYS_KILL, (long)pid, (long)sig);
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

int sigsuspend(const sigset_t *sigmask) {
    __libc_cancel_point();
    if (!sigmask) {
        errno = EFAULT;
        return -1;
    }
    uint32_t mask = (uint32_t)*sigmask;
    long r = __sys1(SYS_SIGSUSPEND, (long)(uintptr_t)&mask);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    errno = (r < 0) ? (int)(-r) : EINTR;
    return -1;
}

static void __k_to_siginfo(const struct __k_siginfo *ki, siginfo_t *info) {
    memset(info, 0, sizeof(*info));
    info->si_signo = ki->signo;
    info->si_errno = ki->err;
    info->si_code = (ki->code == K_SI_QUEUE) ? SI_QUEUE : SI_USER;
    info->si_pid = (pid_t)ki->pid;
    info->si_uid = (uid_t)ki->uid;
    info->si_value.sival_ptr = (void *)(uintptr_t)ki->value;
    info->si_status = ki->status;
}

int sigtimedwait(const sigset_t *restrict set, siginfo_t *restrict info,
                 const struct timespec *restrict timeout) {
    __libc_cancel_point();
    if (!set) {
        errno = EINVAL;
        return -1;
    }
    uint32_t kset = (uint32_t)*set;
    struct __k_siginfo ki = {0};
    long r =
        __sys4(SYS_SIGTIMEDWAIT, (long)(uintptr_t)&kset, (long)(uintptr_t)&ki,
               (long)(uintptr_t)timeout, (long)CLOCK_MONOTONIC);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (info) {
        __k_to_siginfo(&ki, info);
    }
    return (int)r;
}

int sigwaitinfo(const sigset_t *restrict set, siginfo_t *restrict info) {
    return sigtimedwait(set, info, (const struct timespec *)0);
}

int sigwait(const sigset_t *restrict set, int *restrict sig) {
    __libc_cancel_point();
    if (!set) {
        return EINVAL;
    }
    uint32_t kset = (uint32_t)*set;
    for (;;) {
        long r = __sys4(SYS_SIGTIMEDWAIT, (long)(uintptr_t)&kset, 0, 0,
                        (long)CLOCK_MONOTONIC);
        if (r == -EINTR) {
            __libc_cancel_point();
            continue;
        }
        if (r < 0) {
            return (int)(-r);
        }
        if (sig) {
            *sig = (int)r;
        }
        return 0;
    }
}

int sigqueue(pid_t pid, int sig, const union sigval value) {
    uint64_t raw = (uint64_t)(uintptr_t)value.sival_ptr;
    long r = __sys3(SYS_SIGQUEUE, (long)pid, (long)(uintptr_t)&raw, (long)sig);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

static const char *const g_sig_names[NSIG] = {
    [0] = "0",          [SIGHUP] = "HUP",       [SIGINT] = "INT",
    [SIGQUIT] = "QUIT", [SIGILL] = "ILL",       [SIGTRAP] = "TRAP",
    [SIGABRT] = "ABRT", [SIGBUS] = "BUS",       [SIGFPE] = "FPE",
    [SIGKILL] = "KILL", [SIGUSR1] = "USR1",     [SIGSEGV] = "SEGV",
    [SIGUSR2] = "USR2", [SIGPIPE] = "PIPE",     [SIGALRM] = "ALRM",
    [SIGTERM] = "TERM", [SIGCHLD] = "CHLD",     [SIGCONT] = "CONT",
    [SIGSTOP] = "STOP", [SIGTSTP] = "TSTP",     [SIGTTIN] = "TTIN",
    [SIGTTOU] = "TTOU", [SIGURG] = "URG",       [SIGXCPU] = "XCPU",
    [SIGXFSZ] = "XFSZ", [SIGVTALRM] = "VTALRM", [SIGPROF] = "PROF",
    [SIGSYS] = "SYS",
};

static const char *const g_sig_desc[NSIG] = {
    [0] = "Signal 0",
    [SIGHUP] = "Hangup",
    [SIGINT] = "Interrupt",
    [SIGQUIT] = "Quit",
    [SIGILL] = "Illegal instruction",
    [SIGTRAP] = "Trace trap",
    [SIGABRT] = "Abort",
    [SIGBUS] = "Bus error",
    [SIGFPE] = "Arithmetic exception",
    [SIGKILL] = "Killed",
    [SIGUSR1] = "User defined signal 1",
    [SIGSEGV] = "Invalid memory reference",
    [SIGUSR2] = "User defined signal 2",
    [SIGPIPE] = "Broken pipe",
    [SIGALRM] = "Alarm clock",
    [SIGTERM] = "Terminated",
    [SIGCHLD] = "Child process status changed",
    [SIGCONT] = "Continued",
    [SIGSTOP] = "Stopped (signal)",
    [SIGTSTP] = "Stopped (user request from keyboard)",
    [SIGTTIN] = "Stopped (background read from terminal)",
    [SIGTTOU] = "Stopped (background write to terminal)",
    [SIGURG] = "Urgent socket condition",
    [SIGXCPU] = "CPU time limit exceeded",
    [SIGXFSZ] = "File size limit exceeded",
    [SIGVTALRM] = "Virtual timer expired",
    [SIGPROF] = "Profiling timer expired",
    [SIGSYS] = "Bad system call",
};

int sig2str(int signum, char *str) {
    if (!str || signum <= 0 || signum >= NSIG) {
        return -1;
    }
    const char *name = g_sig_names[signum];
    if (!name) {
        return -1;
    }
    size_t len = strlen(name);
    if (len >= SIG2STR_MAX) {
        return -1;
    }
    memcpy(str, name, len + 1u);
    return 0;
}

int str2sig(const char *restrict str, int *restrict pnum) {
    if (!str || !*str || !pnum) {
        return -1;
    }
    int all_digits = 1;
    for (const char *c = str; *c; c++) {
        if (*c < '0' || *c > '9') {
            all_digits = 0;
            break;
        }
    }
    if (all_digits) {
        long v = 0;
        for (const char *c = str; *c; c++) {
            v = v * 10 + (*c - '0');
            if (v > 9999) {
                break;
            }
        }
        if (v >= 0 && v < NSIG && (v == 0 || g_sig_names[v])) {
            *pnum = (int)v;
            return 0;
        }
        return -1;
    }
    for (int i = 1; i < NSIG; i++) {
        const char *n = g_sig_names[i];
        if (n && strcmp(n, str) == 0) {
            *pnum = i;
            return 0;
        }
    }
    return -1;
}

char *strsignal(int sig) {
    static char unknown[32];
    if (sig > 0 && sig < NSIG && g_sig_desc[sig]) {
        return (char *)g_sig_desc[sig];
    }
    (void)snprintf(unknown, sizeof(unknown), "Unknown signal %d", sig);
    return unknown;
}

void psignal(int sig, const char *message) {
    if (message && *message) {
        (void)fprintf(stderr, "%s: %s\n", message, strsignal(sig));
    } else {
        (void)fprintf(stderr, "%s\n", strsignal(sig));
    }
}

void psiginfo(const siginfo_t *pinfo, const char *message) {
    if (!pinfo) {
        psignal(0, message);
        return;
    }
    if (message && *message) {
        (void)fprintf(stderr, "%s: %s\n", message, strsignal(pinfo->si_signo));
    } else {
        (void)fprintf(stderr, "%s\n", strsignal(pinfo->si_signo));
    }
}
