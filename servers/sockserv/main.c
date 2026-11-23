/**
 * @file     main.c
 * @brief    Ring 3 POSIX.1-2024 AF_UNIX local sockets server (/dev/sock)
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
#include <sys/sock.h>
#include <sys/socket.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_IPC_0012 COMP_IPC_0020 */

#define SOCK_MAX 48
#define SOCK_BUF_SIZE 4096u
#define DGRAM_Q_MAX 8
#define RIGHTS_Q_MAX 8
#define ACCEPT_Q_MAX 16
#define RWAIT_MAX 32
#define WWAIT_MAX 32
#define AWAIT_MAX 16
#define PWAIT_MAX 16

enum {
    ST_UNBOUND = 0,
    ST_BOUND = 1,
    ST_LISTENING = 2,
    ST_CONNECTED = 3,
};

typedef struct {
    uint32_t len;
    char sender[SOCK_PATH_MAX];
    uint8_t data[SOCK_DGRAM_DATA_MAX];
    uint32_t rights_count;
    struct __k_fd_entry rights[SOCK_MAX_RIGHTS];
} dgram_pkt_t;

typedef struct {
    uint64_t byte_pos;
    uint32_t count;
    struct __k_fd_entry fds[SOCK_MAX_RIGHTS];
} rights_pkt_t;

typedef struct {
    uint8_t used;
    uint8_t state;
    uint8_t shut_rd;
    uint8_t shut_wr;
    uint8_t peer_closed;
    uint8_t so_reuseaddr;
    int32_t type;
    int32_t so_error;
    uint32_t refs;
    uint32_t peer_idx;
    uint32_t backlog;
    char bound_path[SOCK_PATH_MAX];

    /* SOCK_STREAM receive ring buffer. */
    uint32_t rx_head;
    uint32_t rx_len;
    uint64_t rx_total_written;
    uint64_t rx_total_read;
    uint8_t rx_buf[SOCK_BUF_SIZE];

    /* SOCK_STREAM ancillary SCM_RIGHTS queue ordered by stream offset. */
    uint32_t rights_head;
    uint32_t rights_len;
    rights_pkt_t rights_q[RIGHTS_Q_MAX];

    /* SOCK_STREAM listening accept queue (server-side connected indices). */
    uint32_t accept_head;
    uint32_t accept_len;
    uint32_t accept_q[ACCEPT_Q_MAX];

    /* SOCK_DGRAM receive queue. */
    uint32_t dg_head;
    uint32_t dg_len;
    dgram_pkt_t dg_q[DGRAM_Q_MAX];
} sock_obj_t;

typedef struct {
    uint8_t used;
    uint8_t is_msg;
    uint32_t msgid;
    uint32_t sock_idx;
    uint32_t want_len;
    uint32_t flags;
} recv_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t sock_idx;
    uint32_t len;
    uint32_t rights_count;
    struct __k_fd_entry rights[SOCK_MAX_RIGHTS];
    uint8_t data[SOCK_DATA_MAX];
} send_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t sock_idx;
} accept_waiter_t;

typedef struct {
    uint8_t used;
    uint8_t has_deadline;
    uint32_t msgid;
    uint32_t nfds;
    uint64_t deadline_ns;
    sock_poll_item_t items[SOCK_POLL_MAX];
} poll_waiter_t;

static sock_obj_t g_socks[SOCK_MAX];
static recv_waiter_t g_rwait[RWAIT_MAX];
static send_waiter_t g_wwait[WWAIT_MAX];
static accept_waiter_t g_await[AWAIT_MAX];
static poll_waiter_t g_pwait[PWAIT_MAX];

static volatile unsigned int g_timed_polls;
static int g_self_conn = -1;
static uint32_t g_self_pid;

static void handle_close_handle(uint32_t handle);

static void release_rights_entries(const struct __k_fd_entry *fds,
                                   uint32_t count) {
    for (uint32_t i = 0; i < count && i < SOCK_MAX_RIGHTS; i++) {
        const struct __k_fd_entry *e = &fds[i];
        if (e->kind != FD_KIND_SERVER && e->kind != FD_KIND_VFS &&
            e->kind != FD_KIND_SOCK) {
            continue;
        }
        if (e->srv_pid == 0u || e->srv_chanid == 0u) {
            continue;
        }
        if (e->srv_pid == g_self_pid) {
            handle_close_handle(e->srv_handle);
            continue;
        }
        long c = OpenConnection(e->srv_pid, e->srv_chanid);
        if (c >= 0) {
            (void)SendPulse((int)c, (int)PULSE_CODE_FD_CLOSE,
                            (int)e->srv_handle);
            CloseConnection((int)c);
        }
    }
}

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
            SendPulse(g_self_conn, (int)PULSE_CODE_SOCK_TICK, 0);
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

static sock_obj_t *get_sock(uint32_t handle) {
    if (handle == 0u || handle >= SOCK_MAX || !g_socks[handle].used) {
        return NULL;
    }
    return &g_socks[handle];
}

static uint32_t alloc_sock_slot(int32_t type) {
    for (uint32_t i = 1; i < SOCK_MAX; i++) {
        if (!g_socks[i].used) {
            memset(&g_socks[i], 0, sizeof(g_socks[i]));
            g_socks[i].used = 1;
            g_socks[i].state = ST_UNBOUND;
            g_socks[i].type = type;
            g_socks[i].refs = 1;
            return i;
        }
    }
    return 0;
}

