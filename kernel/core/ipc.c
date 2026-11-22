/**
 * @file     ipc.c
 * @brief    SMP-safe synchronous message passing, priority inheritance, pulses
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <kernel/ipc.h>
#include <kernel/kheap.h>
#include <kernel/printk.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>
#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <sys/errno.h>
#include "channel.h"

/* need:: COMP_IPC_0001 COMP_PROC_0008 */

/* In-flight messages: delivered to a server, awaiting ReplyMsg. */
static chan_msg_t *g_inflight_head;
static uint32_t g_msgid_next = 1;

static void inflight_push_locked(chan_msg_t *m) {
    m->next = g_inflight_head;
    g_inflight_head = m;
}

static chan_msg_t *inflight_find_locked(uint32_t msgid) {
    for (chan_msg_t *m = g_inflight_head; m; m = m->next) {
        if (m->msgid == msgid) {
            return m;
        }
    }
    return NULL;
}

static void inflight_remove_locked(chan_msg_t *m) {
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

/* Resolve a connection id to the channel it targets (caller holds g_ipc_lock
 * or acquires g_proc_lock -> g_ipc_lock). */
static channel_t *resolve_conn_locked(pcb_t *p, int connid) {
    if (!p || connid < 0 || (uint32_t)connid >= p->conn_count) {
        return NULL;
    }
    if (!p->conn_table[connid].pid) {
        return NULL;
    }
    channel_t *ch = channel_get_locked((int)p->conn_table[connid].chanid);
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
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (!proc_find_locked(target_pid)) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -ESRCH;
    }
    spin_lock(&g_ipc_lock);
    channel_t *ch = channel_get_locked((int)chanid);
    if (!ch || ch->owner_pid != target_pid) {
        spin_unlock(&g_ipc_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -ESRCH;
    }
    for (uint32_t i = 0; i < p->conn_count; i++) {
        if (!p->conn_table[i].pid) {
            p->conn_table[i].pid = target_pid;
            p->conn_table[i].chanid = chanid;
            p->conn_table[i].flags = 0;
            ch->refcount++;
            spin_unlock(&g_ipc_lock);
            spin_unlock_irqrestore(&g_proc_lock, pf);
            return (long)i;
        }
    }
    spin_unlock(&g_ipc_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return -EMFILE;
}

long ipc_conn_detach(int connid) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p || connid < 0 || (uint32_t)connid >= p->conn_count) {
        return -EBADF;
    }
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    if (!p->conn_table[connid].pid) {
        spin_unlock_irqrestore(&g_proc_lock, pf);
        return -EBADF;
    }
    spin_lock(&g_ipc_lock);
    channel_t *ch = channel_get_locked((int)p->conn_table[connid].chanid);
    if (ch && ch->refcount > 0) {
        ch->refcount--;
    }
    p->conn_table[connid].pid = 0;
    p->conn_table[connid].chanid = 0;
    spin_unlock(&g_ipc_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);
    return 0;
}

long ipc_msg_send(int connid, uint64_t smsg, uint64_t sbytes, uint64_t rmsg,
                  uint64_t rbytes) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p) {
        return -EPERM;
    }
    if (sbytes > IPC_MSG_LARGE_MAX) {
        return -EINVAL;
    }

    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->sender = t;
    m->sender_pml4 = p->pml4_phys;
    m->send_src = smsg;
    m->send_len = sbytes;
    m->reply_dst = rmsg;
    m->reply_max = rbytes;
    uint32_t stage_len =
        (sbytes > IPC_MSG_MAX) ? IPC_MSG_MAX : (uint32_t)sbytes;
    m->len = stage_len;
    long r = copy_from_user(m->data, smsg, stage_len);
    if (r < 0) {
        kfree(m);
        return r;
    }

    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    spin_lock(&g_ipc_lock);
    channel_t *ch = resolve_conn_locked(p, connid);
    if (!ch) {
        spin_unlock(&g_ipc_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        kfree(m);
        return -ESRCH;
    }

    channel_msg_enqueue(ch, m);
    tcb_t *w = channel_recv_dequeue(ch);

    /* Hand-over-hand onto g_sched_lock before releasing g_ipc_lock so no
     * remote server CPU can reply and wake `t` before `t` switches off-CPU. */
    spin_lock(&g_sched_lock);
    t->state = THREAD_SEND_BLOCKED;
    t->blocked_on_chan = ch;
    if (w) {
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock_locked(w);
    }
    spin_unlock(&g_ipc_lock);
    spin_unlock(&g_proc_lock);
    schedule_with_lock(pf);
    return t->ipc_result;
}

