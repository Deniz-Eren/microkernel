/**
 * @file     channel.c
 * @brief    IPC channel creation, destruction, and priority queues
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "channel.h"
#include <sys/errno.h>
#include <kernel/printk.h>
#include <kernel/string.h>
#include <kernel/sched.h>

/* need:: COMP_IPC_0001 COMP_PROC_0008 */

spinlock_t g_ipc_lock = SPINLOCK_INIT;
static channel_t g_channels[MAX_CHANNELS];

int channel_precreate_sysproc(uint32_t owner_pid) {
    uint64_t flags = spin_lock_irqsave(&g_ipc_lock);
    channel_t *ch = &g_channels[1];
    memset(ch, 0, sizeof(*ch));
    ch->used = 1;
    ch->precreated = 1;
    ch->owner_pid = owner_pid;
    ch->chanid = 1u;
    ch->refcount = 1;
    spin_unlock_irqrestore(&g_ipc_lock, flags);
    return 1;
}

int channel_create(uint32_t owner_pid) {
    uint64_t flags = spin_lock_irqsave(&g_ipc_lock);
    if (owner_pid == 1u && g_channels[1].used && g_channels[1].precreated &&
        g_channels[1].owner_pid == 1u) {
        g_channels[1].precreated = 0;
        spin_unlock_irqrestore(&g_ipc_lock, flags);
        return 1;
    }
    /* Slot 0 is reserved as the NULL channel sentinel; slot 1 is sysproc. */
    for (int i = 1; i < MAX_CHANNELS; i++) {
        if (i == 1 && owner_pid != 1u) {
            continue;
        }
        if (!g_channels[i].used) {
            channel_t *ch = &g_channels[i];
            memset(ch, 0, sizeof(*ch));
            ch->used = 1;
            ch->owner_pid = owner_pid;
            ch->chanid = (uint32_t)i;
            ch->refcount = 1;
            spin_unlock_irqrestore(&g_ipc_lock, flags);
            return i;
        }
    }
    spin_unlock_irqrestore(&g_ipc_lock, flags);
    return -ENOMEM;
}

int channel_destroy(uint32_t owner_pid, int chanid) {
    uint64_t flags = spin_lock_irqsave(&g_ipc_lock);
    if (chanid < 0 || chanid >= MAX_CHANNELS || !g_channels[chanid].used) {
        spin_unlock_irqrestore(&g_ipc_lock, flags);
        return -ESRCH;
    }
    if (g_channels[chanid].owner_pid != owner_pid) {
        spin_unlock_irqrestore(&g_ipc_lock, flags);
        return -EPERM;
    }
    g_channels[chanid].used = 0;
    spin_unlock_irqrestore(&g_ipc_lock, flags);
    return 0;
}

channel_t *channel_get_locked(int chanid) {
    if (chanid < 0 || chanid >= MAX_CHANNELS || !g_channels[chanid].used) {
        return NULL;
    }
    return &g_channels[chanid];
}

channel_t *channel_get(int chanid) {
    uint64_t flags = spin_lock_irqsave(&g_ipc_lock);
    channel_t *ch = channel_get_locked(chanid);
    spin_unlock_irqrestore(&g_ipc_lock, flags);
    return ch;
}

/* Insert into a priority-ordered singly-linked queue: before the first entry
 * with strictly lower priority (FIFO within an equal priority level). */
#define PRIO_INSERT(head, tail, obj, prio_expr, next_field)                    \
    do {                                                                       \
        __typeof__(obj) __m = (obj);                                           \
        __typeof__(obj) __p = NULL;                                            \
        __typeof__(obj) __c = (head);                                          \
        while (__c && (__c->prio_expr >= __m->prio_expr)) {                    \
            __p = __c;                                                         \
            __c = __c->next_field;                                             \
        }                                                                      \
        __m->next_field = __c;                                                 \
        if (__p) {                                                             \
            __p->next_field = __m;                                             \
        } else {                                                               \
            (head) = __m;                                                      \
        }                                                                      \
        if (!__m->next_field) {                                                \
            (tail) = __m;                                                      \
        }                                                                      \
    } while (0)

void channel_msg_enqueue(channel_t *ch, chan_msg_t *m) {
    PRIO_INSERT(ch->msg_head, ch->msg_tail, m, sender->effective_priority,
                next);
}

chan_msg_t *channel_msg_dequeue(channel_t *ch) {
    chan_msg_t *m = ch->msg_head;
    if (m) {
        ch->msg_head = m->next;
        if (!ch->msg_head) {
            ch->msg_tail = NULL;
        }
        m->next = NULL;
    }
    return m;
}

void channel_msg_remove(channel_t *ch, chan_msg_t *m) {
    chan_msg_t **pp = &ch->msg_head;
    chan_msg_t *prev = NULL;
    while (*pp) {
        if (*pp == m) {
            *pp = m->next;
            if (ch->msg_tail == m) {
                ch->msg_tail = prev;
            }
            m->next = NULL;
            return;
        }
        prev = *pp;
        pp = &(*pp)->next;
    }
}

void channel_pulse_enqueue(channel_t *ch, chan_msg_t *m) {
    /* Pulses have no sender thread; order them at the tail (FIFO). */
    m->next = NULL;
    if (ch->pulse_tail) {
        ch->pulse_tail->next = m;
    } else {
        ch->pulse_head = m;
    }
    ch->pulse_tail = m;
}

chan_msg_t *channel_pulse_dequeue(channel_t *ch) {
    chan_msg_t *m = ch->pulse_head;
    if (m) {
        ch->pulse_head = m->next;
        if (!ch->pulse_head) {
            ch->pulse_tail = NULL;
        }
        m->next = NULL;
    }
    return m;
}

void channel_recv_enqueue(channel_t *ch, struct tcb *t) {
    PRIO_INSERT(ch->recv_head, ch->recv_tail, t, effective_priority, ipc_next);
}

struct tcb *channel_recv_dequeue(channel_t *ch) {
    struct tcb *t = ch->recv_head;
    if (t) {
        ch->recv_head = t->ipc_next;
        if (!ch->recv_head) {
            ch->recv_tail = NULL;
        }
        t->ipc_next = NULL;
    }
    return t;
}

void channel_recv_remove(channel_t *ch, struct tcb *t) {
    struct tcb **pp = &ch->recv_head;
    struct tcb *prev = NULL;
    while (*pp) {
        if (*pp == t) {
            *pp = t->ipc_next;
            if (ch->recv_tail == t) {
                ch->recv_tail = prev;
            }
            t->ipc_next = NULL;
            return;
        }
        prev = *pp;
        pp = &(*pp)->ipc_next;
    }
}
