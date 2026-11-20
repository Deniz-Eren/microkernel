// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
/* Microkernel signals (Milestone 4). Handler registration is an ABI extension
 * (SYS_SIGACTION); delivery uses the kernel-built signal frame and libc's
 * __sigreturn_trampoline. */
#include <sys/microkernel.h>

#define NSIG    32
#define SIGINT  2
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12

extern void __sigreturn_trampoline(void);

/* Register a Ring 3 handler for sig (0 = restore default/terminate action).
 * The kernel uses __sigreturn_trampoline as the handler's return address. */
static inline long sigaction(unsigned sig, void (*handler)(int)) {
    return __sys3(SYS_SIGACTION, (long)sig, (long)handler,
                  (long)__sigreturn_trampoline);
}

/* kill (POSIX): send sig to thread tid of process pid (0 = self/current
 * thread). A thread blocked in IPC wakes with -EINTR. */
static inline long kill(unsigned pid, unsigned tid, unsigned sig) {
    return __sys3(SYS_THREAD_INJECT_SIG, (long)pid, (long)tid, (long)sig);
}
