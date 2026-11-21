/**
 * @file     ipc.h
 * @brief    Ring 3 synchronous IPC and sysproc name-service API
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0001 */
/* Microkernel IPC userland API (synchronous message passing) and the
 * name-service protocol spoken with sysproc (PID 1, chanid 1). */
#include <stddef.h>
#include <stdint.h>
#include <sys/microkernel.h>

/* Name service (sysproc, chanid 1): servers ATTACH a path, clients QUERY it. */
#define NAME_MSG_ATTACH 1
#define NAME_MSG_QUERY 2
#define NAME_MAX_PATH 60

typedef struct {
    uint32_t type;
    uint32_t chanid;
    char path[NAME_MAX_PATH];
} name_msg_t;

typedef struct {
    int32_t pid; /* 0 if unresolved */
    int32_t chanid;
    int32_t err; /* 0 = ok */
    int32_t reserved;
} name_reply_t;

/* Echo demo protocol (/dev/echo). */
typedef struct {
    uint32_t seq;
    char text[64];
} echo_msg_t;

/* Filled in by ReceiveMsg describing the message source. msgid == 0 means a
 * pulse (8-byte ipc_pulse_t payload, no reply expected). */
typedef struct {
    uint32_t msgid;
    uint32_t pid;
    uint32_t len;
    uint32_t reserved;
} ipc_recv_info_t;

typedef struct {
    uint32_t code;
    uint32_t value;
} ipc_pulse_t;

typedef struct {
    void *base;
    size_t len;
} ipc_iov_t;

#define IPC_IOV_MAX 16

static inline long CreateChannel(void) {
    return __sys0(SYS_CHANNEL_CREATE);
}

static inline long DestroyChannel(int chanid) {
    return __sys1(SYS_CHANNEL_DESTROY, chanid);
}

static inline long OpenConnection(uint32_t pid, uint32_t chanid) {
    return __sys2(SYS_CONNECT_ATTACH, (long)pid, (long)chanid);
}

static inline long CloseConnection(int connid) {
    return __sys1(SYS_CONNECT_DETACH, connid);
}

/* Send sbytes from smsg over connid and block until the server replies into
 * rmsg (up to rbytes). Returns the reply length, or negative errno. */
static inline long SendMsg(int connid, const void *smsg, size_t sbytes,
                           void *rmsg, size_t rbytes) {
    return __sys5(SYS_MSG_SENDV, connid, (long)smsg, (long)sbytes, (long)rmsg,
                  (long)rbytes);
}

/* Receive the next message/pulse on chanid. Returns msgid (>0 message, 0
 * pulse) or negative errno. */
static inline long ReceiveMsg(int chanid, void *buf, size_t bytes,
                              ipc_recv_info_t *info) {
    return __sys4(SYS_MSG_RECEIVE, chanid, (long)buf, (long)bytes, (long)info);
}

/* Reply to msgid with rbytes from rmsg. Returns 0 or negative errno. */
static inline long ReplyMsg(int msgid, const void *rmsg, size_t rbytes) {
    return __sys3(SYS_MSG_REPLYV, msgid, (long)rmsg, (long)rbytes);
}

/* Read into scatter buffers from an in-flight client message at byte offset. */
static inline long ReadMsgV(int msgid, size_t offset, const ipc_iov_t *iov,
                            size_t iovcnt) {
    return __sys4(SYS_MSG_READV, msgid, (long)offset, (long)(uintptr_t)iov,
                  (long)iovcnt);
}

/* Write from gather buffers into an in-flight client's reply buffer. */
static inline long WriteMsgV(int msgid, size_t offset, const ipc_iov_t *iov,
                             size_t iovcnt) {
    return __sys4(SYS_MSG_WRITEV, msgid, (long)offset, (long)(uintptr_t)iov,
                  (long)iovcnt);
}

/* Fire-and-forget 8-byte pulse to the receiver on connid. */
static inline long SendPulse(int connid, int code, int value) {
    return __sys3(SYS_MSG_SEND_PULSE, connid, code, value);
}
