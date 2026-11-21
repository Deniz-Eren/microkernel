/**
 * @file     aio.c
 * @brief    POSIX.1-2024 Base Ring 3 asynchronous I/O worker engine
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>
#include "tls_internal.h"

/* need:: COMP_IPC_0007 */

#define AIO_ST_IDLE 0u
#define AIO_ST_QUEUED 1u
#define AIO_ST_RUNNING 2u
#define AIO_ST_DONE 3u

#define AIO_OP_READ 1u
#define AIO_OP_WRITE 2u
#define AIO_OP_FSYNC 3u

#define AIO_QUEUE_MAX 64
#define LIO_BATCH_MAX 16

typedef struct {
    unsigned int used;
    unsigned int remaining;
    struct sigevent sig;
} lio_batch_t;

typedef struct {
    unsigned int used;
    unsigned int op;
    struct aiocb *cb;
    lio_batch_t *batch;
} aio_slot_t;

static aio_slot_t g_slots[AIO_QUEUE_MAX];
static lio_batch_t g_batches[LIO_BATCH_MAX];
static unsigned int g_queue_lock;
static unsigned int g_work_seq;
static unsigned int g_done_seq;
static pid_t g_worker_pid;

static void q_lock(void) {
    while (__atomic_exchange_n(&g_queue_lock, 1u, __ATOMIC_ACQUIRE) != 0u) {
        __asm__ volatile("pause");
    }
}

static void q_unlock(void) {
    __atomic_store_n(&g_queue_lock, 0u, __ATOMIC_RELEASE);
}

static void notify_sigevent(const struct sigevent *sev) {
    if (!sev) {
        return;
    }
    if (sev->sigev_notify == SIGEV_SIGNAL && sev->sigev_signo > 0 &&
        sev->sigev_signo < NSIG) {
        (void)kill(getpid(), sev->sigev_signo);
    }
}

static void finish_batch_item(lio_batch_t *batch) {
    if (!batch) {
        return;
    }
    if (__atomic_sub_fetch(&batch->remaining, 1u, __ATOMIC_ACQ_REL) == 0u) {
        struct sigevent sev = batch->sig;
        __atomic_store_n(&batch->used, 0u, __ATOMIC_RELEASE);
        notify_sigevent(&sev);
    }
}

static void wake_done_waiters(void) {
    __atomic_fetch_add(&g_done_seq, 1u, __ATOMIC_ACQ_REL);
    (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_done_seq, FUTEX_WAKE, INT_MAX,
                 0, 0);
}

static void aio_worker(void *arg) {
    (void)arg;
    for (;;) {
        unsigned int seq = __atomic_load_n(&g_work_seq, __ATOMIC_ACQUIRE);
        int found = -1;
        unsigned int op = 0;
        struct aiocb *cb = NULL;
        lio_batch_t *batch = NULL;

        q_lock();
        for (int i = 0; i < AIO_QUEUE_MAX; i++) {
            if (!g_slots[i].used) {
                continue;
            }
            struct aiocb *cand = g_slots[i].cb;
            unsigned int expected = AIO_ST_QUEUED;
            if (__atomic_compare_exchange_n(&cand->__aio_state, &expected,
                                            AIO_ST_RUNNING, 0, __ATOMIC_ACQ_REL,
                                            __ATOMIC_ACQUIRE)) {
                found = i;
                op = g_slots[i].op;
                cb = cand;
                batch = g_slots[i].batch;
                g_slots[i].used = 0;
                g_slots[i].cb = NULL;
                g_slots[i].batch = NULL;
                break;
            }
            /* Request was already canceled while queued: drop slot. */
            g_slots[i].used = 0;
            g_slots[i].cb = NULL;
            g_slots[i].batch = NULL;
        }
        q_unlock();

        if (found < 0) {
            (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_work_seq, FUTEX_WAIT,
                         (long)seq, 0, 0);
            continue;
        }

        ssize_t res = 0;
        int err = 0;
        if (op == AIO_OP_READ) {
            res = read(cb->aio_fildes, (void *)(uintptr_t)cb->aio_buf,
                       cb->aio_nbytes);
            if (res < 0) {
                err = errno;
            }
        } else if (op == AIO_OP_WRITE) {
            res = write(cb->aio_fildes, (const void *)(uintptr_t)cb->aio_buf,
                        cb->aio_nbytes);
            if (res < 0) {
                err = errno;
            }
        } else if (op == AIO_OP_FSYNC) {
            if (fcntl(cb->aio_fildes, F_GETFD) < 0) {
                res = -1;
                err = EBADF;
            } else {
                res = 0;
            }
        }

        cb->__aio_ret = res;
        __atomic_store_n(&cb->__aio_err, err, __ATOMIC_RELEASE);
        __atomic_store_n(&cb->__aio_state, AIO_ST_DONE, __ATOMIC_RELEASE);
        wake_done_waiters();
        notify_sigevent(&cb->aio_sigevent);
        finish_batch_item(batch);
    }
}

