/**
 * @file     main.c
 * @brief    Ring 3 IPC name-lookup, round-trip, and pulse test client
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* ipccli — demo IPC client for the IPC milestone. Resolves "/dev/echo" through
 * the sysproc name service, attaches a connection to the server, and runs three
 * synchronous round trips. */
#include <stdio.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/ipc.h>

int main(void) {
    /* Resolve /dev/echo via sysproc (PID 1, chanid 1). */
    int nconnid = (int)OpenConnection(1, 1);
    if (nconnid < 0) {
        printf("[FAIL] ipccli OpenConnection(name) -> %d\n", nconnid);
        return 1;
    }
    name_msg_t q;
    memset(&q, 0, sizeof(q));
    q.type = NAME_MSG_QUERY;
    memcpy(q.path, "/dev/echo", sizeof("/dev/echo"));
    name_reply_t r = {0};
    long s = SendMsg(nconnid, &q, sizeof(q), &r, sizeof(r));
    if (s < 0 || r.err != 0 || r.pid == 0) {
        printf("[FAIL] ipccli name query -> send=%ld err=%d pid=%d\n", s, r.err,
               r.pid);
        return 1;
    }
    printf("[OK] name resolved /dev/echo -> pid=%d chanid=%d\n", r.pid,
           r.chanid);

    int connid = (int)OpenConnection((uint32_t)r.pid, (uint32_t)r.chanid);
    if (connid < 0) {
        printf("[FAIL] ipccli OpenConnection(echo) -> %d\n", connid);
        return 1;
    }

    for (unsigned i = 1; i <= 3; i++) {
        echo_msg_t req;
        memset(&req, 0, sizeof(req));
        req.seq = i;
        snprintf(req.text, sizeof(req.text), "ping%u", i);
        char rep[96];
        long n = SendMsg(connid, &req, sizeof(req), rep, sizeof(rep));
        if (n < 0) {
            printf("[FAIL] ipccli send %u -> %ld\n", i, n);
            return 1;
        }
        rep[sizeof(rep) - 1] = '\0';
        printf("[OK] echo: %s\n", rep);
    }

    /* Exercise the async pulse path against the name service. */
    long pr = SendPulse(nconnid, 0x10, 0x20);
    if (pr < 0) {
        printf("[FAIL] ipccli pulse -> %ld\n", pr);
        return 1;
    }

    printf("[OK] ipc client done\n");
    return 0;
}