long ipc_msg_receive(int chanid, uint64_t buf, uint64_t bytes, uint64_t info) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p) {
        return -EPERM;
    }

    for (;;) {
        uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
        channel_t *ch = channel_get_locked(chanid);
        if (!ch || ch->owner_pid != p->pid) {
            spin_unlock_irqrestore(&g_ipc_lock, fl);
            return -ESRCH;
        }

        /* Pulses first (small, async, no reply expected), then messages. */
        chan_msg_t *m = channel_pulse_dequeue(ch);
        if (!m) {
            m = channel_msg_dequeue(ch);
        }
        if (m) {
            tcb_t *client = m->sender;
            long msgid = 0;
            uint32_t sender_pid = 0;
            uint64_t copy_len = m->len;
            if (copy_len > bytes) {
                copy_len = bytes;
            }
            uint8_t local_buf[IPC_MSG_MAX];
            if (copy_len > 0) {
                memcpy(local_buf, m->data, copy_len);
            }

            if (client) {
                m->msgid = g_msgid_next++;
                m->receiver = t;
                msgid = (long)m->msgid;
                sender_pid = client->pid;
                spin_lock(&g_sched_lock);
                client->state = THREAD_REPLY_BLOCKED;
                if (client->effective_priority > t->effective_priority) {
                    t->effective_priority = client->effective_priority;
                }
                spin_unlock(&g_sched_lock);
                inflight_push_locked(m);
            } else if (m->is_static) {
                __atomic_store_n(&m->in_use, 0u, __ATOMIC_RELEASE);
            }
            spin_unlock_irqrestore(&g_ipc_lock, fl);

            if (!client && !m->is_static) {
                kfree(m);
            }

            if (copy_len > 0) {
                long cr = copy_to_user(buf, local_buf, copy_len);
                if (cr < 0) {
                    copy_len = 0;
                }
            }
            if (info) {
                ipc_recv_info_t ii = {
                    .msgid = (uint32_t)msgid,
                    .pid = sender_pid,
                    .len = (uint32_t)copy_len,
                    .reserved = 0,
                };
                (void)copy_to_user(info, &ii, sizeof(ii));
            }
            return msgid;
        }

        /* Nothing pending: transition to RECEIVE_BLOCKED under g_sched_lock
         * before releasing g_ipc_lock. */
        spin_lock(&g_sched_lock);
        if (__atomic_load_n(&t->pending_signals, __ATOMIC_ACQUIRE) &
            ~t->blocked_signals) {
            spin_unlock(&g_sched_lock);
            spin_unlock_irqrestore(&g_ipc_lock, fl);
            return -EINTR;
        }
        t->ipc_buf = buf;
        t->ipc_buflen = bytes;
        t->ipc_info = info;
        t->state = THREAD_RECEIVE_BLOCKED;
        t->blocked_on_chan = ch;
        t->ipc_result = 0;
        channel_recv_enqueue(ch, t);
        spin_unlock(&g_ipc_lock);
        schedule_with_lock(fl);

        if (t->ipc_result < 0) {
            return t->ipc_result;
        }
    }
}

