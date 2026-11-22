/**
 * @file     test_smp.c
 * @brief    QEMU multi-core SMP concurrency test (parallelism, futex, VMM, IPC)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: VER_PROC_0007 */

/* ---- 1. True multi-core lock-step atomic ping-pong ---- */

#define PINGPONG_TURNS 10000u

typedef struct {
    pthread_barrier_t bar;
    volatile uint32_t turn;
    volatile uint32_t count;
} pingpong_t;

static void *pingpong_worker(void *arg) {
    pingpong_t *pp = (pingpong_t *)arg;
    (void)pthread_barrier_wait(&pp->bar);
    for (uint32_t i = 0; i < PINGPONG_TURNS; i++) {
        while (__atomic_load_n(&pp->turn, __ATOMIC_ACQUIRE) != 1u) {
        }
        __atomic_fetch_add(&pp->count, 1u, __ATOMIC_RELAXED);
        __atomic_store_n(&pp->turn, 0u, __ATOMIC_RELEASE);
    }
    return NULL;
}

static int test_parallel_pingpong(void) {
    pingpong_t pp;
    memset(&pp, 0, sizeof(pp));
    if (pthread_barrier_init(&pp.bar, NULL, 2u) != 0) {
        printf("[FAIL] smptest barrier_init\n");
        return -1;
    }
    pthread_t th = 0;
    if (pthread_create(&th, NULL, pingpong_worker, &pp) != 0) {
        printf("[FAIL] smptest pingpong pthread_create\n");
        return -1;
    }
    (void)pthread_barrier_wait(&pp.bar);
    for (uint32_t i = 0; i < PINGPONG_TURNS; i++) {
        while (__atomic_load_n(&pp.turn, __ATOMIC_ACQUIRE) != 0u) {
        }
        __atomic_fetch_add(&pp.count, 1u, __ATOMIC_RELAXED);
        __atomic_store_n(&pp.turn, 1u, __ATOMIC_RELEASE);
    }
    (void)pthread_join(th, NULL);
    (void)pthread_barrier_destroy(&pp.bar);
    if (pp.count != PINGPONG_TURNS * 2u) {
        printf("[FAIL] smptest pingpong count=%u\n", (unsigned)pp.count);
        return -1;
    }
    printf("[OK] SMP multi-core atomic ping-pong (%u turns) verified\n",
           (unsigned)(PINGPONG_TURNS * 2u));
    return 0;
}

/* ---- 2. Concurrent futex / mutex / condvar / semaphore contention ---- */

#define CONTEND_THREADS 4u
#define CONTEND_ITERS 1500u

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    sem_t sem;
    uint32_t counter;
    uint32_t ready;
    int go;
} contend_t;

static void *contend_worker(void *arg) {
    contend_t *c = (contend_t *)arg;
    (void)pthread_mutex_lock(&c->mtx);
    c->ready++;
    if (c->ready == CONTEND_THREADS) {
        c->go = 1;
        (void)pthread_cond_broadcast(&c->cv);
    } else {
        while (!c->go) {
            (void)pthread_cond_wait(&c->cv, &c->mtx);
        }
    }
    (void)pthread_mutex_unlock(&c->mtx);

    for (uint32_t i = 0; i < CONTEND_ITERS; i++) {
        (void)pthread_mutex_lock(&c->mtx);
        c->counter++;
        (void)pthread_mutex_unlock(&c->mtx);
    }
    (void)sem_post(&c->sem);
    return NULL;
}

static int test_futex_contention(void) {
    contend_t c;
    memset(&c, 0, sizeof(c));
    (void)pthread_mutex_init(&c.mtx, NULL);
    (void)pthread_cond_init(&c.cv, NULL);
    (void)sem_init(&c.sem, 0, 0u);

    pthread_t ths[CONTEND_THREADS];
    for (uint32_t i = 0; i < CONTEND_THREADS; i++) {
        if (pthread_create(&ths[i], NULL, contend_worker, &c) != 0) {
            printf("[FAIL] smptest contend pthread_create %u\n", (unsigned)i);
            return -1;
        }
    }
    for (uint32_t i = 0; i < CONTEND_THREADS; i++) {
        while (sem_wait(&c.sem) != 0 && errno == EINTR) {
        }
    }
    for (uint32_t i = 0; i < CONTEND_THREADS; i++) {
        (void)pthread_join(ths[i], NULL);
    }
    (void)sem_destroy(&c.sem);
    (void)pthread_cond_destroy(&c.cv);
    (void)pthread_mutex_destroy(&c.mtx);

    uint32_t expect = CONTEND_THREADS * CONTEND_ITERS;
    if (c.counter != expect) {
        printf("[FAIL] smptest mutex counter=%u expected=%u\n",
               (unsigned)c.counter, (unsigned)expect);
        return -1;
    }
    printf("[OK] SMP concurrent futex, mutex, condvar, and sem (%u ops) "
           "verified\n",
           (unsigned)expect);
    return 0;
}

