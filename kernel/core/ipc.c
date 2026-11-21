/**
 * @file     ipc.c
 * @brief    Synchronous message passing, priority inheritance, and pulses
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/ipc.h>
#include <kernel/uaccess.h>
#include <kernel/kheap.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include "channel.h"

/* need:: COMP_IPC_0001 */

/* In-flight messages: delivered to a server, awaiting ReplyMsg. */
static chan_msg_t *g_inflight_head;
static uint32_t g_msgid_next = 1;

static void inflight_push(chan_msg_t *m) {
    m->next = g_inflight_head;
    g_inflight_head = m;
}

static chan_msg_t *inflight_find(uint32_t msgid) {
    for (chan_msg_t *m = g_inflight_head; m; m = m->next) {
        if (m->msgid == msgid) {
            return m;
        }
    }
    return NULL;
}

static void inflight_remove(chan_msg_t *m) {
    chan_msg_t **pp = &g_inflight_head;
    while (*pp) {
        if (*pp == m) {
            *pp = m->next;
            m->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

/* Resolve a connection id to the channel it targets. */
static channel_t *resolve_conn(int connid) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p || connid < 0 || (uint32_t)connid >= p->conn_count) {
        return NULL;
    }
    if (!p->conn_table[connid].pid) {
        return NULL;
    }
    channel_t *ch = channel_get((int)p->conn_table[connid].chanid);
    if (!ch || ch->owner_pid != p->conn_table[connid].pid) {
        return NULL;
    }
    return ch;
}

long ipc_conn_attach(uint32_t target_pid, uint32_t chanid) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p) {
        return -EPERM;
    }
    channel_t *ch = channel_get((int)chanid);
    if (!ch || ch->owner_pid != target_pid) {
        return -ESRCH;
    }
    if (!proc_find(target_pid)) {
        return -ESRCH;
    }
    for (uint32_t i = 0; i < p->conn_count; i++) {
        if (!p->conn_table[i].pid) {
            p->conn_table[i].pid = target_pid;
            p->conn_table[i].chanid = chanid;
            p->conn_table[i].flags = 0;
            ch->refcount++;
            return (long)i;
        }
    }
    return -EMFILE;
}

long ipc_conn_detach(int connid) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p || connid < 0 || (uint32_t)connid >= p->conn_count ||
        !p->conn_table[connid].pid) {
        return -EBADF;
    }
    channel_t *ch = channel_get((int)p->conn_table[connid].chanid);
    if (ch) {
        ch->refcount--;
    }
    p->conn_table[connid].pid = 0;
    p->conn_table[connid].chanid = 0;
    return 0;
}

/* The current (server) thread accepts a queued message or pulse: deliver the
 * payload into the server's saved receive buffer, assign a msgid, mark the
 * client REPLY_BLOCKED, apply priority inheritance, and return the msgid.
 * Pulses (sender == NULL) get msgid 0 and are freed immediately. */
static long complete_receive(tcb_t *srv, chan_msg_t *m) {
    tcb_t *client = m->sender;
    long msgid = 0;

    if (client) {
        m->msgid = g_msgid_next++;
        m->receiver = srv;
        msgid = (long)m->msgid;
        client->state = THREAD_REPLY_BLOCKED; /* stays blocked until Reply */
    }

    uint64_t n = m->len;
    if (n > srv->ipc_buflen) {
        n = srv->ipc_buflen;
    }
    if (n) {
        long r =
            copy_to_user_pml4(srv->proc->pml4_phys, srv->ipc_buf, m->data, n);
        if (r < 0) {
            n = 0; /* undeliverable payload: receiver sees an empty message */
        }
    }

    if (srv->ipc_info) {
        ipc_recv_info_t ii = {
            .msgid = (uint32_t)msgid,
            .pid = client ? client->pid : 0,
            .len = (uint32_t)n,
            .reserved = 0,
        };
        copy_to_user_pml4(srv->proc->pml4_phys, srv->ipc_info, &ii, sizeof(ii));
    }

    if (client) {
        /* Priority inheritance: while processing, the server runs at the
         * client's effective priority. */
        if (client->effective_priority > srv->effective_priority) {
            srv->effective_priority = client->effective_priority;
        }
        inflight_push(m);
    } else if (m->is_static) {
        m->in_use = 0; /* pre-allocated IRQ pulse node: recycle without kfree */
    } else {
        kfree(m); /* heap pulse: no reply, nothing to track */
    }

    srv->ipc_result = msgid;
    sched_unblock(srv);
    return msgid;
}

