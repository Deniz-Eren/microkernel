/**
 * @file     ipc.h
 * @brief    Kernel synchronous IPC and pulse subsystem interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
/* Synchronous message-passing IPC (Milestone 3).
 *
 * Model: a channel is a receive endpoint owned by one process (chanid). A
 * client opens a connection (connid) to (pid, chanid) and sends over it.
 * SendMsg blocks the client until a receiver accepts the message, then until
 * the receiver replies (SEND_BLOCKED -> REPLY_BLOCKED). ReceiveMsg blocks the
 * server until a message or pulse arrives. ReplyMsg completes the round trip
 * and hands the reply length back to the client as its SendMsg return value.
 *
 * Message payloads are staged through kernel-owned nodes and copied with
 * explicit per-address-space accessors, so no user pointer is ever dereferenced
 * directly and SMAP is never bypassed.
 */
#include <stdint.h>
#include <kernel/sched.h>

#define IPC_MSG_MAX 512 /* largest stageable message payload */

/* Kernel-side message node: one per in-flight send or pulse. */
typedef struct chan_msg {
    struct chan_msg *next;
    tcb_t *sender;          /* blocked client (NULL for pulses) */
    tcb_t *receiver;        /* server tcb that accepted it */
    uint64_t sender_pml4;   /* client address space for the reply copy */
    uint64_t reply_dst;     /* client reply buffer (user VA) */
    uint64_t reply_max;     /* client reply buffer capacity */
    uint64_t reply_written; /* high-water bytes written via WriteMsgV */
    uint32_t msgid;         /* 0 for pulses */
    uint32_t len;           /* staged payload length */
    uint8_t is_static;      /* 1 = pre-allocated IRQ node (do not kfree) */
    uint8_t in_use;         /* 1 = static IRQ node currently queued */
    uint8_t data[IPC_MSG_MAX];
} chan_msg_t;

typedef struct {
    void *base;
    size_t len;
} ipc_iov_t;

#define IPC_IOV_MAX 16

typedef struct {
    uint32_t msgid; /* 0 = pulse */
    uint32_t pid;   /* sender pid (0 for pulses) */
    uint32_t len;   /* bytes written into the receive buffer */
    uint32_t reserved;
} ipc_recv_info_t;

typedef struct {
    uint32_t code;
    uint32_t value;
} ipc_pulse_t;

/* Syscall entry points (called from syscall.c with the caller current). */
long ipc_conn_attach(uint32_t target_pid, uint32_t chanid);
long ipc_conn_detach(int connid);
long ipc_msg_send(int connid, uint64_t smsg, uint64_t sbytes, uint64_t rmsg,
                  uint64_t rbytes);
long ipc_msg_receive(int chanid, uint64_t buf, uint64_t bytes, uint64_t info);
long ipc_msg_reply(int msgid, uint64_t rmsg, uint64_t rbytes);
long ipc_msg_readv(int msgid, uint64_t offset, uint64_t iov_va,
                   uint64_t iovcnt);
long ipc_msg_writev(int msgid, uint64_t offset, uint64_t iov_va,
                    uint64_t iovcnt);
long ipc_msg_send_pulse(int connid, uint32_t code, uint32_t value);
void ipc_deliver_irq_pulse(int chanid, chan_msg_t *node, uint32_t code,
                           uint32_t value);

/* Reclaim a dying thread's queue slots and in-flight messages. */
void ipc_detach_thread(tcb_t *t);

/* Wake a thread out of any IPC block with the given result (e.g. -EINTR
 * when a signal arrives). */
void ipc_unblock_thread(tcb_t *t, long result);