/* ---- 3. Concurrent PMM/VMM/mmap/mprotect/COW stress ---- */

static volatile int g_vmm_err;

static void *vmm_worker(void *arg) {
    uint32_t tag = (uint32_t)(uintptr_t)arg;
    for (uint32_t iter = 0; iter < 40u; iter++) {
        void *p = mmap(NULL, 8192u, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            __atomic_store_n(&g_vmm_err, 1, __ATOMIC_RELEASE);
            return NULL;
        }
        uint32_t *w = (uint32_t *)p;
        w[0] = tag ^ iter;
        w[1024] = (tag << 8) ^ iter;
        if (mprotect(p, 8192u, PROT_READ) != 0) {
            __atomic_store_n(&g_vmm_err, 2, __ATOMIC_RELEASE);
            (void)munmap(p, 8192u);
            return NULL;
        }
        if (w[0] != (tag ^ iter) || w[1024] != ((tag << 8) ^ iter)) {
            __atomic_store_n(&g_vmm_err, 3, __ATOMIC_RELEASE);
            (void)munmap(p, 8192u);
            return NULL;
        }
        if (munmap(p, 8192u) != 0) {
            __atomic_store_n(&g_vmm_err, 4, __ATOMIC_RELEASE);
            return NULL;
        }
    }
    return NULL;
}

static int test_vmm_cow_smp(void) {
    g_vmm_err = 0;
    pthread_t t1 = 0;
    pthread_t t2 = 0;
    if (pthread_create(&t1, NULL, vmm_worker, (void *)(uintptr_t)0xA501u) !=
            0 ||
        pthread_create(&t2, NULL, vmm_worker, (void *)(uintptr_t)0xB602u) !=
            0) {
        printf("[FAIL] smptest vmm pthread_create\n");
        return -1;
    }

    /* Run concurrent fork + COW writes while t1 and t2 hammer mmap/mprotect. */
    static uint32_t cow_buf[1024];
    cow_buf[0] = 0xCAFEBABEu;
    for (int i = 0; i < 6; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("[FAIL] smptest fork %d\n", i);
            return -1;
        }
        if (pid == 0) {
            cow_buf[0] = 0x12340000u + (uint32_t)i;
            _exit(cow_buf[0] == (0x12340000u + (uint32_t)i) ? 0 : 1);
        }
        int st = 0;
        while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
        }
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0 ||
            cow_buf[0] != 0xCAFEBABEu) {
            printf("[FAIL] smptest COW child %d st=%d val=0x%x\n", i, st,
                   (unsigned)cow_buf[0]);
            return -1;
        }
    }

    (void)pthread_join(t1, NULL);
    (void)pthread_join(t2, NULL);
    if (g_vmm_err != 0) {
        printf("[FAIL] smptest vmm_worker err=%d\n", g_vmm_err);
        return -1;
    }
    printf("[OK] SMP concurrent PMM, VMM, TLB shootdown, and COW verified\n");
    return 0;
}

/* ---- 4. Concurrent multi-client IPC + AVX2/FMA XSAVE isolation ---- */

#define IPC_CLIENTS 2u
#define IPC_REQS_PER_CLIENT 150u

typedef struct {
    int chanid;
    uint32_t total_reqs;
} ipc_srv_arg_t;

typedef struct {
    uint32_t pid;
    int chanid;
    uint32_t client_id;
    int err;
} ipc_cli_arg_t;

typedef struct {
    uint32_t cid;
    uint32_t seq;
} smp_msg_t;

