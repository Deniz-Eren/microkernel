/**
 * @file     main.c
 * @brief    Ring 3 pipe and ppoll multiplexer server (/dev/pipe)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/thread.h>
#include <time.h>

/* need:: COMP_IPC_0005 */

#define PIPE_MAX 32
#define PIPE_BUF_SIZE 4096u
#define RWAIT_MAX 32
#define WWAIT_MAX 32
#define PWAIT_MAX 16

typedef struct {
    uint8_t used;
    uint32_t readers;
    uint32_t writers;
    uint32_t head;
    uint32_t len;
    uint8_t buf[PIPE_BUF_SIZE];
} pipe_obj_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t pipe_idx;
    uint32_t want_len;
} read_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t pipe_idx;
    uint32_t len;
    uint8_t data[PIPE_CHUNK_MAX];
} write_waiter_t;

typedef struct {
    uint8_t used;
    uint8_t has_deadline;
    uint32_t msgid;
    uint32_t nfds;
    uint64_t deadline_ns;
    pipe_poll_item_t items[PIPE_POLL_MAX];
} poll_waiter_t;

static pipe_obj_t g_pipes[PIPE_MAX];
static read_waiter_t g_rwait[RWAIT_MAX];
static write_waiter_t g_wwait[WWAIT_MAX];
static poll_waiter_t g_pwait[PWAIT_MAX];

static volatile unsigned int g_timed_polls;
static int g_self_conn = -1;

static uint64_t now_mono_ns(void) {
    struct timespec ts = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void ticker_thread(void *arg) {
    (void)arg;
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L}; /* 5 ms */
    for (;;) {
        while (__atomic_load_n(&g_timed_polls, __ATOMIC_ACQUIRE) == 0u) {
            __sys5(SYS_FUTEX, (long)(uintptr_t)&g_timed_polls, FUTEX_WAIT, 0, 0,
                   0);
        }
        clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
        if (g_self_conn >= 0) {
            SendPulse(g_self_conn, (int)PULSE_CODE_PIPE_TICK, 0);
        }
    }
}

static void inc_timed_polls(void) {
    unsigned int prev =
        __atomic_fetch_add(&g_timed_polls, 1u, __ATOMIC_ACQ_REL);
    if (prev == 0u) {
        __sys5(SYS_FUTEX, (long)(uintptr_t)&g_timed_polls, FUTEX_WAKE, 1, 0, 0);
    }
}

static void dec_timed_polls(void) {
    unsigned int cur = __atomic_load_n(&g_timed_polls, __ATOMIC_ACQUIRE);
    if (cur > 0u) {
        __atomic_fetch_sub(&g_timed_polls, 1u, __ATOMIC_ACQ_REL);
    }
}

static uint32_t pipe_peek(const pipe_obj_t *p, uint8_t *dst, uint32_t max_n) {
    uint32_t n = p->len < max_n ? p->len : max_n;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = p->buf[(p->head + i) % PIPE_BUF_SIZE];
    }
    return n;
}

static void pipe_consume(pipe_obj_t *p, uint32_t n) {
    p->head = (p->head + n) % PIPE_BUF_SIZE;
    p->len -= n;
}

static uint32_t pipe_push(pipe_obj_t *p, const uint8_t *src, uint32_t n) {
    uint32_t space = PIPE_BUF_SIZE - p->len;
    uint32_t take = n < space ? n : space;
    uint32_t tail = (p->head + p->len) % PIPE_BUF_SIZE;
    for (uint32_t i = 0; i < take; i++) {
        p->buf[(tail + i) % PIPE_BUF_SIZE] = src[i];
    }
    p->len += take;
    return take;
}

static int eval_poll_item(pipe_poll_item_t *it) {
    uint32_t idx = it->handle >> 1;
    uint32_t is_write = it->handle & 1u;
    if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used) {
        it->revents = POLLNVAL;
        return 1;
    }
    pipe_obj_t *p = &g_pipes[idx];
    short rev = 0;
    if (!is_write) {
        if (p->len > 0u) {
            rev = (short)(rev | (it->events & (POLLIN | POLLRDNORM)));
        }
        if (p->writers == 0u) {
            rev = (short)(rev | POLLHUP);
        }
    } else {
        if (p->readers == 0u) {
            rev = (short)(rev | POLLERR);
        } else if (p->len < PIPE_BUF_SIZE) {
            rev = (short)(rev | (it->events & (POLLOUT | POLLWRNORM)));
        }
    }
    it->revents = rev;
    return rev != 0;
}