static uint32_t rx_peek(const sock_obj_t *s, uint8_t *dst, uint32_t max_n) {
    uint32_t n = s->rx_len < max_n ? s->rx_len : max_n;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = s->rx_buf[(s->rx_head + i) % SOCK_BUF_SIZE];
    }
    return n;
}

static void rx_consume(sock_obj_t *s, uint32_t n) {
    s->rx_head = (s->rx_head + n) % SOCK_BUF_SIZE;
    s->rx_len -= n;
    s->rx_total_read += (uint64_t)n;
}

static uint32_t rx_push(sock_obj_t *s, const uint8_t *src, uint32_t n) {
    uint32_t space = SOCK_BUF_SIZE - s->rx_len;
    uint32_t take = n < space ? n : space;
    uint32_t tail = (s->rx_head + s->rx_len) % SOCK_BUF_SIZE;
    for (uint32_t i = 0; i < take; i++) {
        s->rx_buf[(tail + i) % SOCK_BUF_SIZE] = src[i];
    }
    s->rx_len += take;
    s->rx_total_written += (uint64_t)take;
    return take;
}

static void rights_push(sock_obj_t *s, uint64_t byte_pos,
                        const struct __k_fd_entry *fds, uint32_t count) {
    if (count == 0u || count > SOCK_MAX_RIGHTS ||
        s->rights_len >= RIGHTS_Q_MAX) {
        return;
    }
    uint32_t tail = (s->rights_head + s->rights_len) % RIGHTS_Q_MAX;
    s->rights_q[tail].byte_pos = byte_pos;
    s->rights_q[tail].count = count;
    memcpy(s->rights_q[tail].fds, fds, count * sizeof(struct __k_fd_entry));
    s->rights_len++;
}

static uint32_t stream_recv_prepare(sock_obj_t *s, uint32_t want, int is_msg,
                                    sock_rep_t *rep, int *pop_rights_out) {
    *pop_rights_out = 0;
    uint32_t cap = want;
    if (is_msg && cap > SOCK_MSG_DATA_MAX) {
        cap = SOCK_MSG_DATA_MAX;
    }
    if (s->rights_len > 0u) {
        const rights_pkt_t *rp = &s->rights_q[s->rights_head];
        if (rp->byte_pos == s->rx_total_read) {
            if (is_msg) {
                rep->u.msg.rights_count = rp->count;
                memcpy(rep->u.msg.rights, rp->fds,
                       rp->count * sizeof(struct __k_fd_entry));
                *pop_rights_out = 1;
                if (s->rights_len > 1u) {
                    const rights_pkt_t *next =
                        &s->rights_q[(s->rights_head + 1u) % RIGHTS_Q_MAX];
                    uint64_t max_bytes = next->byte_pos > s->rx_total_read
                                             ? next->byte_pos - s->rx_total_read
                                             : 1u;
                    if ((uint64_t)cap > max_bytes) {
                        cap = (uint32_t)max_bytes;
                    }
                }
            } else {
                *pop_rights_out = 2; /* discard unread rights on plain recv */
            }
        } else if (rp->byte_pos > s->rx_total_read) {
            uint64_t diff = rp->byte_pos - s->rx_total_read;
            if ((uint64_t)cap > diff) {
                cap = (uint32_t)diff;
            }
        }
    }
    uint8_t *dst = is_msg ? rep->u.msg.data : rep->u.data;
    return rx_peek(s, dst, cap);
}

static void stream_recv_commit(sock_obj_t *s, uint32_t n, uint32_t flags,
                               int pop_rights) {
    if (flags & (uint32_t)MSG_PEEK) {
        return;
    }
    if (n > 0u) {
        rx_consume(s, n);
    }
    if (pop_rights != 0 && s->rights_len > 0u) {
        if (pop_rights == 2) {
            const rights_pkt_t *rp = &s->rights_q[s->rights_head];
            release_rights_entries(rp->fds, rp->count);
        }
        s->rights_head = (s->rights_head + 1u) % RIGHTS_Q_MAX;
        s->rights_len--;
    }
}

static uint32_t find_bound_sock(const char *path, int32_t type,
                                int want_listen) {
    if (!path || !path[0]) {
        return 0;
    }
    for (uint32_t i = 1; i < SOCK_MAX; i++) {
        sock_obj_t *s = &g_socks[i];
        if (!s->used || !s->bound_path[0]) {
            continue;
        }
        if (strcmp(s->bound_path, path) != 0) {
            continue;
        }
        if (type != 0 && s->type != type) {
            continue;
        }
        if (want_listen && s->state != ST_LISTENING) {
            continue;
        }
        return i;
    }
    return 0;
}

static int eval_poll_item(sock_poll_item_t *it) {
    sock_obj_t *s = get_sock(it->handle);
    if (!s) {
        it->revents = POLLNVAL;
        return 1;
    }
    short rev = 0;
    if (s->type == SOCK_DGRAM) {
        if (s->dg_len > 0u || s->shut_rd) {
            rev = (short)(rev | (it->events & (POLLIN | POLLRDNORM)));
        }
        if (!s->shut_wr) {
            rev = (short)(rev | (it->events & (POLLOUT | POLLWRNORM)));
        }
    } else if (s->state == ST_LISTENING) {
        if (s->accept_len > 0u) {
            rev = (short)(rev | (it->events & (POLLIN | POLLRDNORM)));
        }
    } else if (s->state == ST_CONNECTED || s->peer_closed) {
        sock_obj_t *peer = get_sock(s->peer_idx);
        int peer_eof = (!peer || s->peer_closed || peer->shut_wr);
        if (s->rx_len > 0u || peer_eof || s->shut_rd) {
            rev = (short)(rev | (it->events & (POLLIN | POLLRDNORM)));
        }
        if (peer_eof) {
            rev = (short)(rev | POLLHUP);
        } else if (!s->shut_wr && !peer->shut_rd &&
                   peer->rx_len < SOCK_BUF_SIZE) {
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
            sock_poll_rep_t rep = {0};
            rep.err = 0;
            rep.ready = ready;
            memcpy(rep.items, pw->items, pw->nfds * sizeof(sock_poll_item_t));
            (void)ReplyMsg((int)pw->msgid, &rep, sizeof(rep));
            if (pw->has_deadline) {
                dec_timed_polls();
            }
            pw->used = 0;
        }
    }
}

