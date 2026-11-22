/**
 * @file     signal.h
 * @brief    POSIX.1-2024 signal types, constants, and function declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

/* need:: ICD_SIG_0002 */

#include <sys/signal.h>
#include <sys/types.h>
#include <time.h>

typedef volatile int sig_atomic_t;
typedef unsigned int sigset_t;

#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define SIG_ERR ((void (*)(int)) - 1)

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

#ifndef _SIGEVENT_DEFINED
#define _SIGEVENT_DEFINED
#define SIGEV_NONE 0
#define SIGEV_SIGNAL 1
#define SIGEV_THREAD 2

union sigval {
    int sival_int;
    void *sival_ptr;
};

struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
};
#endif /* _SIGEVENT_DEFINED */

#define SI_USER 0
#define SI_QUEUE (-1)
#define SI_TIMER (-2)
#define SI_ASYNCIO (-3)
#define SI_MESGQ (-4)

#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6

typedef struct {
    int si_signo;
    int si_code;
    int si_errno;
    pid_t si_pid;
    uid_t si_uid;
    void *si_addr;
    int si_status;
    long si_band;
    union sigval si_value;
} siginfo_t;

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signo);
int sigdelset(sigset_t *set, int signo);
int sigismember(const sigset_t *set, int signo);

int sigaction(int sig, const struct sigaction *restrict act,
              struct sigaction *restrict oact);
int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset);
int sigpending(sigset_t *set);
int pthread_sigmask(int how, const sigset_t *restrict set,
                    sigset_t *restrict oset);
int kill(pid_t pid, int sig);
int pthread_kill(pthread_t thread, int sig);
int raise(int sig);
void (*signal(int sig, void (*func)(int)))(int);

int sigsuspend(const sigset_t *sigmask);
int sigwait(const sigset_t *restrict set, int *restrict sig);
int sigwaitinfo(const sigset_t *restrict set, siginfo_t *restrict info);
int sigtimedwait(const sigset_t *restrict set, siginfo_t *restrict info,
                 const struct timespec *restrict timeout);
int sigqueue(pid_t pid, int sig, const union sigval value);

#define SIG2STR_MAX 32
int sig2str(int signum, char *str);
int str2sig(const char *restrict str, int *restrict pnum);
void psignal(int sig, const char *message);
void psiginfo(const siginfo_t *pinfo, const char *message);

#endif /* _SIGNAL_H */
