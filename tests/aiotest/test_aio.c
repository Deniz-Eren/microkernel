/**
 * @file     test_aio.c
 * @brief    QEMU integration test for POSIX.1-2024 Base <aio.h> operations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_IPC_0004 */

static volatile sig_atomic_t g_aio_sig_hits;

static void aio_sig_handler(int sig) {
    if (sig == SIGUSR1) {
        g_aio_sig_hits++;
    }
}

static int test_aio_read_write_and_sigev(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] aiotest pipe2 failed (errno=%d)\n", errno);
        return 1;
    }

    struct sigaction sa = {0};
    sa.sa_handler = aio_sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("[FAIL] aiotest sigaction failed\n");
        return 1;
    }

    const char *msg = "posix2024-aio-ok";
    size_t mlen = strlen(msg);
    char rbuf[32];
    memset(rbuf, 0, sizeof(rbuf));

    struct aiocb wcb = {0};
    wcb.aio_fildes = pfd[1];
    wcb.aio_buf = (volatile void *)(uintptr_t)msg;
    wcb.aio_nbytes = mlen;
    wcb.aio_sigevent.sigev_notify = SIGEV_NONE;

    struct aiocb rcb = {0};
    rcb.aio_fildes = pfd[0];
    rcb.aio_buf = rbuf;
    rcb.aio_nbytes = mlen;
    rcb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    rcb.aio_sigevent.sigev_signo = SIGUSR1;

    if (aio_write(&wcb) != 0 || aio_read(&rcb) != 0) {
        printf("[FAIL] aio_write/aio_read submit failed (errno=%d)\n", errno);
        return 1;
    }

    const struct aiocb *wlist[1] = {&wcb};
    const struct aiocb *rlist[1] = {&rcb};
    if (aio_suspend(wlist, 1, NULL) != 0 || aio_suspend(rlist, 1, NULL) != 0) {
        printf("[FAIL] aio_suspend failed (errno=%d)\n", errno);
        return 1;
    }

    if (aio_error(&wcb) != 0 || aio_return(&wcb) != (ssize_t)mlen) {
        printf("[FAIL] aio_write completion mismatch\n");
        return 1;
    }
    if (aio_error(&rcb) != 0 || aio_return(&rcb) != (ssize_t)mlen ||
        strcmp(rbuf, msg) != 0) {
        printf("[FAIL] aio_read completion mismatch: '%s'\n", rbuf);
        return 1;
    }

    /* Wait briefly if SIGUSR1 delivery is still pending. */
    struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
    for (int i = 0; i < 10 && g_aio_sig_hits == 0; i++) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
    }
    if (g_aio_sig_hits < 1) {
        printf("[FAIL] aio_read SIGEV_SIGNAL did not fire SIGUSR1\n");
        return 1;
    }

    close(pfd[0]);
    close(pfd[1]);
    printf(
        "[OK] aio_read, aio_write, aio_suspend, and SIGEV_SIGNAL verified\n");
    return 0;
}

static int test_aio_cancel_suspend_timeout_and_fsync(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] aiotest pipe2(2) failed\n");
        return 1;
    }

    char b1[8] = {0};
    char b2[8] = {0};
    struct aiocb cb1 = {0};
    cb1.aio_fildes = pfd[0];
    cb1.aio_buf = b1;
    cb1.aio_nbytes = 4;
    cb1.aio_sigevent.sigev_notify = SIGEV_NONE;

    struct aiocb cb2 = {0};
    cb2.aio_fildes = pfd[0];
    cb2.aio_buf = b2;
    cb2.aio_nbytes = 4;
    cb2.aio_sigevent.sigev_notify = SIGEV_NONE;

    /* Submit cb1 (blocks worker on empty pipe), yield so worker claims cb1. */
    if (aio_read(&cb1) != 0) {
        printf("[FAIL] aio_read(cb1) failed\n");
        return 1;
    }
    struct timespec d = {.tv_sec = 0, .tv_nsec = 15000000L};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);

    /* Queue cb2 behind busy worker, then cancel cb2 (queued) and cb1 (busy). */
    if (aio_read(&cb2) != 0) {
        printf("[FAIL] aio_read(cb2) failed\n");
        return 1;
    }
    int c2 = aio_cancel(pfd[0], &cb2);
    int c1 = aio_cancel(pfd[0], &cb1);
    if (c2 != AIO_CANCELED || aio_error(&cb2) != ECANCELED) {
        printf("[FAIL] aio_cancel(queued) c2=%d err=%d\n", c2, aio_error(&cb2));
        return 1;
    }
    errno = 0;
    if (aio_return(&cb2) != -1 || errno != ECANCELED) {
        printf("[FAIL] aio_return(canceled) errno=%d\n", errno);
        return 1;
    }
    if (c1 != AIO_NOTCANCELED) {
        printf("[FAIL] aio_cancel(running) expected AIO_NOTCANCELED, got %d\n",
               c1);
        return 1;
    }

    /* Finite aio_suspend timeout while cb1 is still blocked -> EAGAIN. */
    const struct aiocb *slist[1] = {&cb1};
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 20000000L};
    errno = 0;
    if (aio_suspend(slist, 1, &ts) != -1 || errno != EAGAIN) {
        printf("[FAIL] aio_suspend timeout expected EAGAIN, got errno=%d\n",
               errno);
        return 1;
    }

    /* Unblock cb1 by writing 4 bytes, verify completion + AIO_ALLDONE. */
    if (write(pfd[1], "DONE", 4) != 4) {
        printf("[FAIL] pipe write to unblock cb1 failed\n");
        return 1;
    }
    if (aio_suspend(slist, 1, NULL) != 0 || aio_error(&cb1) != 0 ||
        aio_cancel(pfd[0], &cb1) != AIO_ALLDONE || aio_return(&cb1) != 4 ||
        memcmp(b1, "DONE", 4) != 0) {
        printf("[FAIL] cb1 completion / AIO_ALLDONE check failed\n");
        return 1;
    }

    /* Verify aio_fsync(O_SYNC / O_DSYNC) and invalid op -> EINVAL. */
    struct aiocb fcb = {0};
    fcb.aio_fildes = pfd[1];
    fcb.aio_sigevent.sigev_notify = SIGEV_NONE;
    errno = 0;
    if (aio_fsync(0, &fcb) != -1 || errno != EINVAL) {
        printf("[FAIL] aio_fsync(0) did not yield EINVAL\n");
        return 1;
    }
    if (aio_fsync(O_DSYNC, &fcb) != 0) {
        printf("[FAIL] aio_fsync(O_DSYNC) failed\n");
        return 1;
    }
    const struct aiocb *flist[1] = {&fcb};
    if (aio_suspend(flist, 1, NULL) != 0 || aio_error(&fcb) != 0 ||
        aio_return(&fcb) != 0) {
        printf("[FAIL] aio_fsync completion check failed\n");
        return 1;
    }

    close(pfd[0]);
    close(pfd[1]);
    printf("[OK] aio_cancel, aio_suspend EAGAIN timeout, and aio_fsync "
           "verified\n");
    return 0;
}

