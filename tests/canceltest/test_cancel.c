/**
 * @file     test_cancel.c
 * @brief    QEMU test for POSIX.1-2024 thread cancellation points & stdio locks
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_LIBC_0004 */

static pthread_mutex_t g_cond_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static volatile int g_cond_ready;
static volatile int g_cond_cleanup_ran;

static void cond_cleanup(void *arg) {
    pthread_mutex_t *m = (pthread_mutex_t *)arg;
    g_cond_cleanup_ran = 1;
    (void)pthread_mutex_unlock(m);
}

static void *cond_cancel_worker(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_cond_mtx);
    pthread_cleanup_push(cond_cleanup, &g_cond_mtx);
    __atomic_store_n(&g_cond_ready, 1, __ATOMIC_RELEASE);
    for (;;) {
        pthread_cond_wait(&g_cond, &g_cond_mtx);
    }
    pthread_cleanup_pop(1);
    return NULL;
}

static int test_cond_cancel_and_cleanup(void) {
    pthread_t th = 0;
    if (pthread_create(&th, NULL, cond_cancel_worker, NULL) != 0) {
        printf("[FAIL] canceltest pthread_create(cond) failed\n");
        return 1;
    }
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L};
    while (!__atomic_load_n(&g_cond_ready, __ATOMIC_ACQUIRE)) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
    }
    /* Wait until worker releases g_cond_mtx inside pthread_cond_wait. */
    pthread_mutex_lock(&g_cond_mtx);
    pthread_mutex_unlock(&g_cond_mtx);

    if (pthread_cancel(th) != 0) {
        printf("[FAIL] pthread_cancel(cond) failed\n");
        return 1;
    }
    void *res = NULL;
    if (pthread_join(th, &res) != 0 || res != PTHREAD_CANCELED ||
        !g_cond_cleanup_ran) {
        printf("[FAIL] cond cancel res=%p cleanup=%d\n", res,
               g_cond_cleanup_ran);
        return 1;
    }
    if (pthread_mutex_trylock(&g_cond_mtx) != 0) {
        printf("[FAIL] cond cancel left mutex locked\n");
        return 1;
    }
    pthread_mutex_unlock(&g_cond_mtx);
    printf("[OK] pthread_cancel woke cond_wait and ran cleanup with mutex "
           "re-acquired\n");
    return 0;
}

static volatile int g_dis_ready;
static volatile int g_dis_proceed;
static volatile int g_dis_survived;
static char g_lifo_log[4];
static int g_lifo_idx;

static void lifo_cb(void *arg) {
    char ch = *(const char *)arg;
    if (g_lifo_idx < 3) {
        g_lifo_log[g_lifo_idx++] = ch;
    }
}

static void *disable_lifo_worker(void *arg) {
    (void)arg;
    int old = -1;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old) != 0 ||
        old != PTHREAD_CANCEL_ENABLE) {
        return (void *)1;
    }
    __atomic_store_n(&g_dis_ready, 1, __ATOMIC_RELEASE);
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L};
    while (!__atomic_load_n(&g_dis_proceed, __ATOMIC_ACQUIRE)) {
        nanosleep(&step, NULL);
    }
    /* Cancellation is pending but disabled: nanosleep must NOT cancel. */
    nanosleep(&step, NULL);
    __atomic_store_n(&g_dis_survived, 1, __ATOMIC_RELEASE);

    static const char kA = 'A';
    static const char kB = 'B';
    pthread_cleanup_push(lifo_cb, (void *)&kA);
    pthread_cleanup_push(lifo_cb, (void *)&kB);
    (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old);
    pthread_testcancel();
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    return (void *)2;
}

static int test_disable_and_lifo_cleanup(void) {
    pthread_t th = 0;
    if (pthread_create(&th, NULL, disable_lifo_worker, NULL) != 0) {
        printf("[FAIL] canceltest pthread_create(disable) failed\n");
        return 1;
    }
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L};
    while (!__atomic_load_n(&g_dis_ready, __ATOMIC_ACQUIRE)) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
    }
    if (pthread_cancel(th) != 0) {
        printf("[FAIL] pthread_cancel(disable) failed\n");
        return 1;
    }
    __atomic_store_n(&g_dis_proceed, 1, __ATOMIC_RELEASE);

    void *res = NULL;
    if (pthread_join(th, &res) != 0 || res != PTHREAD_CANCELED ||
        !g_dis_survived || strcmp(g_lifo_log, "BA") != 0) {
        printf("[FAIL] disable/LIFO res=%p survived=%d log='%s'\n", res,
               g_dis_survived, g_lifo_log);
        return 1;
    }
    printf("[OK] PTHREAD_CANCEL_DISABLE, testcancel, and LIFO cleanup "
           "verified\n");
    return 0;
}

typedef struct {
    int kind; /* 0=read, 1=ppoll, 2=sem_wait, 3=clock_nanosleep */
    int fd;
    sem_t *sem;
    volatile int started;
} point_ctx_t;