long ipc_msg_reply(int msgid, uint64_t rmsg, uint64_t rbytes) {
    tcb_t *srv = sched_current();
    if (msgid <= 0) {
        return -EINVAL;
    }
    if (rbytes > IPC_MSG_MAX) {
        return -EINVAL;
    }

    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    chan_msg_t *m = inflight_find_locked((uint32_t)msgid);
    if (!m) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -ESRCH;
    }
    if (m->receiver != srv) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -EPERM;
    }
    inflight_remove_locked(m);
    spin_unlock_irqrestore(&g_ipc_lock, fl);

    tcb_t *client = m->sender;
    if (rbytes > m->reply_max) {
        rbytes = m->reply_max;
    }
    long err = 0;
    if (rbytes) {
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

    int need_preempt = 0;
    uint64_t sf = spin_lock_irqsave(&g_sched_lock);
    srv->effective_priority = srv->base_priority; /* drop inheritance */
    if (client) {
        long final_len = (rbytes == 0 && m->reply_written > 0)
                             ? (long)m->reply_written
                             : (long)rbytes;
        client->ipc_result = err < 0 ? err : final_len;
        client->blocked_on_chan = NULL;
        sched_unblock_locked(client);
        if (client->effective_priority > srv->effective_priority) {
            need_preempt = 1;
        }
    }
    spin_unlock_irqrestore(&g_sched_lock, sf);

    kfree(m);
    if (need_preempt) {
        schedule();
    }
    return 0;
}

static void release_msg_op(chan_msg_t *m) {
    int do_free = 0;
    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    if (m->in_op > 0u) {
        m->in_op--;
    }
    if (m->in_op == 0u && m->detached) {
        do_free = 1;
    }
    spin_unlock_irqrestore(&g_ipc_lock, fl);
    if (do_free) {
        kfree(m);
    }
}

long ipc_msg_readv(int msgid, uint64_t offset, uint64_t iov_va,
                   uint64_t iovcnt) {
    tcb_t *srv = sched_current();
    if (msgid <= 0 || iovcnt > IPC_IOV_MAX) {
        return -EINVAL;
    }
    ipc_iov_t iov[IPC_IOV_MAX];
    if (iovcnt > 0 &&
        copy_from_user(iov, iov_va, iovcnt * sizeof(ipc_iov_t)) < 0) {
        return -EFAULT;
    }

    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    chan_msg_t *m = inflight_find_locked((uint32_t)msgid);
    if (!m) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -ESRCH;
    }
    if (m->receiver != srv) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -EPERM;
    }
    m->in_op++;
    spin_unlock_irqrestore(&g_ipc_lock, fl);

    uint64_t avail_len = m->send_len ? m->send_len : (uint64_t)m->len;
    if (iovcnt == 0 || offset >= avail_len) {
        release_msg_op(m);
        return 0;
    }

    uint8_t stage[256];
    uint64_t cur = offset;
    uint64_t total = 0;
    long rc = 0;
    for (uint64_t i = 0; i < iovcnt && cur < avail_len; i++) {
        uint64_t seg_off = 0;
        uint64_t seg_len = iov[i].len;
        if (seg_len > avail_len - cur) {
            seg_len = avail_len - cur;
        }
        while (seg_off < seg_len) {
            size_t n = (size_t)(seg_len - seg_off);
            if (cur < m->len) {
                if (n > (size_t)(m->len - cur)) {
                    n = (size_t)(m->len - cur);
                }
                if (copy_to_user((uint64_t)(uintptr_t)iov[i].base + seg_off,
                                 m->data + cur, n) < 0) {
                    rc = -EFAULT;
                    break;
                }
            } else {
                if (n > sizeof(stage)) {
                    n = sizeof(stage);
                }
                if (copy_from_user_pml4(m->sender_pml4, stage,
                                        m->send_src + cur, n) < 0) {
                    rc = -EFAULT;
                    break;
                }
                if (copy_to_user((uint64_t)(uintptr_t)iov[i].base + seg_off,
                                 stage, n) < 0) {
                    rc = -EFAULT;
                    break;
                }
            }
            seg_off += n;
            cur += n;
            total += n;
        }
        if (rc < 0) {
            break;
        }
    }
    release_msg_op(m);
    return rc < 0 ? rc : (long)total;
}

