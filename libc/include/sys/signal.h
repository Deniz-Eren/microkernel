/**
 * @file     signal.h
 * @brief    Kernel and libc shared signal numbers and action flags
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

/* need:: ICD_SIG_0001 */

#include <stdint.h>

#define NSIG 32

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGSYS 31

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_NOCLDSTOP 0x01
#define SA_RESETHAND 0x02
#define SA_NODEFER 0x04

/* Kernel/libc ABI structure for SYS_SIGACTION (not exposed by <signal.h>). */
struct __k_sigaction {
    uint64_t handler; /* 0 = SIG_DFL, 1 = SIG_IGN, else Ring 3 function */
    uint32_t mask;    /* additional signals blocked during handler */
    uint32_t flags;   /* SA_* flags */
};

#endif /* _SYS_SIGNAL_H */