static void wake_sock_waiters(uint32_t idx) {
    sock_obj_t *s = get_sock(idx);
    if (!s) {
        return;
    }

    /* 1. Wake deferred accept() waiters on listening sockets. */
    if (s->state == ST_LISTENING && s->accept_len > 0u) {
        for (int i = 0; i < AWAIT_MAX && s->accept_len > 0u; i++) {
            accept_waiter_t *aw = &g_await[i];
            if (!aw->used || aw->sock_idx != idx) {
                continue;
            }
            uint32_t acc_idx = s->accept_q[s->accept_head];
            sock_rep_t rep = {0};
            rep.err = 0;
            rep.handle = acc_idx;
            sock_obj_t *acc = get_sock(acc_idx);
            sock_obj_t *peer = acc ? get_sock(acc->peer_idx) : NULL;
            if (peer && peer->bound_path[0]) {
                strlcpy(rep.u.path, peer->bound_path, sizeof(rep.u.path));
            }
            long rc = ReplyMsg((int)aw->msgid, &rep, sizeof(rep));
            aw->used = 0;
            if (rc >= 0) {
                s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
                s->accept_len--;
            }
        }
    }

    /* 2. Wake deferred recv() waiters. */
    for (int i = 0; i < RWAIT_MAX; i++) {
        recv_waiter_t *rw = &g_rwait[i];
        if (!rw->used || rw->sock_idx != idx) {
            continue;
        }
        if (s->type == SOCK_DGRAM) {
            if (s->dg_len == 0u && !s->shut_rd) {
                continue;
            }
            sock_rep_t rep = {0};
            rep.err = 0;
            if (s->dg_len > 0u) {
                dgram_pkt_t *pkt = &s->dg_q[s->dg_head];
                uint32_t copy =
                    pkt->len < rw->want_len ? pkt->len : rw->want_len;
                rep.len = copy;
                if (rw->is_msg) {
                    rep.u.msg.rights_count = pkt->rights_count;
                    if (pkt->rights_count > 0u) {
                        memcpy(rep.u.msg.rights, pkt->rights,
                               pkt->rights_count * sizeof(struct __k_fd_entry));
                    }
                    if (copy > 0u) {
                        memcpy(rep.u.msg.data, pkt->data, copy);
                    }
                } else {
                    strlcpy(rep.u.dgram.path, pkt->sender,
                            sizeof(rep.u.dgram.path));
                    if (copy > 0u) {
                        memcpy(rep.u.dgram.data, pkt->data, copy);
                    }
                }
                long rc = ReplyMsg((int)rw->msgid, &rep, sizeof(rep));
                rw->used = 0;
                if (rc >= 0 && !(rw->flags & (uint32_t)MSG_PEEK)) {
                    if (!rw->is_msg && pkt->rights_count > 0u) {
                        release_rights_entries(pkt->rights, pkt->rights_count);
                    }
                    s->dg_head = (s->dg_head + 1u) % DGRAM_Q_MAX;
                    s->dg_len--;
                }
            } else {
                (void)ReplyMsg((int)rw->msgid, &rep, sizeof(rep));
                rw->used = 0;
            }
            continue;
        }

        sock_obj_t *peer = get_sock(s->peer_idx);
        int peer_eof = (!peer || s->peer_closed || peer->shut_wr || s->shut_rd);
        if (s->rx_len == 0u && s->rights_len == 0u && !peer_eof) {
            continue;
        }
        sock_rep_t rep = {0};
        int pop_rights = 0;
        uint32_t n = stream_recv_prepare(s, rw->want_len, (int)rw->is_msg, &rep,
                                         &pop_rights);
        rep.err = 0;
        rep.len = n;
        long rc = ReplyMsg((int)rw->msgid, &rep, sizeof(rep));
        rw->used = 0;
        if (rc >= 0) {
            stream_recv_commit(s, n, rw->flags, pop_rights);
        }
    }

    /* 3. Wake deferred send() waiters waiting for space in peer's rx_buf. */
    for (int i = 0; i < WWAIT_MAX; i++) {
        send_waiter_t *ww = &g_wwait[i];
        if (!ww->used) {
            continue;
        }
        sock_obj_t *sender = get_sock(ww->sock_idx);
        if (!sender || sender->peer_idx != idx) {
            continue;
        }
        if (s->shut_rd || sender->shut_wr || sender->peer_closed) {
            if (ww->rights_count > 0u) {
                release_rights_entries(ww->rights, ww->rights_count);
                ww->rights_count = 0u;
            }
            sock_rep_t rep = {0};
            rep.err = -EPIPE;
            (void)ReplyMsg((int)ww->msgid, &rep, sizeof(rep));
            ww->used = 0;
            continue;
        }
        if (s->rx_len < SOCK_BUF_SIZE &&
            (ww->rights_count == 0u || s->rights_len < RIGHTS_Q_MAX)) {
            uint64_t rpos = s->rx_total_written;
            uint32_t n = rx_push(s, ww->data, ww->len);
            if (ww->rights_count > 0u) {
                rights_push(s, rpos, ww->rights, ww->rights_count);
            }
            sock_rep_t rep = {0};
            rep.err = 0;
            rep.len = n;
            long rc = ReplyMsg((int)ww->msgid, &rep, sizeof(rep));
            uint32_t rcnt = ww->rights_count;
            ww->rights_count = 0u;
            ww->used = 0;
            if (rc < 0) {
                s->rx_len -= n;
                s->rx_total_written -= (uint64_t)n;
                if (rcnt > 0u && s->rights_len > 0u) {
                    s->rights_len--;
                    release_rights_entries(ww->rights, rcnt);
                }
            } else {
                wake_sock_waiters(idx);
            }
        }
    }

    service_pollers(0, 0);
}