long ipc_msg_writev(int msgid, uint64_t offset, uint64_t iov_va,
                    uint64_t iovcnt) {
    tcb_t *srv = sched_current();
    if (msgid <= 0 || iovcnt > IPC_IOV_MAX) {
        return -EINVAL;
    }
    ipc_iov_t iov[IPC_IOV_MAX];
    if (iovcnt > 0 &&
        copy_from_user(iov, iov_va, iovcnt * sizeof(ipc_iov_t)) < 0) {
        return -EFAULT;
    }

    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    chan_msg_t *m = inflight_find_locked((uint32_t)msgid);
    if (!m) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -ESRCH;
    }
    if (m->receiver != srv) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return -EPERM;
    }
    m->in_op++;
    spin_unlock_irqrestore(&g_ipc_lock, fl);

    if (iovcnt == 0 || offset >= m->reply_max) {
        release_msg_op(m);
        return 0;
    }

    uint8_t stage[256];
    uint64_t cur = offset;
    uint64_t total = 0;
    long rc = 0;
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
                rc = -EFAULT;
                break;
            }
            if (copy_to_user_pml4(m->sender_pml4, m->reply_dst + cur, stage,
                                  n) < 0) {
                rc = -EFAULT;
                break;
            }
            seg_off += n;
            cur += n;
            total += n;
        }
        if (rc < 0) {
            break;
        }
    }
    if (rc == 0 && cur > m->reply_written) {
        m->reply_written = cur;
    }
    release_msg_op(m);
    return rc < 0 ? rc : (long)total;
}

void ipc_deliver_irq_pulse(int chanid, chan_msg_t *node, uint32_t code,
                           uint32_t value) {
    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    channel_t *ch = channel_get_locked(chanid);
    if (!ch || !node || __atomic_load_n(&node->in_use, __ATOMIC_ACQUIRE)) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return;
    }
    node->next = NULL;
    node->sender = NULL;
    node->receiver = NULL;
    node->msgid = 0;
    node->is_static = 1;
    __atomic_store_n(&node->in_use, 1u, __ATOMIC_RELEASE);
    node->len = sizeof(ipc_pulse_t);
    ipc_pulse_t p = {.code = code, .value = value};
    memcpy(node->data, &p, sizeof(p));

    channel_pulse_enqueue(ch, node);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        spin_lock(&g_sched_lock);
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock_locked(w);
        spin_unlock(&g_sched_lock);
    }
    spin_unlock_irqrestore(&g_ipc_lock, fl);
}

long ipc_msg_send_pulse(int connid, uint32_t code, uint32_t value) {
    tcb_t *t = sched_current();
    pcb_t *p = t ? t->proc : NULL;
    if (!p) {
        return -EPERM;
    }

    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->len = sizeof(ipc_pulse_t);
    ipc_pulse_t pl = {.code = code, .value = value};
    memcpy(m->data, &pl, sizeof(pl));

    int need_preempt = 0;
    uint64_t pf = spin_lock_irqsave(&g_proc_lock);
    spin_lock(&g_ipc_lock);
    channel_t *ch = resolve_conn_locked(p, connid);
    if (!ch) {
        spin_unlock(&g_ipc_lock);
        spin_unlock_irqrestore(&g_proc_lock, pf);
        kfree(m);
        return -ESRCH;
    }
    channel_pulse_enqueue(ch, m);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        spin_lock(&g_sched_lock);
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock_locked(w);
        if (w->effective_priority > t->effective_priority) {
            need_preempt = 1;
        }
        spin_unlock(&g_sched_lock);
    }
    spin_unlock(&g_ipc_lock);
    spin_unlock_irqrestore(&g_proc_lock, pf);
    if (need_preempt) {
        schedule();
    }
    return 0;
}