static int test_lio_listio(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] aiotest pipe2(3) failed\n");
        return 1;
    }

    const char *payload = "lio-batch-123";
    size_t plen = strlen(payload);
    char out[32];
    memset(out, 0, sizeof(out));

    struct aiocb nop_cb = {0};
    nop_cb.aio_lio_opcode = LIO_NOP;

    struct aiocb wcb = {0};
    wcb.aio_fildes = pfd[1];
    wcb.aio_buf = (volatile void *)(uintptr_t)payload;
    wcb.aio_nbytes = plen;
    wcb.aio_lio_opcode = LIO_WRITE;
    wcb.aio_sigevent.sigev_notify = SIGEV_NONE;

    struct aiocb rcb = {0};
    rcb.aio_fildes = pfd[0];
    rcb.aio_buf = out;
    rcb.aio_nbytes = plen;
    rcb.aio_lio_opcode = LIO_READ;
    rcb.aio_sigevent.sigev_notify = SIGEV_NONE;

    struct aiocb *list[3] = {&nop_cb, &wcb, &rcb};
    if (lio_listio(LIO_WAIT, list, 3, NULL) != 0) {
        printf("[FAIL] lio_listio(LIO_WAIT) failed (errno=%d)\n", errno);
        return 1;
    }
    if (aio_return(&wcb) != (ssize_t)plen ||
        aio_return(&rcb) != (ssize_t)plen || strcmp(out, payload) != 0) {
        printf("[FAIL] lio_listio data mismatch: '%s'\n", out);
        return 1;
    }

    /* Test LIO_NOWAIT with batch SIGEV_SIGNAL completion. */
    sig_atomic_t before = g_aio_sig_hits;
    memset(out, 0, sizeof(out));
    struct sigevent sev = {0};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    if (lio_listio(LIO_NOWAIT, list, 3, &sev) != 0) {
        printf("[FAIL] lio_listio(LIO_NOWAIT) failed\n");
        return 1;
    }
    const struct aiocb *rwait[1] = {&rcb};
    if (aio_suspend(rwait, 1, NULL) != 0 || aio_return(&wcb) != (ssize_t)plen ||
        aio_return(&rcb) != (ssize_t)plen || strcmp(out, payload) != 0) {
        printf("[FAIL] lio_listio(LIO_NOWAIT) result mismatch\n");
        return 1;
    }
    struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
    for (int i = 0; i < 10 && g_aio_sig_hits == before; i++) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
    }
    if (g_aio_sig_hits <= before) {
        printf("[FAIL] lio_listio(LIO_NOWAIT) batch SIGUSR1 did not fire\n");
        return 1;
    }

    close(pfd[0]);
    close(pfd[1]);
    printf("[OK] lio_listio LIO_WAIT and LIO_NOWAIT batch verified\n");
    return 0;
}

int main(void) {
    if (test_aio_read_write_and_sigev() != 0) {
        return 1;
    }
    if (test_aio_cancel_suspend_timeout_and_fsync() != 0) {
        return 1;
    }
    if (test_lio_listio() != 0) {
        return 1;
    }
    return 0;
}