long ipc_msg_send(int connid, uint64_t smsg, uint64_t sbytes, uint64_t rmsg,
                  uint64_t rbytes) {
    tcb_t *t = sched_current();
    channel_t *ch = resolve_conn(connid);
    if (!ch) {
        return -ESRCH;
    }
    if (sbytes > IPC_MSG_MAX) {
        return -EINVAL;
    }

    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->sender = t;
    m->sender_pml4 = t->proc->pml4_phys;
    m->reply_dst = rmsg;
    m->reply_max = rbytes;
    m->len = (uint32_t)sbytes;
    long r = copy_from_user(m->data, smsg, sbytes);
    if (r < 0) {
        kfree(m);
        return r;
    }

    t->state = THREAD_SEND_BLOCKED;
    t->blocked_on_chan = ch;
    channel_msg_enqueue(ch, m);

    /* Wake a blocked receiver if any; it dequeues and delivers from its own
     * context (spurious wakeups are tolerated by its retry loop). */
    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        w->blocked_on_chan = NULL;
        w->ipc_result = 1; /* "message available": resume the receive loop */
        sched_unblock(w);
    }

    /* Dispatch away; we resume inside this frame once the server replies. */
    schedule();
    return t->ipc_result;
}

long ipc_msg_receive(int chanid, uint64_t buf, uint64_t bytes, uint64_t info) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p) {
        return -EPERM;
    }
    channel_t *ch = channel_get(chanid);
    if (!ch || ch->owner_pid != p->pid) {
        return -ESRCH;
    }

    for (;;) {
        /* Pulses first (small, async, no reply expected), then messages. */
        chan_msg_t *m = channel_pulse_dequeue(ch);
        if (!m) {
            m = channel_msg_dequeue(ch);
        }
        if (m) {
            t->ipc_buf = buf;
            t->ipc_buflen = bytes;
            t->ipc_info = info;
            return complete_receive(t, m);
        }

        /* Nothing pending: block until a sender or pulse arrives. */
        t->ipc_buf = buf;
        t->ipc_buflen = bytes;
        t->ipc_info = info;
        t->state = THREAD_RECEIVE_BLOCKED;
        t->blocked_on_chan = ch;
        channel_recv_enqueue(ch, t);
        schedule();
        /* Woken by a sender (or spuriously): retry the dequeue. */
    }
}

long ipc_msg_reply(int msgid, uint64_t rmsg, uint64_t rbytes) {
    tcb_t *srv = sched_current();
    if (msgid <= 0) {
        return -EINVAL;
    }
    chan_msg_t *m = inflight_find((uint32_t)msgid);
    if (!m) {
        return -ESRCH;
    }
    if (m->receiver != srv) {
        return -EPERM;
    }

    tcb_t *client = m->sender;
    if (rbytes > m->reply_max) {
        rbytes = m->reply_max;
    }
    long err = 0;
    if (rbytes) {
        /* Stage the reply through the node, then copy it into the client's
         * address space (the client is not the current address space). */
        long r = copy_from_user(m->data, rmsg, rbytes);
        if (r < 0) {
            err = r;
        } else {
            r = copy_to_user_pml4(m->sender_pml4, m->reply_dst, m->data,
                                  rbytes);
            if (r < 0) {
                err = r;
            }
        }
    }

    srv->effective_priority = srv->base_priority; /* drop inheritance */

    if (client) {
        long final_len = (rbytes == 0 && m->reply_written > 0)
                             ? (long)m->reply_written
                             : (long)rbytes;
        client->ipc_result = err < 0 ? err : final_len;
        client->blocked_on_chan = NULL;
        /* sched_unblock marks the client READY and preempts us if it
         * outranks us. It must not be pre-marked READY here. */
        sched_unblock(client);
    }

    inflight_remove(m);
    kfree(m);
    return 0;
}

long ipc_msg_readv(int msgid, uint64_t offset, uint64_t iov_va,
                   uint64_t iovcnt) {
    tcb_t *srv = sched_current();
    if (msgid <= 0 || iovcnt > IPC_IOV_MAX) {
        return -EINVAL;
    }
    chan_msg_t *m = inflight_find((uint32_t)msgid);
    if (!m) {
        return -ESRCH;
    }
    if (m->receiver != srv) {
        return -EPERM;
    }
    if (iovcnt == 0 || offset >= m->len) {
        return 0;
    }
    ipc_iov_t iov[IPC_IOV_MAX];
    if (copy_from_user(iov, iov_va, iovcnt * sizeof(ipc_iov_t)) < 0) {
        return -EFAULT;
    }
    uint64_t cur = offset;
    uint64_t total = 0;
    for (uint64_t i = 0; i < iovcnt && cur < m->len; i++) {
        size_t chunk = iov[i].len;
        if (chunk == 0) {
            continue;
        }
        if (chunk > (size_t)(m->len - cur)) {
            chunk = (size_t)(m->len - cur);
        }
        if (copy_to_user((uint64_t)(uintptr_t)iov[i].base, m->data + cur,
                         chunk) < 0) {
            return -EFAULT;
        }
        cur += chunk;
        total += chunk;
    }
    return (long)total;
}