static void fail_waiters_on_close(uint32_t idx) {
    for (int i = 0; i < AWAIT_MAX; i++) {
        if (g_await[i].used && g_await[i].sock_idx == idx) {
            sock_rep_t rep = {0};
            rep.err = -EBADF;
            (void)ReplyMsg((int)g_await[i].msgid, &rep, sizeof(rep));
            g_await[i].used = 0;
        }
    }
    for (int i = 0; i < RWAIT_MAX; i++) {
        if (g_rwait[i].used && g_rwait[i].sock_idx == idx) {
            sock_rep_t rep = {0};
            rep.err = -EBADF;
            (void)ReplyMsg((int)g_rwait[i].msgid, &rep, sizeof(rep));
            g_rwait[i].used = 0;
        }
    }
    for (int i = 0; i < WWAIT_MAX; i++) {
        if (g_wwait[i].used && g_wwait[i].sock_idx == idx) {
            sock_rep_t rep = {0};
            rep.err = -EPIPE;
            (void)ReplyMsg((int)g_wwait[i].msgid, &rep, sizeof(rep));
            g_wwait[i].used = 0;
        }
    }
}

static void handle_close_handle(uint32_t handle) {
    sock_obj_t *s = get_sock(handle);
    if (!s) {
        return;
    }
    if (s->refs > 1u) {
        s->refs--;
        return;
    }

    fail_waiters_on_close(handle);

    /* Close any queued unaccepted server-side sockets. */
    if (s->state == ST_LISTENING) {
        while (s->accept_len > 0u) {
            uint32_t qidx = s->accept_q[s->accept_head];
            s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
            s->accept_len--;
            handle_close_handle(qidx);
        }
    }

    /* Release any unread SCM_RIGHTS FD entries queued on this socket. */
    while (s->rights_len > 0u) {
        const rights_pkt_t *rp = &s->rights_q[s->rights_head];
        release_rights_entries(rp->fds, rp->count);
        s->rights_head = (s->rights_head + 1u) % RIGHTS_Q_MAX;
        s->rights_len--;
    }
    while (s->dg_len > 0u) {
        const dgram_pkt_t *pkt = &s->dg_q[s->dg_head];
        if (pkt->rights_count > 0u) {
            release_rights_entries(pkt->rights, pkt->rights_count);
        }
        s->dg_head = (s->dg_head + 1u) % DGRAM_Q_MAX;
        s->dg_len--;
    }

    /* Notify connected peer of EOF / EPIPE. */
    uint32_t pidx = s->peer_idx;
    s->peer_idx = 0;
    s->used = 0;
    s->bound_path[0] = '\0';

    sock_obj_t *peer = get_sock(pidx);
    if (peer && peer->peer_idx == handle) {
        peer->peer_idx = 0;
        peer->peer_closed = 1;
        /* Fail any pending send waiters from peer targeting the closed sock. */
        for (int i = 0; i < WWAIT_MAX; i++) {
            if (g_wwait[i].used && g_wwait[i].sock_idx == pidx) {
                sock_rep_t rep = {0};
                rep.err = -EPIPE;
                (void)ReplyMsg((int)g_wwait[i].msgid, &rep, sizeof(rep));
                g_wwait[i].used = 0;
            }
        }
        wake_sock_waiters(pidx);
    }
    service_pollers(0, 0);
}

