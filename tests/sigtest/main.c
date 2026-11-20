// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

/* sigtest — Milestone 4 verification: threads and signals.
 * 1. spawns a worker thread (THREAD_CREATE + THREAD_RESET),
 * 2. reads its context back (THREAD_GET_CTX),
 * 3. catches SIGUSR1 (handler runs, SIGRETURN restores execution),
 * 4. dies from an unhandled SIGUSR2 (default action -> zombie). */
#include <stdio.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/ipc.h>
#include <sys/thread.h>
#include <sys/signal.h>

static void worker(void *arg) {
    (void)arg;
    thread_info_t ti;
    long r = ThreadGetCtx(0, &ti);
    if (r < 0) {
        printf("[FAIL] worker ThreadGetCtx -> %ld\n", r);
        return;
    }
    /* The kernel fills ti through the syscall; the analyzer cannot see the
     * wrapper body and flags the read as possibly uninitialized. */
    printf("[OK] worker thread running (tid=%u prio=%u entry=%llx)\n", ti.tid, // NOLINT(clang-analyzer-core.CallAndMessage)
           ti.prio, (unsigned long long)ti.entry);
    for (;;) {
        __asm__ volatile("pause");
    }
}

static void handler(int sig) {
    printf("[OK] signal handler entered (sig=%d)\n", sig);
}

int main(void) {
    long tid = ThreadSpawn(worker, (void *)0, 100);
    if (tid < 0) {
        printf("[FAIL] ThreadSpawn -> %ld\n", tid);
        return 1;
    }
    printf("[OK] sigtest worker spawned (tid=%ld)\n", tid);

    thread_info_t ti;
    if (ThreadGetCtx((unsigned)tid, &ti) < 0) {
        printf("[FAIL] ThreadGetCtx\n");
        return 1;
    }
    printf("[OK] thread ctx: tid=%u state=%u prio=%u stack=%llx\n", ti.tid,
           ti.state, ti.prio, (unsigned long long)ti.stack_top);

    if (sigaction(SIGUSR1, handler) < 0) {
        printf("[FAIL] sigaction\n");
        return 1;
    }

    /* Yield to the worker deterministically: while this thread blocks in
     * IPC, the scheduler dispatches the worker (it prints its line). */
    int nconnid = (int)OpenConnection(1, 1);
    name_msg_t q;
    memset(&q, 0, sizeof(q));
    q.type = NAME_MSG_QUERY;
    memcpy(q.path, "/dev/echo", sizeof("/dev/echo"));
    name_reply_t nr;
    if (SendMsg(nconnid, &q, sizeof(q), &nr, sizeof(nr)) < 0) {
        printf("[FAIL] sigtest yield IPC\n");
        return 1;
    }
    printf("[OK] sigtest main resumed after worker slice\n");

    printf("[OK] sigtest raising SIGUSR1\n");
    long k = kill(0, 0, SIGUSR1); /* self, current thread */
    if (k < 0) {
        printf("[FAIL] kill -> %ld\n", k);
        return 1;
    }
    /* If SIGRETURN restored us correctly, the INJECT_SIG result is intact. */
    printf("[OK] sigtest survived SIGUSR1 (kill returned %ld)\n", k);

    printf("[OK] sigtest raising SIGUSR2 (unhandled)\n");
    kill(0, 0, SIGUSR2);
    printf("[FAIL] sigtest should have died from SIGUSR2\n");
    return 2;
}