long ipc_msg_writev(int msgid, uint64_t offset, uint64_t iov_va,
                    uint64_t iovcnt) {
    tcb_t *srv = sched_current();
    if (msgid <= 0 || iovcnt > IPC_IOV_MAX) {
        return -EINVAL;
    }
    chan_msg_t *m = inflight_find((uint32_t)msgid);
    if (!m) {
        return -ESRCH;
    }
    if (m->receiver != srv) {
        return -EPERM;
    }
    if (iovcnt == 0 || offset >= m->reply_max) {
        return 0;
    }
    ipc_iov_t iov[IPC_IOV_MAX];
    if (copy_from_user(iov, iov_va, iovcnt * sizeof(ipc_iov_t)) < 0) {
        return -EFAULT;
    }
    uint8_t stage[256];
    uint64_t cur = offset;
    uint64_t total = 0;
    for (uint64_t i = 0; i < iovcnt && cur < m->reply_max; i++) {
        uint64_t seg_off = 0;
        uint64_t seg_len = iov[i].len;
        if (seg_len > m->reply_max - cur) {
            seg_len = m->reply_max - cur;
        }
        while (seg_off < seg_len) {
            size_t n = (size_t)(seg_len - seg_off);
            if (n > sizeof(stage)) {
                n = sizeof(stage);
            }
            if (copy_from_user(
                    stage, (uint64_t)(uintptr_t)iov[i].base + seg_off, n) < 0) {
                return -EFAULT;
            }
            if (copy_to_user_pml4(m->sender_pml4, m->reply_dst + cur, stage,
                                  n) < 0) {
                return -EFAULT;
            }
            seg_off += n;
            cur += n;
            total += n;
        }
    }
    if (cur > m->reply_written) {
        m->reply_written = cur;
    }
    return (long)total;
}

void ipc_deliver_irq_pulse(int chanid, chan_msg_t *node, uint32_t code,
                           uint32_t value) {
    channel_t *ch = channel_get(chanid);
    if (!ch || !node || node->in_use) {
        return;
    }
    node->next = NULL;
    node->sender = NULL;
    node->receiver = NULL;
    node->msgid = 0;
    node->is_static = 1;
    node->in_use = 1;
    node->len = sizeof(ipc_pulse_t);
    ipc_pulse_t p = {.code = code, .value = value};
    memcpy(node->data, &p, sizeof(p));

    channel_pulse_enqueue(ch, node);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock(w);
    }
}

long ipc_msg_send_pulse(int connid, uint32_t code, uint32_t value) {
    channel_t *ch = resolve_conn(connid);
    if (!ch) {
        return -ESRCH;
    }

    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->len = sizeof(ipc_pulse_t);
    ipc_pulse_t p = {.code = code, .value = value};
    memcpy(m->data, &p, sizeof(p));

    channel_pulse_enqueue(ch, m);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock(w);
    }
    return 0;
}

long ipc_send_pulse_kernel(uint32_t target_pid, uint32_t chanid, uint32_t code,
                           uint32_t value) {
    channel_t *ch = channel_get((int)chanid);
    if (!ch || ch->owner_pid != target_pid) {
        return -ESRCH;
    }
    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->len = sizeof(ipc_pulse_t);
    ipc_pulse_t p = {.code = code, .value = value};
    memcpy(m->data, &p, sizeof(p));

    channel_pulse_enqueue(ch, m);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_ready(w);
    }
    return 0;
}

/* Wake a thread out of any IPC block, handing back `result` (used by
 * signal delivery: blocked threads resume with -EINTR). */
void ipc_unblock_thread(tcb_t *t, long result) {
    if (!t->blocked_on_chan) {
        return;
    }
    channel_t *ch = (channel_t *)t->blocked_on_chan;
    t->blocked_on_chan = NULL;

    if (t->state == THREAD_RECEIVE_BLOCKED) {
        channel_recv_remove(ch, t);
    } else if (t->state == THREAD_SEND_BLOCKED) {
        /* Drop our queued message node. */
        chan_msg_t *m = ch->msg_head;
        while (m) {
            chan_msg_t *next = m->next;
            if (m->sender == t) {
                channel_msg_remove(ch, m);
                kfree(m);
                break;
            }
            m = next;
        }
    } else if (t->state == THREAD_REPLY_BLOCKED) {
        /* Drop our in-flight node; the server's later reply gets -ESRCH. */
        chan_msg_t **ip = &g_inflight_head;
        while (*ip) {
            chan_msg_t *m = *ip;
            if (m->sender == t) {
                *ip = m->next;
                kfree(m);
                break;
            }
            ip = &m->next;
        }
    }

    t->ipc_result = result;
    sched_unblock(t);
}

void ipc_detach_thread(tcb_t *t) {
    /* Receive-queue membership. */
    if (t->blocked_on_chan) {
        channel_t *ch = (channel_t *)t->blocked_on_chan;
        channel_recv_remove(ch, t);
        t->blocked_on_chan = NULL;
    }
    /* Queued (send-blocked) messages. */
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channel_t *ch = channel_get(i);
        if (!ch) {
            continue;
        }
        chan_msg_t *m = ch->msg_head;
        while (m) {
            chan_msg_t *next = m->next;
            if (m->sender == t) {
                channel_msg_remove(ch, m);
                kfree(m);
            }
            m = next;
        }
    }
    /* In-flight messages the server has not replied to yet: the server will
     * get -ESRCH from ReplyMsg. */
    chan_msg_t **ip = &g_inflight_head;
    while (*ip) {
        chan_msg_t *m = *ip;
        if (m->sender == t) {
            *ip = m->next;
            kfree(m);
        } else {
            ip = &m->next;
        }
    }
}