static void handle_sock_req(uint32_t msgid, const sock_req_t *req) {
    sock_rep_t rep = {0};

    switch (req->op) {
    case SOCK_OP_CREATE: {
        int32_t type = (int32_t)req->arg1;
        if (type != SOCK_STREAM && type != SOCK_DGRAM) {
            rep.err = -EPROTOTYPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t slot = alloc_sock_slot(type);
        if (slot == 0u) {
            rep.err = -ENFILE;
        } else {
            rep.err = 0;
            rep.handle = slot;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_PAIR: {
        int32_t type = (int32_t)req->arg1;
        if (type != SOCK_STREAM && type != SOCK_DGRAM) {
            rep.err = -EPROTOTYPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t a = alloc_sock_slot(type);
        uint32_t b = a ? alloc_sock_slot(type) : 0u;
        if (a == 0u || b == 0u) {
            if (a) {
                g_socks[a].used = 0;
            }
            rep.err = -ENFILE;
        } else {
            g_socks[a].state = ST_CONNECTED;
            g_socks[b].state = ST_CONNECTED;
            g_socks[a].peer_idx = b;
            g_socks[b].peer_idx = a;
            rep.err = 0;
            rep.handle = a;
            rep.handle2 = b;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_DUP: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else {
            s->refs++;
            rep.err = 0;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_CLOSE:
        handle_close_handle(req->handle);
        rep.err = 0;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    case SOCK_OP_BIND: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (s->bound_path[0] != '\0') {
            rep.err = -EINVAL;
        } else if (req->u.path[0] == '\0') {
            rep.err = -EINVAL;
        } else if (find_bound_sock(req->u.path, 0, 0) != 0u) {
            rep.err = -EADDRINUSE;
        } else {
            strlcpy(s->bound_path, req->u.path, sizeof(s->bound_path));
            if (s->state == ST_UNBOUND) {
                s->state = ST_BOUND;
            }
            rep.err = 0;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_LISTEN: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (s->type != SOCK_STREAM) {
            rep.err = -EOPNOTSUPP;
        } else if (s->bound_path[0] == '\0') {
            rep.err = -EDESTADDRREQ;
        } else {
            uint32_t bl = req->arg1;
            if (bl == 0u) {
                bl = 1u;
            }
            if (bl > ACCEPT_Q_MAX) {
                bl = ACCEPT_Q_MAX;
            }
            s->backlog = bl;
            s->state = ST_LISTENING;
            rep.err = 0;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_ACCEPT: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->type != SOCK_STREAM || s->state != ST_LISTENING) {
            rep.err = -EINVAL;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->accept_len > 0u) {
            uint32_t acc_idx = s->accept_q[s->accept_head];
            rep.err = 0;
            rep.handle = acc_idx;
            sock_obj_t *acc = get_sock(acc_idx);
            sock_obj_t *peer = acc ? get_sock(acc->peer_idx) : NULL;
            if (peer && peer->bound_path[0]) {
                strlcpy(rep.u.path, peer->bound_path, sizeof(rep.u.path));
            }
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0) {
                s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
                s->accept_len--;
                service_pollers(0, 0);
            }
            return;
        }
        if (req->flags & (uint32_t)O_NONBLOCK) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < AWAIT_MAX; i++) {
            if (!g_await[i].used) {
                g_await[i].used = 1;
                g_await[i].msgid = msgid;
                g_await[i].sock_idx = req->handle;
                return; /* deferred ReplyMsg */
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_CONNECT: {
        sock_obj_t *c = get_sock(req->handle);
        if (!c) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (c->type == SOCK_DGRAM) {
            uint32_t dst = find_bound_sock(req->u.path, SOCK_DGRAM, 0);
            if (dst == 0u) {
                rep.err = -ENOENT;
            } else {
                c->peer_idx = dst;
                c->state = ST_CONNECTED;
                rep.err = 0;
            }
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (c->state == ST_CONNECTED) {
            rep.err = -EISCONN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (c->state == ST_LISTENING) {
            rep.err = -EOPNOTSUPP;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t lidx = find_bound_sock(req->u.path, SOCK_STREAM, 1);
        if (lidx == 0u) {
            rep.err =
                find_bound_sock(req->u.path, 0, 0) ? -ECONNREFUSED : -ENOENT;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        sock_obj_t *listener = &g_socks[lidx];
        if (listener->accept_len >= listener->backlog) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t srv_idx = alloc_sock_slot(SOCK_STREAM);
        if (srv_idx == 0u) {
            rep.err = -ENFILE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        sock_obj_t *srv = &g_socks[srv_idx];
        srv->state = ST_CONNECTED;
        srv->peer_idx = req->handle;
        strlcpy(srv->bound_path, listener->bound_path, sizeof(srv->bound_path));

        c->state = ST_CONNECTED;
        c->peer_idx = srv_idx;
        c->peer_closed = 0;

        uint32_t tail =
            (listener->accept_head + listener->accept_len) % ACCEPT_Q_MAX;
        listener->accept_q[tail] = srv_idx;
        listener->accept_len++;

        rep.err = 0;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        wake_sock_waiters(lidx);
        return;
    }
    case SOCK_OP_SEND: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->shut_wr) {
            rep.err = -EPIPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->type == SOCK_DGRAM) {
            uint32_t dst_idx = 0u;
            const uint8_t *payload = NULL;
            uint32_t plen = req->len;
            if (req->arg1 != 0u) {
                /* sendto() with explicit destination path in req->u.dgram. */
                if (plen > SOCK_DGRAM_DATA_MAX) {
                    rep.err = -EMSGSIZE;
                    ReplyMsg((int)msgid, &rep, sizeof(rep));
                    return;
                }
                dst_idx = find_bound_sock(req->u.dgram.path, SOCK_DGRAM, 0);
                payload = req->u.dgram.data;
            } else {
                if (plen > SOCK_DGRAM_DATA_MAX) {
                    rep.err = -EMSGSIZE;
                    ReplyMsg((int)msgid, &rep, sizeof(rep));
                    return;
                }
                dst_idx = s->peer_idx;
                payload = req->u.data;
            }
            sock_obj_t *dst = get_sock(dst_idx);
            if (!dst) {
                rep.err = (req->arg1 != 0u) ? -ENOENT : -EDESTADDRREQ;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            if (dst->dg_len >= DGRAM_Q_MAX) {
                rep.err = -EAGAIN;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            uint32_t tail = (dst->dg_head + dst->dg_len) % DGRAM_Q_MAX;
            dgram_pkt_t *pkt = &dst->dg_q[tail];
            pkt->len = plen;
            pkt->rights_count = 0u;
            strlcpy(pkt->sender, s->bound_path, sizeof(pkt->sender));
            if (plen > 0u) {
                memcpy(pkt->data, payload, plen);
            }
            dst->dg_len++;
            rep.err = 0;
            rep.len = plen;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            wake_sock_waiters(dst_idx);
            return;
        }

        /* SOCK_STREAM send */
        if (s->state != ST_CONNECTED && !s->peer_closed) {
            rep.err = -ENOTCONN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        sock_obj_t *peer = get_sock(s->peer_idx);
        if (!peer || s->peer_closed || peer->shut_rd) {
            rep.err = -EPIPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t wlen = req->len > SOCK_DATA_MAX ? SOCK_DATA_MAX : req->len;
        if (wlen == 0u) {
            rep.err = 0;
            rep.len = 0;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (peer->rx_len < SOCK_BUF_SIZE) {
            uint32_t n = rx_push(peer, req->u.data, wlen);
            rep.err = 0;
            rep.len = n;
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0) {
                wake_sock_waiters(s->peer_idx);
            } else {
                peer->rx_len -= n;
                peer->rx_total_written -= (uint64_t)n;
            }
            return;
        }
        if (req->flags & ((uint32_t)O_NONBLOCK | (uint32_t)MSG_DONTWAIT)) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < WWAIT_MAX; i++) {
            if (!g_wwait[i].used) {
                g_wwait[i].used = 1;
                g_wwait[i].msgid = msgid;
                g_wwait[i].sock_idx = req->handle;
                g_wwait[i].len = wlen;
                g_wwait[i].rights_count = 0u;
                memcpy(g_wwait[i].data, req->u.data, wlen);
                return; /* deferred ReplyMsg */
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_SENDMSG: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->shut_wr) {
            rep.err = -EPIPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t rcnt = req->u.msg.rights_count;
        if (rcnt > SOCK_MAX_RIGHTS) {
            rep.err = -EINVAL;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t wlen =
            req->len > SOCK_MSG_DATA_MAX ? SOCK_MSG_DATA_MAX : req->len;
        if (s->type == SOCK_DGRAM) {
            sock_obj_t *dst = get_sock(s->peer_idx);
            if (!dst) {
                rep.err = -EDESTADDRREQ;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            if (dst->dg_len >= DGRAM_Q_MAX) {
                rep.err = -EAGAIN;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            uint32_t tail = (dst->dg_head + dst->dg_len) % DGRAM_Q_MAX;
            dgram_pkt_t *pkt = &dst->dg_q[tail];
            pkt->len = wlen;
            pkt->rights_count = rcnt;
            if (rcnt > 0u) {
                memcpy(pkt->rights, req->u.msg.rights,
                       rcnt * sizeof(struct __k_fd_entry));
            }
            strlcpy(pkt->sender, s->bound_path, sizeof(pkt->sender));
            if (wlen > 0u) {
                memcpy(pkt->data, req->u.msg.data, wlen);
            }
            dst->dg_len++;
            rep.err = 0;
            rep.len = wlen;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            wake_sock_waiters(s->peer_idx);
            return;
        }
        if (s->state != ST_CONNECTED && !s->peer_closed) {
            rep.err = -ENOTCONN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        sock_obj_t *peer = get_sock(s->peer_idx);
        if (!peer || s->peer_closed || peer->shut_rd) {
            rep.err = -EPIPE;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (wlen == 0u && rcnt == 0u) {
            rep.err = 0;
            rep.len = 0;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if ((wlen == 0u || peer->rx_len < SOCK_BUF_SIZE) &&
            (rcnt == 0u || peer->rights_len < RIGHTS_Q_MAX)) {
            uint64_t rpos = peer->rx_total_written;
            uint32_t n =
                (wlen > 0u) ? rx_push(peer, req->u.msg.data, wlen) : 0u;
            if (rcnt > 0u) {
                rights_push(peer, rpos, req->u.msg.rights, rcnt);
            }
            rep.err = 0;
            rep.len = n;
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0) {
                wake_sock_waiters(s->peer_idx);
            } else {
                peer->rx_len -= n;
                peer->rx_total_written -= (uint64_t)n;
                if (rcnt > 0u && peer->rights_len > 0u) {
                    peer->rights_len--;
                }
            }
            return;
        }
        if (req->flags & ((uint32_t)O_NONBLOCK | (uint32_t)MSG_DONTWAIT)) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < WWAIT_MAX; i++) {
            if (!g_wwait[i].used) {
                g_wwait[i].used = 1;
                g_wwait[i].msgid = msgid;
                g_wwait[i].sock_idx = req->handle;
                g_wwait[i].len = wlen;
                g_wwait[i].rights_count = rcnt;
                if (rcnt > 0u) {
                    memcpy(g_wwait[i].rights, req->u.msg.rights,
                           rcnt * sizeof(struct __k_fd_entry));
                }
                if (wlen > 0u) {
                    memcpy(g_wwait[i].data, req->u.msg.data, wlen);
                }
                return;
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_RECV:
    case SOCK_OP_RECVMSG: {
        int is_msg = (req->op == SOCK_OP_RECVMSG);
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        if (s->type == SOCK_DGRAM) {
            uint32_t dmax = is_msg ? SOCK_MSG_DATA_MAX : SOCK_DGRAM_DATA_MAX;
            uint32_t want = req->len > dmax ? dmax : req->len;
            if (s->dg_len > 0u) {
                dgram_pkt_t *pkt = &s->dg_q[s->dg_head];
                uint32_t copy = pkt->len < want ? pkt->len : want;
                rep.err = 0;
                rep.len = copy;
                if (is_msg) {
                    rep.u.msg.rights_count = pkt->rights_count;
                    if (pkt->rights_count > 0u) {
                        memcpy(rep.u.msg.rights, pkt->rights,
                               pkt->rights_count * sizeof(struct __k_fd_entry));
                    }
                    if (copy > 0u) {
                        memcpy(rep.u.msg.data, pkt->data, copy);
                    }
                } else {
                    strlcpy(rep.u.dgram.path, pkt->sender,
                            sizeof(rep.u.dgram.path));
                    if (copy > 0u) {
                        memcpy(rep.u.dgram.data, pkt->data, copy);
                    }
                }
                if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0 &&
                    !(req->flags & (uint32_t)MSG_PEEK)) {
                    if (!is_msg && pkt->rights_count > 0u) {
                        release_rights_entries(pkt->rights, pkt->rights_count);
                    }
                    s->dg_head = (s->dg_head + 1u) % DGRAM_Q_MAX;
                    s->dg_len--;
                    service_pollers(0, 0);
                }
                return;
            }
            if (s->shut_rd) {
                rep.err = 0;
                rep.len = 0;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            if (req->flags & ((uint32_t)O_NONBLOCK | (uint32_t)MSG_DONTWAIT)) {
                rep.err = -EAGAIN;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                return;
            }
            for (int i = 0; i < RWAIT_MAX; i++) {
                if (!g_rwait[i].used) {
                    g_rwait[i].used = 1;
                    g_rwait[i].is_msg = (uint8_t)is_msg;
                    g_rwait[i].msgid = msgid;
                    g_rwait[i].sock_idx = req->handle;
                    g_rwait[i].want_len = want;
                    g_rwait[i].flags = req->flags;
                    return; /* deferred ReplyMsg */
                }
            }
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }

        /* SOCK_STREAM recv / recvmsg */
        if (s->state != ST_CONNECTED && !s->peer_closed) {
            rep.err = -ENOTCONN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        uint32_t smax = is_msg ? SOCK_MSG_DATA_MAX : SOCK_DATA_MAX;
        uint32_t want = req->len > smax ? smax : req->len;
        sock_obj_t *peer = get_sock(s->peer_idx);
        int peer_eof = (!peer || s->peer_closed || peer->shut_wr || s->shut_rd);
        if (want == 0u || s->rx_len > 0u || s->rights_len > 0u || peer_eof) {
            int pop_rights = 0;
            uint32_t n =
                stream_recv_prepare(s, want, is_msg, &rep, &pop_rights);
            rep.err = 0;
            rep.len = n;
            if (ReplyMsg((int)msgid, &rep, sizeof(rep)) >= 0) {
                stream_recv_commit(s, n, req->flags, pop_rights);
                wake_sock_waiters(req->handle);
            }
            return;
        }
        if (req->flags & ((uint32_t)O_NONBLOCK | (uint32_t)MSG_DONTWAIT)) {
            rep.err = -EAGAIN;
            ReplyMsg((int)msgid, &rep, sizeof(rep));
            return;
        }
        for (int i = 0; i < RWAIT_MAX; i++) {
            if (!g_rwait[i].used) {
                g_rwait[i].used = 1;
                g_rwait[i].is_msg = (uint8_t)is_msg;
                g_rwait[i].msgid = msgid;
                g_rwait[i].sock_idx = req->handle;
                g_rwait[i].want_len = want;
                g_rwait[i].flags = req->flags;
                return; /* deferred ReplyMsg */
            }
        }
        rep.err = -EAGAIN;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_SHUTDOWN: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (s->state != ST_CONNECTED && !s->peer_closed) {
            rep.err = -ENOTCONN;
        } else {
            int how = (int)req->arg1;
            if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR) {
                rep.err = -EINVAL;
            } else {
                if (how == SHUT_RD || how == SHUT_RDWR) {
                    s->shut_rd = 1;
                }
                if (how == SHUT_WR || how == SHUT_RDWR) {
                    s->shut_wr = 1;
                }
                rep.err = 0;
                ReplyMsg((int)msgid, &rep, sizeof(rep));
                wake_sock_waiters(req->handle);
                if (s->peer_idx) {
                    wake_sock_waiters(s->peer_idx);
                }
                return;
            }
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_GETNAME: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (req->arg1 != 0u) {
            /* getpeername */
            sock_obj_t *peer = get_sock(s->peer_idx);
            if (!peer || s->state != ST_CONNECTED) {
                rep.err = -ENOTCONN;
            } else {
                strlcpy(rep.u.path, peer->bound_path, sizeof(rep.u.path));
                rep.err = 0;
            }
        } else {
            /* getsockname */
            strlcpy(rep.u.path, s->bound_path, sizeof(rep.u.path));
            rep.err = 0;
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_GETOPT: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (req->arg1 != (uint32_t)SOL_SOCKET) {
            rep.err = -ENOPROTOOPT;
        } else {
            rep.err = 0;
            switch ((int)req->arg2) {
            case SO_TYPE:
                rep.val = s->type;
                break;
            case SO_ERROR:
                rep.val = s->so_error;
                s->so_error = 0;
                break;
            case SO_REUSEADDR:
                rep.val = (int32_t)s->so_reuseaddr;
                break;
            case SO_ACCEPTCONN:
                rep.val = (s->state == ST_LISTENING) ? 1 : 0;
                break;
            case SO_SNDBUF:
            case SO_RCVBUF:
                rep.val = (int32_t)SOCK_BUF_SIZE;
                break;
            default:
                rep.err = -ENOPROTOOPT;
                break;
            }
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_SETOPT: {
        sock_obj_t *s = get_sock(req->handle);
        if (!s) {
            rep.err = -EBADF;
        } else if (req->arg1 != (uint32_t)SOL_SOCKET) {
            rep.err = -ENOPROTOOPT;
        } else {
            rep.err = 0;
            switch ((int)req->arg2) {
            case SO_REUSEADDR:
                s->so_reuseaddr = (req->len != 0u) ? 1u : 0u;
                break;
            case SO_SNDBUF:
            case SO_RCVBUF:
            case SO_KEEPALIVE:
                break;
            default:
                rep.err = -ENOPROTOOPT;
                break;
            }
        }
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    case SOCK_OP_STATS: {
        uint32_t active_socks = 0;
        uint32_t waiters = 0;
        for (int i = 1; i < SOCK_MAX; i++) {
            if (g_socks[i].used) {
                active_socks++;
            }
        }
        for (int i = 0; i < RWAIT_MAX; i++) {
            if (g_rwait[i].used) {
                waiters++;
            }
        }
        for (int i = 0; i < WWAIT_MAX; i++) {
            if (g_wwait[i].used) {
                waiters++;
            }
        }
        for (int i = 0; i < AWAIT_MAX; i++) {
            if (g_await[i].used) {
                waiters++;
            }
        }
        for (int i = 0; i < PWAIT_MAX; i++) {
            if (g_pwait[i].used) {
                waiters++;
            }
        }
        rep.err = 0;
        rep.len = active_socks;
        rep.val = (int32_t)waiters;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
    default:
        rep.err = -ENOSYS;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }
}

static void handle_poll_req(uint32_t msgid, const sock_poll_req_t *req) {
    sock_poll_rep_t rep = {0};
    if (req->nfds > SOCK_POLL_MAX) {
        rep.err = -EINVAL;
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }

    sock_poll_item_t items[SOCK_POLL_MAX];
    uint32_t ready = 0;
    for (uint32_t k = 0; k < req->nfds; k++) {
        items[k] = req->items[k];
        if (eval_poll_item(&items[k])) {
            ready++;
        }
    }

    if (ready > 0u || req->timeout_ns == 0) {
        rep.err = 0;
        rep.ready = ready;
        memcpy(rep.items, items, req->nfds * sizeof(sock_poll_item_t));
        ReplyMsg((int)msgid, &rep, sizeof(rep));
        return;
    }

    for (int i = 0; i < PWAIT_MAX; i++) {
        if (!g_pwait[i].used) {
            g_pwait[i].used = 1;
            g_pwait[i].msgid = msgid;
            g_pwait[i].nfds = req->nfds;
            memcpy(g_pwait[i].items, items,
                   req->nfds * sizeof(sock_poll_item_t));
            if (req->timeout_ns > 0) {
                g_pwait[i].has_deadline = 1;
                g_pwait[i].deadline_ns =
                    now_mono_ns() + (uint64_t)req->timeout_ns;
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
    long ch = CreateChannel();
    if (ch < 0) {
        printf("[FAIL] sockserv CreateChannel failed (%ld)\n", ch);
        return 1;
    }

    long ns = -1;
    for (int i = 0; i < 200; i++) {
        ns = OpenConnection(1, 1);
        if (ns >= 0) {
            break;
        }
        struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
    }
    if (ns < 0) {
        printf("[FAIL] sockserv failed to connect to sysproc\n");
        return 1;
    }

    name_msg_t reg = {0};
    reg.type = NAME_MSG_ATTACH;
    strlcpy(reg.path, "/dev/sock", sizeof(reg.path));
    reg.chanid = (uint32_t)ch;
    name_reply_t nrep = {0};
    if (SendMsg((int)ns, &reg, sizeof(reg), &nrep, sizeof(nrep)) < 0 ||
        nrep.err != 0) {
        printf("[FAIL] sockserv failed to register /dev/sock\n");
        CloseConnection((int)ns);
        return 1;
    }
    CloseConnection((int)ns);

    g_self_pid = (uint32_t)getpid();
    g_self_conn = (int)OpenConnection(g_self_pid, (uint32_t)ch);
    (void)ThreadSpawn(ticker_thread, NULL, 90);

    printf("[OK] sockserv registered /dev/sock (chan=%ld)\n", ch);

    union {
        uint32_t op;
        sock_req_t req;
        sock_poll_req_t preq;
        ipc_pulse_t pulse;
        uint8_t raw[512];
    } msg;
    ipc_recv_info_t info = {0};

    for (;;) {
        memset(&msg, 0, sizeof(msg));
        long msgid = ReceiveMsg((int)ch, &msg, sizeof(msg), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            if (msg.pulse.code == PULSE_CODE_FD_CLOSE) {
                handle_close_handle((uint32_t)msg.pulse.value);
            } else if (msg.pulse.code == PULSE_CODE_SOCK_TICK) {
                service_pollers(now_mono_ns(), 1);
            }
            continue;
        }
        if (msg.op == SOCK_OP_POLL) {
            handle_poll_req((uint32_t)msgid, &msg.preq);
        } else {
            handle_sock_req((uint32_t)msgid, &msg.req);
        }
    }
}