static int ensure_worker(void) {
    pid_t pid = getpid();
    if (__atomic_load_n(&g_worker_pid, __ATOMIC_ACQUIRE) == pid) {
        return 0;
    }
    q_lock();
    if (g_worker_pid != pid) {
        /* Reset stale state inherited across fork(). */
        memset(g_slots, 0, sizeof(g_slots));
        memset(g_batches, 0, sizeof(g_batches));
        long tid = ThreadSpawn(aio_worker, NULL, 100);
        if (tid < 0) {
            q_unlock();
            return (int)tid;
        }
        __atomic_store_n(&g_worker_pid, pid, __ATOMIC_RELEASE);
    }
    q_unlock();
    return 0;
}

static int enqueue_aio(struct aiocb *cb, unsigned int op, lio_batch_t *batch) {
    if (!cb) {
        errno = EINVAL;
        return -1;
    }
    if (cb->aio_reqprio < 0) {
        errno = EINVAL;
        return -1;
    }
    if (fcntl(cb->aio_fildes, F_GETFD) < 0) {
        errno = EBADF;
        return -1;
    }
    if ((op == AIO_OP_READ || op == AIO_OP_WRITE) && cb->aio_nbytes > 0u &&
        !cb->aio_buf) {
        errno = EFAULT;
        return -1;
    }

    int wr = ensure_worker();
    if (wr < 0) {
        errno = -wr;
        return -1;
    }

    q_lock();
    int slot = -1;
    for (int i = 0; i < AIO_QUEUE_MAX; i++) {
        if (!g_slots[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        q_unlock();
        errno = EAGAIN;
        return -1;
    }
    cb->__aio_ret = -1;
    __atomic_store_n(&cb->__aio_err, EINPROGRESS, __ATOMIC_RELEASE);
    __atomic_store_n(&cb->__aio_state, AIO_ST_QUEUED, __ATOMIC_RELEASE);

    g_slots[slot].used = 1;
    g_slots[slot].op = op;
    g_slots[slot].cb = cb;
    g_slots[slot].batch = batch;
    q_unlock();

    __atomic_fetch_add(&g_work_seq, 1u, __ATOMIC_ACQ_REL);
    (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_work_seq, FUTEX_WAKE, 1, 0, 0);
    return 0;
}

int aio_read(struct aiocb *aiocbp) {
    return enqueue_aio(aiocbp, AIO_OP_READ, NULL);
}

int aio_write(struct aiocb *aiocbp) {
    return enqueue_aio(aiocbp, AIO_OP_WRITE, NULL);
}

int aio_fsync(int op, struct aiocb *aiocbp) {
    if (op != O_SYNC && op != O_DSYNC) {
        errno = EINVAL;
        return -1;
    }
    return enqueue_aio(aiocbp, AIO_OP_FSYNC, NULL);
}

int aio_error(const struct aiocb *aiocbp) {
    if (!aiocbp) {
        return EINVAL;
    }
    unsigned int st = __atomic_load_n(&aiocbp->__aio_state, __ATOMIC_ACQUIRE);
    if (st == AIO_ST_QUEUED || st == AIO_ST_RUNNING) {
        return EINPROGRESS;
    }
    if (st == AIO_ST_DONE) {
        return __atomic_load_n(&aiocbp->__aio_err, __ATOMIC_ACQUIRE);
    }
    return EINVAL;
}

ssize_t aio_return(struct aiocb *aiocbp) {
    if (!aiocbp) {
        errno = EINVAL;
        return -1;
    }
    unsigned int st = __atomic_load_n(&aiocbp->__aio_state, __ATOMIC_ACQUIRE);
    if (st != AIO_ST_DONE) {
        errno = EINVAL;
        return -1;
    }
    int err = __atomic_load_n(&aiocbp->__aio_err, __ATOMIC_ACQUIRE);
    ssize_t ret = aiocbp->__aio_ret;
    __atomic_store_n(&aiocbp->__aio_state, AIO_ST_IDLE, __ATOMIC_RELEASE);
    if (err != 0) {
        errno = err;
        return -1;
    }
    return ret;
}

static int cancel_one(struct aiocb *cb, lio_batch_t *batch) {
    unsigned int expected = AIO_ST_QUEUED;
    if (__atomic_compare_exchange_n(&cb->__aio_state, &expected, AIO_ST_DONE, 0,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        cb->__aio_ret = -1;
        __atomic_store_n(&cb->__aio_err, ECANCELED, __ATOMIC_RELEASE);
        wake_done_waiters();
        notify_sigevent(&cb->aio_sigevent);
        finish_batch_item(batch);
        return AIO_CANCELED;
    }
    if (expected == AIO_ST_RUNNING) {
        return AIO_NOTCANCELED;
    }
    return AIO_ALLDONE;
}

int aio_cancel(int fildes, struct aiocb *aiocbp) {
    if (fcntl(fildes, F_GETFD) < 0) {
        errno = EBADF;
        return -1;
    }
    if (aiocbp) {
        if (aiocbp->aio_fildes != fildes) {
            errno = EINVAL;
            return -1;
        }
        lio_batch_t *batch = NULL;
        q_lock();
        for (int i = 0; i < AIO_QUEUE_MAX; i++) {
            if (g_slots[i].used && g_slots[i].cb == aiocbp) {
                batch = g_slots[i].batch;
                break;
            }
        }
        q_unlock();
        return cancel_one(aiocbp, batch);
    }

    int canceled = 0;
    int not_canceled = 0;
    struct aiocb *cands[AIO_QUEUE_MAX];
    lio_batch_t *batches[AIO_QUEUE_MAX];
    int n = 0;

    q_lock();
    for (int i = 0; i < AIO_QUEUE_MAX; i++) {
        if (g_slots[i].used && g_slots[i].cb &&
            g_slots[i].cb->aio_fildes == fildes) {
            cands[n] = g_slots[i].cb;
            batches[n] = g_slots[i].batch;
            n++;
        }
    }
    q_unlock();

    for (int i = 0; i < n; i++) {
        int rc = cancel_one(cands[i], batches[i]);
        if (rc == AIO_CANCELED) {
            canceled++;
        } else if (rc == AIO_NOTCANCELED) {
            not_canceled++;
        }
    }
    if (not_canceled > 0) {
        return AIO_NOTCANCELED;
    }
    if (canceled > 0) {
        return AIO_CANCELED;
    }
    return AIO_ALLDONE;
}

int aio_suspend(const struct aiocb *const list[], int nent,
                const struct timespec *timeout) {
    __libc_cancel_point();
    if (nent < 0 || (nent > 0 && !list)) {
        errno = EINVAL;
        return -1;
    }
    struct timespec abs_dl = {0};
    const struct timespec *dl_ptr = NULL;
    int zero_timeout = 0;

    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
            timeout->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
            zero_timeout = 1;
        } else {
            struct timespec now = {0};
            if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
                return -1;
            }
            abs_dl.tv_sec = now.tv_sec + timeout->tv_sec;
            abs_dl.tv_nsec = now.tv_nsec + timeout->tv_nsec;
            if (abs_dl.tv_nsec >= 1000000000L) {
                abs_dl.tv_sec += 1;
                abs_dl.tv_nsec -= 1000000000L;
            }
            dl_ptr = &abs_dl;
        }
    }

    for (;;) {
        unsigned int seq = __atomic_load_n(&g_done_seq, __ATOMIC_ACQUIRE);
        int any_pending = 0;
        for (int i = 0; i < nent; i++) {
            if (!list[i]) {
                continue;
            }
            if (aio_error(list[i]) != EINPROGRESS) {
                return 0;
            }
            any_pending = 1;
        }
        if (!any_pending) {
            return 0;
        }
        if (zero_timeout) {
            errno = EAGAIN;
            return -1;
        }

        long r = __sys5(SYS_FUTEX, (long)(uintptr_t)&g_done_seq, FUTEX_WAIT,
                        (long)seq, (long)(uintptr_t)dl_ptr, CLOCK_MONOTONIC);
        if (r == -EINTR) {
            __libc_cancel_point();
            errno = EINTR;
            return -1;
        }
        if (r == -ETIMEDOUT) {
            errno = EAGAIN;
            return -1;
        }
    }
}