static void service_pollers(uint64_t now_ns, int check_deadlines) {
    for (int i = 0; i < PWAIT_MAX; i++) {
        poll_waiter_t *pw = &g_pwait[i];
        if (!pw->used) {
            continue;
        }
        uint32_t ready = 0;
        for (uint32_t k = 0; k < pw->nfds; k++) {
            if (eval_poll_item(&pw->items[k])) {
                ready++;
            }
        }
        int expired =
            check_deadlines && pw->has_deadline && now_ns >= pw->deadline_ns;
        if (ready > 0u || expired) {
            pipe_poll_rep_t rep = {0};
            rep.err = 0;
            rep.ready = ready;
            memcpy(rep.items, pw->items, pw->nfds * sizeof(pipe_poll_item_t));
            (void)ReplyMsg((int)pw->msgid, &rep, sizeof(rep));
            if (pw->has_deadline) {
                dec_timed_polls();
            }
            pw->used = 0;
        }
    }
}

static void wake_pipe_waiters(uint32_t idx) {
    if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used) {
        return;
    }
    pipe_obj_t *p = &g_pipes[idx];

    /* 1. Wake blocked readers if data is available or all writers closed. */
    for (int i = 0; i < RWAIT_MAX; i++) {
        read_waiter_t *rw = &g_rwait[i];
        if (!rw->used || rw->pipe_idx != idx) {
            continue;
        }
        if (p->len == 0u && p->writers > 0u) {
            continue;
        }
        pipe_rep_t rep = {0};
        uint32_t n = pipe_peek(p, rep.data, rw->want_len);
        rep.err = 0;
        rep.len = n;
        long rc = ReplyMsg((int)rw->msgid, &rep, sizeof(rep));
        rw->used = 0;
        if (rc >= 0 && n > 0u) {
            pipe_consume(p, n);
        }
    }

    /* 2. Wake blocked writers if buffer space exists or all readers closed. */
    for (int i = 0; i < WWAIT_MAX; i++) {
        write_waiter_t *ww = &g_wwait[i];
        if (!ww->used || ww->pipe_idx != idx) {
            continue;
        }
        if (p->readers == 0u) {
            pipe_rep_t rep = {0};
            rep.err = -EPIPE;
            (void)ReplyMsg((int)ww->msgid, &rep, sizeof(rep));
            ww->used = 0;
            continue;
        }
        if (p->len < PIPE_BUF_SIZE) {
            uint32_t n = pipe_push(p, ww->data, ww->len);
            pipe_rep_t rep = {0};
            rep.err = 0;
            rep.len = n;
            long rc = ReplyMsg((int)ww->msgid, &rep, sizeof(rep));
            ww->used = 0;
            if (rc < 0) {
                /* Client left on EINTR: drop uncommitted bytes. */
                p->len -= n;
            }
        }
    }

    /* 3. Re-evaluate pollers after buffer/refcount changes. */
    service_pollers(0, 0);
}

static void handle_close_handle(uint32_t handle) {
    uint32_t idx = handle >> 1;
    uint32_t is_write = handle & 1u;
    if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used) {
        return;
    }
    pipe_obj_t *p = &g_pipes[idx];
    if (is_write) {
        if (p->writers > 0u) {
            p->writers--;
        }
    } else {
        if (p->readers > 0u) {
            p->readers--;
        }
    }
    wake_pipe_waiters(idx);
    if (p->readers == 0u && p->writers == 0u) {
        p->used = 0;
        p->head = 0;
        p->len = 0;
    }
}

