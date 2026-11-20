/**
 * @file     main.c
 * @brief    Ring 3 multithreaded errno, thread, and POSIX signal test app
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/*
 * sigtest — Milestone 4 / POSIX.1-2024 verification: spawns a worker thread
 * (THREAD_CREATE + THREAD_RESET), verifies thread-local errno, tests POSIX
 * sigaction SIGKILL/SIGSTOP EINVAL, sigprocmask blocking and sig_frame_t mask
 * save/restore with an async-signal-safe handler, SIG_IGN, and unhandled
 * SIGUSR2 termination.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/microkernel.h>
#include <sys/ipc.h>
#include <sys/thread.h>

static volatile int g_worker_errno_ok;
static sig_atomic_t g_caught_sig;
static sig_atomic_t g_handler_saw_blocked;

static void worker(void *arg) {
    (void)arg;
    thread_info_t ti = {0};
    long r = ThreadGetCtx(0, &ti);
    if (r < 0) {
        printf("[FAIL] worker ThreadGetCtx -> %ld\n", r);
        return;
    }
    /* Verify per-thread errno starts at 0, is set on failure, and is preserved
     * across subsequent successful writes. */
    int init_err = errno;
    ssize_t bad = write(99, "x", 1);
    int after_fail = errno;
    printf("[OK] worker thread running (tid=%u prio=%u entry=%llx)\n", ti.tid,
           ti.prio, (unsigned long long)ti.entry);
    int after_ok = errno;
    if (init_err == 0 && bad == -1 && after_fail == EBADF &&
        after_ok == EBADF) {
        g_worker_errno_ok = 1;
    } else {
        printf("[FAIL] worker errno init=%d bad=%ld fail=%d ok=%d\n", init_err,
               (long)bad, after_fail, after_ok);
    }
    for (;;) {
        __asm__ volatile("pause");
    }
}

/* Async-signal-safe handler (XSH 2.4.3): uses only sigprocmask and write. */
static void handler(int sig) {
    static const char msg[] = "[OK] signal handler entered\n";
    sigset_t cur = 0;
    if (sigprocmask(SIG_BLOCK, (const sigset_t *)0, &cur) == 0 &&
        sigismember(&cur, sig) == 1) {
        g_handler_saw_blocked = 1;
    }
    g_caught_sig = sig;
    (void)write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main(void) {
    long tid = ThreadSpawn(worker, (void *)0, 100);
    if (tid < 0) {
        printf("[FAIL] ThreadSpawn -> %ld\n", tid);
        return 1;
    }
    printf("[OK] sigtest worker spawned (tid=%ld)\n", tid);

    thread_info_t ti = {0};
    if (ThreadGetCtx((unsigned)tid, &ti) < 0) {
        printf("[FAIL] ThreadGetCtx\n");
        return 1;
    }
    printf("[OK] thread ctx: tid=%u state=%u prio=%u stack=%llx\n", ti.tid,
           ti.state, ti.prio, (unsigned long long)ti.stack_top);

    /* POSIX: sigaction must reject catching or ignoring SIGKILL and SIGSTOP. */
    struct sigaction sa = {0};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    int r_kill = sigaction(SIGKILL, &sa, (struct sigaction *)0);
    int e_kill = errno;
    int r_stop = sigaction(SIGSTOP, &sa, (struct sigaction *)0);
    int e_stop = errno;
    if (r_kill == -1 && e_kill == EINVAL && r_stop == -1 && e_stop == EINVAL) {
        printf("[OK] sigaction rejects SIGKILL and SIGSTOP with EINVAL\n");
    } else {
        printf("[FAIL] SIGKILL/SIGSTOP r=%d/%d e=%d/%d\n", r_kill, r_stop,
               e_kill, e_stop);
        return 1;
    }

    if (sigaction(SIGUSR1, &sa, (struct sigaction *)0) < 0) {
        printf("[FAIL] sigaction(SIGUSR1)\n");
        return 1;
    }

    /* Set main's thread-local errno before yielding to worker so we can check
     * isolation across threads after worker sets EBADF. */
    errno = EINVAL;

    /* Yield to the worker deterministically: while this thread blocks in IPC,
     * the scheduler dispatches the worker (it prints its line). */
    int nconnid = (int)OpenConnection(1, 1);
    name_msg_t q;
    memset(&q, 0, sizeof(q));
    q.type = NAME_MSG_QUERY;
    memcpy(q.path, "/dev/echo", sizeof("/dev/echo"));
    name_reply_t nr = {0};
    if (SendMsg(nconnid, &q, sizeof(q), &nr, sizeof(nr)) < 0) {
        printf("[FAIL] sigtest yield IPC\n");
        return 1;
    }
    printf("[OK] sigtest main resumed after worker slice\n");
    if (errno == EINVAL && g_worker_errno_ok) {
        printf("[OK] errno thread-local and preserved across success\n");
    } else {
        printf("[FAIL] errno isolation main=%d worker_ok=%d\n", errno,
               g_worker_errno_ok);
        return 1;
    }

    /* Verify sigprocmask deferral and sig_frame_t mask save/restore. */
    sigset_t blk = 0;
    sigset_t old = 0;
    sigemptyset(&blk);
    sigaddset(&blk, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blk, &old) != 0) {
        printf("[FAIL] sigprocmask(SIG_BLOCK)\n");
        return 1;
    }
    printf("[OK] sigtest raising SIGUSR1 while blocked\n");
    if (raise(SIGUSR1) != 0) {
        printf("[FAIL] raise(SIGUSR1)\n");
        return 1;
    }
    if (g_caught_sig != 0) {
        printf("[FAIL] SIGUSR1 delivered while blocked\n");
        return 1;
    }
    if (sigprocmask(SIG_SETMASK, &old, (sigset_t *)0) != 0) {
        printf("[FAIL] sigprocmask(SIG_SETMASK)\n");
        return 1;
    }
    sigset_t after = 0;
    sigprocmask(SIG_BLOCK, (const sigset_t *)0, &after);
    if (g_caught_sig == SIGUSR1 && g_handler_saw_blocked &&
        sigismember(&after, SIGUSR1) == 0) {
        printf("[OK] signal mask saved and restored across sigreturn\n");
    } else {
        printf("[FAIL] signal mask state caught=%d in_h=%d after=%u\n",
               (int)g_caught_sig, (int)g_handler_saw_blocked, after);
        return 1;
    }

    /* Verify SIG_IGN discards a signal without terminating. */
    struct sigaction ign = {0};
    ign.sa_handler = SIG_IGN;
    if (sigaction(SIGALRM, &ign, (struct sigaction *)0) != 0 ||
        raise(SIGALRM) != 0) {
        printf("[FAIL] SIG_IGN handling\n");
        return 1;
    }

    printf("[OK] sigtest raising SIGUSR2 (unhandled)\n");
    raise(SIGUSR2);
    printf("[FAIL] sigtest should have died from SIGUSR2\n");
    return 2;
}
