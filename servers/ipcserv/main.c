/* ipcserv — demo echo server for the IPC milestone.
 * Creates a channel, registers "/dev/echo" with the sysproc name service,
 * then serves echo requests from TWO threads (main + worker) to exercise
 * concurrent receivers on one channel. */
#include <stdio.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/ipc.h>
#include <sys/thread.h>

static int g_chanid;

static void echo_loop(void) {
    echo_msg_t req;
    char rep[96];
    ipc_recv_info_t info;
    for (;;) {
        long msgid = ReceiveMsg(g_chanid, &req, sizeof(req), &info);
        if (msgid <= 0) {
            continue; /* pulse or error: nothing to reply to */
        }
        req.text[sizeof(req.text) - 1] = '\0';
        /* The kernel fills req via ReceiveMsg; the analyzer cannot see the
         * syscall wrapper body and flags the read as uninitialized. */
        int n = snprintf(rep, sizeof(rep), "ECHO:%u:%s", req.seq, req.text); // NOLINT(clang-analyzer-core.CallAndMessage)
        if (n < 0) {
            n = 0;
        }
        if (ReplyMsg((int)msgid, rep, (size_t)n + 1) < 0) {
            printf("[FAIL] ipcserv ReplyMsg msgid=%ld\n", msgid);
            continue;
        }
        printf("[ipcserv] tid=%ld served seq=%u\n", ThreadGetTid(), req.seq);
    }
}

static void worker(void *arg) {
    (void)arg;
    printf("[OK] ipcserv echo worker running\n");
    echo_loop();
}

int main(void) {
    g_chanid = (int)CreateChannel();
    if (g_chanid < 0) {
        printf("[FAIL] ipcserv CreateChannel -> %d\n", g_chanid);
        return 1;
    }

    /* Register with the sysproc name service (PID 1, chanid 1). */
    int nconnid = (int)OpenConnection(1, 1);
    if (nconnid < 0) {
        printf("[FAIL] ipcserv OpenConnection(name) -> %d\n", nconnid);
        return 1;
    }
    name_msg_t nm;
    memset(&nm, 0, sizeof(nm));
    nm.type = NAME_MSG_ATTACH;
    nm.chanid = (uint32_t)g_chanid;
    memcpy(nm.path, "/dev/echo", sizeof("/dev/echo"));
    name_reply_t nr;
    long s = SendMsg(nconnid, &nm, sizeof(nm), &nr, sizeof(nr));
    if (s < 0 || nr.err != 0) { // NOLINT(clang-analyzer-core.UndefinedBinaryOperatorResult)
        printf("[FAIL] ipcserv register -> send=%ld err=%d\n", s, nr.err); // NOLINT(clang-analyzer-core.CallAndMessage)
        return 1;
    }
    printf("[OK] ipcserv registered /dev/echo (chanid=%d)\n", g_chanid);

    /* Second receiver thread on the same channel. */
    long tid = ThreadSpawn(worker, (void *)0, 100);
    if (tid < 0) {
        printf("[FAIL] ipcserv ThreadSpawn -> %ld\n", tid);
        return 1;
    }
    printf("[OK] ipcserv worker spawned (tid=%ld)\n", tid);

    echo_loop(); /* never returns */
    return 0;
}