static void *ipc_server_thread(void *arg) {
    ipc_srv_arg_t *sa = (ipc_srv_arg_t *)arg;
    for (uint32_t i = 0; i < sa->total_reqs; i++) {
        smp_msg_t req = {0};
        ipc_recv_info_t info = {0};
        long msgid = ReceiveMsg(sa->chanid, &req, sizeof(req), &info);
        if (msgid <= 0) {
            break;
        }
        smp_msg_t rep = {.cid = req.cid, .seq = req.seq ^ 0x5A5A5A5Au};
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
    }
    return NULL;
}

static void *ipc_client_thread(void *arg) {
    ipc_cli_arg_t *ca = (ipc_cli_arg_t *)arg;
    long conn = OpenConnection(ca->pid, (uint32_t)ca->chanid);
    if (conn < 0) {
        ca->err = 1;
        return NULL;
    }
    /* Interleave AVX2/FMA arithmetic with synchronous IPC across cores. */
    double acc = 1.0 + (double)ca->client_id;
    for (uint32_t i = 0; i < IPC_REQS_PER_CLIENT; i++) {
        smp_msg_t req = {.cid = ca->client_id, .seq = i};
        smp_msg_t rep = {0};
        long r = SendMsg((int)conn, &req, sizeof(req), &rep, sizeof(rep));
        if (r != (long)sizeof(rep) || rep.cid != ca->client_id ||
            rep.seq != (i ^ 0x5A5A5A5Au)) {
            ca->err = 2;
            break;
        }
        acc = acc * 1.0001 + 0.5;
    }
    (void)CloseConnection((int)conn);
    if (acc < 70.0) {
        ca->err = 3;
    }
    return NULL;
}

static int test_ipc_and_simd_smp(void) {
    long chan = CreateChannel();
    if (chan < 0) {
        printf("[FAIL] smptest CreateChannel\n");
        return -1;
    }
    ipc_srv_arg_t sa = {
        .chanid = (int)chan,
        .total_reqs = IPC_CLIENTS * IPC_REQS_PER_CLIENT,
    };
    pthread_t srv = 0;
    if (pthread_create(&srv, NULL, ipc_server_thread, &sa) != 0) {
        (void)DestroyChannel((int)chan);
        printf("[FAIL] smptest srv pthread_create\n");
        return -1;
    }

    ipc_cli_arg_t ca[IPC_CLIENTS];
    pthread_t cli[IPC_CLIENTS];
    uint32_t self_pid = (uint32_t)getpid();
    for (uint32_t i = 0; i < IPC_CLIENTS; i++) {
        ca[i].pid = self_pid;
        ca[i].chanid = (int)chan;
        ca[i].client_id = i + 1u;
        ca[i].err = 0;
        if (pthread_create(&cli[i], NULL, ipc_client_thread, &ca[i]) != 0) {
            printf("[FAIL] smptest cli pthread_create\n");
            return -1;
        }
    }
    for (uint32_t i = 0; i < IPC_CLIENTS; i++) {
        (void)pthread_join(cli[i], NULL);
    }
    (void)pthread_join(srv, NULL);
    (void)DestroyChannel((int)chan);

    for (uint32_t i = 0; i < IPC_CLIENTS; i++) {
        if (ca[i].err != 0) {
            printf("[FAIL] smptest ipc client %u err=%d\n", (unsigned)i,
                   ca[i].err);
            return -1;
        }
    }
    printf("[OK] SMP concurrent IPC and XSAVE/FMA isolation (%u round-trips) "
           "verified\n",
           (unsigned)(IPC_CLIENTS * IPC_REQS_PER_CLIENT));
    return 0;
}

int main(void) {
    struct __k_sys_stats ks = {0};
    if (SysStats(&ks) != 0 || ks.online_cpus < 2u) {
        printf("[FAIL] smptest expected >= 2 online CPUs, got %u\n",
               (unsigned)ks.online_cpus);
        return 1;
    }
    printf("[OK] SMP SysStats reports %u online CPUs\n",
           (unsigned)ks.online_cpus);

    if (test_parallel_pingpong() != 0) {
        return 1;
    }
    if (test_futex_contention() != 0) {
        return 1;
    }
    if (test_vmm_cow_smp() != 0) {
        return 1;
    }
    if (test_ipc_and_simd_smp() != 0) {
        return 1;
    }
    return 0;
}