long ipc_send_pulse_kernel(uint32_t target_pid, uint32_t chanid, uint32_t code,
                           uint32_t value) {
    chan_msg_t *m = kmalloc(sizeof(*m));
    if (!m) {
        return -ENOMEM;
    }
    memset(m, 0, sizeof(*m));
    m->len = sizeof(ipc_pulse_t);
    ipc_pulse_t pl = {.code = code, .value = value};
    memcpy(m->data, &pl, sizeof(pl));

    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    channel_t *ch = channel_get_locked((int)chanid);
    if (!ch || ch->owner_pid != target_pid) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        kfree(m);
        return -ESRCH;
    }
    channel_pulse_enqueue(ch, m);

    tcb_t *w = channel_recv_dequeue(ch);
    if (w) {
        spin_lock(&g_sched_lock);
        w->blocked_on_chan = NULL;
        w->ipc_result = 1;
        sched_unblock_locked(w);
        spin_unlock(&g_sched_lock);
    }
    spin_unlock_irqrestore(&g_ipc_lock, fl);
    return 0;
}

/* Wake a thread out of any IPC block, handing back `result` (used by
 * signal delivery: blocked threads resume with -EINTR). */
void ipc_unblock_thread(tcb_t *t, long result) {
    chan_msg_t *to_free = NULL;
    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    if (!t->blocked_on_chan) {
        spin_unlock_irqrestore(&g_ipc_lock, fl);
        return;
    }
    channel_t *ch = (channel_t *)t->blocked_on_chan;
    t->blocked_on_chan = NULL;

    spin_lock(&g_sched_lock);
    thread_state_t st = t->state;
    if (st == THREAD_RECEIVE_BLOCKED) {
        channel_recv_remove(ch, t);
    } else if (st == THREAD_SEND_BLOCKED) {
        chan_msg_t *m = ch->msg_head;
        while (m) {
            chan_msg_t *next = m->next;
            if (m->sender == t) {
                channel_msg_remove(ch, m);
                to_free = m;
                break;
            }
            m = next;
        }
    } else if (st == THREAD_REPLY_BLOCKED) {
        chan_msg_t **ip = &g_inflight_head;
        while (*ip) {
            chan_msg_t *m = *ip;
            if (m->sender == t) {
                *ip = m->next;
                m->next = NULL;
                if (m->in_op == 0u) {
                    to_free = m;
                } else {
                    m->detached = 1u;
                }
                break;
            }
            ip = &m->next;
        }
    }
    t->ipc_result = result;
    sched_unblock_locked(t);
    spin_unlock(&g_sched_lock);
    spin_unlock_irqrestore(&g_ipc_lock, fl);

    if (to_free) {
        kfree(to_free);
    }
}

void ipc_detach_thread(tcb_t *t) {
    chan_msg_t *free_list = NULL;
    uint64_t fl = spin_lock_irqsave(&g_ipc_lock);
    if (t->blocked_on_chan) {
        channel_t *ch = (channel_t *)t->blocked_on_chan;
        channel_recv_remove(ch, t);
        t->blocked_on_chan = NULL;
    }
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channel_t *ch = channel_get_locked(i);
        if (!ch) {
            continue;
        }
        chan_msg_t *m = ch->msg_head;
        while (m) {
            chan_msg_t *next = m->next;
            if (m->sender == t) {
                channel_msg_remove(ch, m);
                m->next = free_list;
                free_list = m;
            }
            m = next;
        }
    }
    chan_msg_t **ip = &g_inflight_head;
    while (*ip) {
        chan_msg_t *m = *ip;
        if (m->sender == t) {
            *ip = m->next;
            m->next = NULL;
            if (m->in_op == 0u) {
                m->next = free_list;
                free_list = m;
            } else {
                m->detached = 1u;
            }
        } else {
            ip = &m->next;
        }
    }
    spin_unlock_irqrestore(&g_ipc_lock, fl);

    while (free_list) {
        chan_msg_t *m = free_list;
        free_list = m->next;
        kfree(m);
    }
}