static void handle_pipe_req(uint32_t msgid, const pipe_req_t *req) {
    pipe_rep_t rep = {0};

    switch (req->op) {
    case PIPE_OP_CREATE: {
        uint32_t slot = 0;
        for (uint32_t i = 1; i < PIPE_MAX; i++) {
            if (!g_pipes[i].used) {
                slot = i;
                break;
            }
        }
        if (slot == 0u) {
            rep.err = -ENFILE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        memset(&g_pipes[slot], 0, sizeof(g_pipes[slot]));
        g_pipes[slot].used = 1;
        g_pipes[slot].readers = 1;
        g_pipes[slot].writers = 1;
        rep.err = 0;
        rep.handle_r = (slot << 1) | 0u;
        rep.handle_w = (slot << 1) | 1u;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case PIPE_OP_DUP: {
        uint32_t idx = req->handle >> 1;
        uint32_t is_write = req->handle & 1u;
        if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used) {
            rep.err = -EBADF;
        } else {
            if (is_write) {
                g_pipes[idx].writers++;
            } else {
                g_pipes[idx].readers++;
            }
            rep.err = 0;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case PIPE_OP_CLOSE:
        handle_close_handle(req->handle);
        rep.err = 0;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    case PIPE_OP_READ: {
        uint32_t idx = req->handle >> 1;
        uint32_t is_write = req->handle & 1u;
        if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used || is_write) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t want = req->len > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : req->len;
        pipe_obj_t *p = &g_pipes[idx];
        if (want == 0u || p->len > 0u || p->writers == 0u) {
            uint32_t n = pipe_peek(p, rep.data, want);
            rep.err = 0;
            rep.len = n;
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0 && n > 0u) {
                pipe_consume(p, n);
                wake_pipe_waiters(idx);
            }
            return;
        }
        if (req->flags & (uint32_t)O_NONBLOCK) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < RWAIT_MAX; i++) {
            if (!g_rwait[i].used) {
                g_rwait[i].used = 1;
                g_rwait[i].msgid = msgid;
                g_rwait[i].pipe_idx = idx;
                g_rwait[i].want_len = want;
                return; /* deferred ReplyMsg */
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case PIPE_OP_WRITE: {
        uint32_t idx = req->handle >> 1;
        uint32_t is_write = req->handle & 1u;
        if (idx == 0u || idx >= PIPE_MAX || !g_pipes[idx].used || !is_write) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t want = req->len > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : req->len;
        pipe_obj_t *p = &g_pipes[idx];
        if (p->readers == 0u) {
            rep.err = -EPIPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (want == 0u || p->len < PIPE_BUF_SIZE) {
            uint32_t n = pipe_push(p, req->data, want);
            rep.err = 0;
            rep.len = n;
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0) {
                wake_pipe_waiters(idx);
            } else {
                p->len -= n;
            }
            return;
        }
        if (req->flags & (uint32_t)O_NONBLOCK) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < WWAIT_MAX; i++) {
            if (!g_wwait[i].used) {
                g_wwait[i].used = 1;
                g_wwait[i].msgid = msgid;
                g_wwait[i].pipe_idx = idx;
                g_wwait[i].len = want;
                memcpy(g_wwait[i].data, req->data, want);
                return; /* deferred ReplyMsg */
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    default:
        rep.err = -EINVAL;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
}

static void handle_poll_req(uint32_t msgid, const pipe_poll_req_t *preq) {
    pipe_poll_rep_t rep = {0};
    uint32_t nfds = preq->nfds;
    if (nfds > PIPE_POLL_MAX) {
        rep.err = -EINVAL;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }

    uint32_t ready = 0;
    for (uint32_t i = 0; i < nfds; i++) {
        rep.items[i] = preq->items[i];
        if (eval_poll_item(&rep.items[i])) {
            ready++;
        }
    }
    if (ready > 0u || preq->timeout_ns == 0) {
        rep.err = 0;
        rep.ready = ready;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }

    for (int i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            g_pwait[i].used = 1;
            g_pwait[i].msgid = msgid;
            g_pwait[i].nfds = nfds;
            memcpy(g_pwait[i].items, preq->items,
                   nfds * sizeof(pipe_poll_item_t));
            if (preq->timeout_ns > 0) {
                g_pwait[i].has_deadline = 1;
                g_pwait[i].deadline_ns =
                    now_mono_ns() + (uint64_t)preq->timeout_ns;
                inc_timed_polls();
            } else {
                g_pwait[i].has_deadline = 0;
                g_pwait[i].deadline_ns = 0;
            }
            return; /* deferred ReplyMsg */
        }
    }
    rep.err = -EAGAIN;
    ReplyMsg((int)msgid, &rep, sizeof(rep));
}

int main(void) {
    long chan = CreateChannel();
    if (chan < 0) {
        printf("[FAIL] pipeserv CreateChannel -> %ld\n", chan);
        return 1;
    }

    long ns_conn = -1;
    for (int tries = 0; tries < 200 && ns_conn < 0; tries++) {
        ns_conn = OpenConnection(1, 1);
        if (ns_conn < 0) {
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
        }
    }
    if (ns_conn < 0) {
        printf("[FAIL] pipeserv cannot connect to sysproc\n");
        return 1;
    }

    name_msg_t reg = {0};
    reg.type = NAME_MSG_ATTACH;
    reg.chanid = (uint32_t)chan;
    strcpy(reg.path, "/dev/pipe");
    name_reply_t nrep = {0};
    if (SendMsg((int)ns_conn, &reg, sizeof(reg), &nrep, sizeof(nrep)) < 0 ||
        nrep.err != 0) {
        printf("[FAIL] pipeserv failed to register /dev/pipe\n");
        return 1;
    }
    CloseConnection((int)ns_conn);

    g_self_conn =
        (int)OpenConnection((uint32_t)__sys0(SYS_PROC_GETPID), (uint32_t)chan);
    ThreadSpawn(ticker_thread, NULL, 90);
    printf("[OK] pipeserv registered /dev/pipe (chanid=%ld)\n", chan);

    union {
        uint32_t op;
        pipe_req_t req;
        pipe_poll_req_t preq;
        ipc_pulse_t pulse;
        uint8_t raw[512];
    } msg;
    ipc_recv_info_t info = {0};

    for (;;) {
        memset(&msg, 0, sizeof(msg));
        long msgid = ReceiveMsg((int)chan, &msg, sizeof(msg), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            if (msg.pulse.code == PULSE_CODE_FD_CLOSE) {
                handle_close_handle(msg.pulse.value);
            } else if (msg.pulse.code == PULSE_CODE_PIPE_TICK) {
                service_pollers(now_mono_ns(), 1);
            }
            continue;
        }
        if (msg.op == PIPE_OP_POLL) {
            handle_poll_req((uint32_t)msgid, &msg.preq);
        } else {
            handle_pipe_req((uint32_t)msgid, &msg.req);
        }
    }
}