static void *cancel_point_worker(void *arg) {
    point_ctx_t *ctx = (point_ctx_t *)arg;
    __atomic_store_n(&ctx->started, 1, __ATOMIC_RELEASE);
    if (ctx->kind == 0) {
        char buf[8];
        (void)read(ctx->fd, buf, sizeof(buf));
    } else if (ctx->kind == 1) {
        struct pollfd pfd = {.fd = ctx->fd, .events = POLLIN, .revents = 0};
        struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
        (void)ppoll(&pfd, 1, &ts, NULL);
    } else if (ctx->kind == 2) {
        (void)sem_wait(ctx->sem);
    } else {
        struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
        (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
    return (void *)99;
}

static int test_blocking_cancel_points(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] canceltest pipe2 failed\n");
        return 1;
    }
    sem_t sem;
    if (sem_init(&sem, 0, 0u) != 0) {
        printf("[FAIL] canceltest sem_init failed\n");
        return 1;
    }

    struct timespec wait_block = {.tv_sec = 0, .tv_nsec = 15000000L};
    for (int kind = 0; kind < 4; kind++) {
        point_ctx_t ctx = {
            .kind = kind, .fd = pfd[0], .sem = &sem, .started = 0};
        pthread_t th = 0;
        if (pthread_create(&th, NULL, cancel_point_worker, &ctx) != 0) {
            printf("[FAIL] cancel_point_worker(%d) create failed\n", kind);
            return 1;
        }
        while (!__atomic_load_n(&ctx.started, __ATOMIC_ACQUIRE)) {
            struct timespec s = {.tv_sec = 0, .tv_nsec = 2000000L};
            clock_nanosleep(CLOCK_MONOTONIC, 0, &s, NULL);
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &wait_block, NULL);
        if (pthread_cancel(th) != 0) {
            printf("[FAIL] pthread_cancel(kind=%d) failed\n", kind);
            return 1;
        }
        void *res = NULL;
        if (pthread_join(th, &res) != 0 || res != PTHREAD_CANCELED) {
            printf("[FAIL] cancel point kind=%d res=%p\n", kind, res);
            return 1;
        }
    }

    if (sem_destroy(&sem) != 0) {
        printf("[FAIL] sem_destroy after canceled sem_wait failed\n");
        return 1;
    }
    close(pfd[0]);
    close(pfd[1]);
    printf("[OK] cancellation points read, ppoll, sem_wait, and "
           "clock_nanosleep verified\n");
    return 0;
}

static volatile int g_stdio_trylock_failed;
static volatile int g_stdio_worker_done;

static void *stdio_lock_worker(void *arg) {
    (void)arg;
    /* Main thread currently holds recursive flockfile(stdout). */
    if (ftrylockfile(stdout) != 0) {
        __atomic_store_n(&g_stdio_trylock_failed, 1, __ATOMIC_RELEASE);
    }
    flockfile(stdout);
    fprintf(stdout, "[OK] flockfile/ftrylockfile/funlockfile serialized "
                    "concurrent stdio\n");
    funlockfile(stdout);
    __atomic_store_n(&g_stdio_worker_done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static int test_stdio_locking(void) {
    /* Recursive flockfile on same thread must not deadlock. */
    flockfile(stdout);
    flockfile(stdout);
    if (ftrylockfile(stdout) != 0) {
        funlockfile(stdout);
        funlockfile(stdout);
        printf("[FAIL] recursive ftrylockfile(stdout) failed\n");
        return 1;
    }

    pthread_t th = 0;
    if (pthread_create(&th, NULL, stdio_lock_worker, NULL) != 0) {
        funlockfile(stdout);
        funlockfile(stdout);
        funlockfile(stdout);
        printf("[FAIL] stdio_lock_worker create failed\n");
        return 1;
    }

    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L};
    for (int i = 0;
         i < 40 && !__atomic_load_n(&g_stdio_trylock_failed, __ATOMIC_ACQUIRE);
         i++) {
        clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
    }
    int blocked_while_locked =
        (__atomic_load_n(&g_stdio_worker_done, __ATOMIC_ACQUIRE) == 0);

    /* Release all 3 recursive holds so worker acquires stdout and prints. */
    funlockfile(stdout);
    funlockfile(stdout);
    funlockfile(stdout);

    if (pthread_join(th, NULL) != 0 || !g_stdio_trylock_failed ||
        !blocked_while_locked || !g_stdio_worker_done) {
        printf("[FAIL] stdio locking check failed (%d, %d, %d)\n",
               g_stdio_trylock_failed, blocked_while_locked,
               g_stdio_worker_done);
        return 1;
    }
    return 0;
}

int main(void) {
    if (test_cond_cancel_and_cleanup() != 0) {
        return 1;
    }
    if (test_disable_and_lifo_cleanup() != 0) {
        return 1;
    }
    if (test_blocking_cancel_points() != 0) {
        return 1;
    }
    if (test_stdio_locking() != 0) {
        return 1;
    }
    return 0;
}