int lio_listio(int mode, struct aiocb *restrict const list[restrict], int nent,
               struct sigevent *restrict sig) {
    if ((mode != LIO_WAIT && mode != LIO_NOWAIT) || nent < 0 ||
        (nent > 0 && !list)) {
        errno = EINVAL;
        return -1;
    }

    unsigned int active = 0;
    for (int i = 0; i < nent; i++) {
        if (!list[i] || list[i]->aio_lio_opcode == LIO_NOP) {
            continue;
        }
        if (list[i]->aio_lio_opcode != LIO_READ &&
            list[i]->aio_lio_opcode != LIO_WRITE) {
            errno = EINVAL;
            return -1;
        }
        active++;
    }

    if (active == 0u) {
        if (mode == LIO_NOWAIT && sig) {
            notify_sigevent(sig);
        }
        return 0;
    }

    lio_batch_t *batch = NULL;
    if (mode == LIO_NOWAIT && sig) {
        q_lock();
        for (int i = 0; i < LIO_BATCH_MAX; i++) {
            if (!g_batches[i].used) {
                g_batches[i].used = 1;
                g_batches[i].remaining = active;
                g_batches[i].sig = *sig;
                batch = &g_batches[i];
                break;
            }
        }
        q_unlock();
        if (!batch) {
            errno = EAGAIN;
            return -1;
        }
    }

    int failed = 0;
    for (int i = 0; i < nent; i++) {
        struct aiocb *cb = list[i];
        if (!cb || cb->aio_lio_opcode == LIO_NOP) {
            continue;
        }
        unsigned int op =
            (cb->aio_lio_opcode == LIO_READ) ? AIO_OP_READ : AIO_OP_WRITE;
        if (enqueue_aio(cb, op, batch) != 0) {
            failed = 1;
            finish_batch_item(batch);
        }
    }

    if (mode == LIO_WAIT) {
        for (;;) {
            int pending = 0;
            for (int i = 0; i < nent; i++) {
                struct aiocb *cb = list[i];
                if (!cb || cb->aio_lio_opcode == LIO_NOP) {
                    continue;
                }
                int err = aio_error(cb);
                if (err == EINPROGRESS) {
                    pending = 1;
                } else if (err != 0) {
                    failed = 1;
                }
            }
            if (!pending) {
                break;
            }
            if (aio_suspend((const struct aiocb *const *)list, nent, NULL) !=
                0) {
                return -1;
            }
        }
    }

    if (failed) {
        errno = EIO;
        return -1;
    }
    return 0;
}
