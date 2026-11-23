/**
 * @file     main.c
 * @brief    Ring 3 /dev/net IPC server event loop and timer ticker
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/intr.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/net.h>
#include <sys/thread.h>
#include <time.h>
#include <unistd.h>
#include "net_internal.h"

/* need:: COMP_IPC_0018 */

static volatile unsigned int g_tick_refs;
static int g_self_conn = -1;

uint64_t net_now_mono_ns(void) {
    struct timespec ts = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0ULL;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void ticker_thread(void *arg) {
    (void)arg;
    struct timespec step = {.tv_sec = 0, .tv_nsec = 5000000L}; /* 5 ms */
    for (;;) {
        while (__atomic_load_n(&g_tick_refs, __ATOMIC_ACQUIRE) == 0u) {
            (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_tick_refs, FUTEX_WAIT,
                         0, 0, 0);
        }
        (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &step, NULL);
        if (g_self_conn >= 0) {
            (void)SendPulse(g_self_conn, (int)PULSE_CODE_NET_TICK, 0);
        }
    }
}

void net_tick_ref_inc(void) {
    unsigned int prev = __atomic_fetch_add(&g_tick_refs, 1u, __ATOMIC_ACQ_REL);
    if (prev == 0u) {
        (void)__sys5(SYS_FUTEX, (long)(uintptr_t)&g_tick_refs, FUTEX_WAKE, 1, 0,
                     0);
    }
}

void net_tick_ref_dec(void) {
    unsigned int cur = __atomic_load_n(&g_tick_refs, __ATOMIC_ACQUIRE);
    if (cur > 0u) {
        (void)__atomic_fetch_sub(&g_tick_refs, 1u, __ATOMIC_ACQ_REL);
    }
}

void net_reply(uint32_t msgid, const net_rep_t *rep, uint32_t payload_len) {
    if (payload_len == 0u) {
        (void)ReplyMsg((int)msgid, rep, offsetof(net_rep_t, data));
        return;
    }
    if (payload_len > NET_DATA_MAX) {
        payload_len = NET_DATA_MAX;
    }
    ipc_iov_t iov = {
        .base = (void *)rep,
        .len = offsetof(net_rep_t, data) + payload_len,
    };
    (void)WriteMsgV((int)msgid, 0u, &iov, 1u);
    (void)ReplyMsg((int)msgid, NULL, 0u);
}

static int register_name(const char *path, int chanid) {
    int conn = (int)OpenConnection(1u, 1u);
    if (conn < 0) {
        return -1;
    }
    name_msg_t msg = {0};
    name_reply_t rep = {0};
    msg.type = NAME_MSG_ATTACH;
    msg.chanid = (uint32_t)chanid;
    strncpy(msg.path, path, NAME_MAX_PATH - 1u);
    long r = SendMsg(conn, &msg, sizeof(msg), &rep, sizeof(rep));
    (void)CloseConnection(conn);
    return (r < 0 || rep.err != 0) ? -1 : 0;
}

int main(void) {
    int svc_chan = (int)CreateChannel();
    if (svc_chan < 0) {
        printf("[FAIL] netserv failed to create IPC channel\n");
        return 1;
    }

    uint8_t mac[6] = {0x52u, 0x54u, 0x00u, 0x12u, 0x34u, 0x56u};
    int nic_ready = (virtio_net_init(svc_chan, mac) == 0);
    net_proto_init(nic_ready, mac);

    g_self_conn = (int)OpenConnection((uint32_t)getpid(), (uint32_t)svc_chan);
    (void)ThreadSpawn(ticker_thread, NULL, 95);

    if (register_name("/dev/net", svc_chan) != 0) {
        printf("[FAIL] netserv failed to register /dev/net\n");
        return 1;
    }
    printf("[OK] netserv registered /dev/net (nic=%s)\n",
           nic_ready ? "virtio-net" : "loopback-only");

    static net_req_t req;
    ipc_recv_info_t info = {0};
    for (;;) {
        memset(&req, 0, offsetof(net_req_t, data));
        long msgid =
            ReceiveMsg(svc_chan, &req, offsetof(net_req_t, data), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            ipc_pulse_t pulse = {0};
            memcpy(&pulse, &req, sizeof(pulse));
            if (pulse.code == PULSE_CODE_IRQ) {
                virtio_net_handle_irq(pulse.value);
            } else if (pulse.code == PULSE_CODE_NET_TICK) {
                virtio_net_drain_rx();
                net_on_tick(net_now_mono_ns());
            } else if (pulse.code == PULSE_CODE_FD_CLOSE) {
                net_handle_fd_close(pulse.value);
            }
            continue;
        }

        uint32_t data_len = 0u;
        if (req.op == NET_OP_SOCK_SEND && req.len > 0u) {
            uint32_t want = req.len > NET_DATA_MAX ? NET_DATA_MAX : req.len;
            ipc_iov_t iov = {.base = req.data, .len = want};
            long got =
                ReadMsgV((int)msgid, offsetof(net_req_t, data), &iov, 1u);
            if (got < 0) {
                net_rep_t rep = {0};
                rep.err = -EFAULT;
                net_reply((uint32_t)msgid, &rep, 0u);
                continue;
            }
            data_len = (uint32_t)got;
        }
        net_handle_ipc((uint32_t)msgid, &req, data_len);
    }
}
