/**
 * @file     test_net.c
 * @brief    Ring 3 PCI virtio-net and TCP/IP/loopback end-to-end QEMU test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_IPC_0011 */

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/net.h>
#include <sys/socket.h>
#include <time.h>

static uint32_t g_net_pid;
static uint32_t g_net_chan;

static int resolve_net(void) {
    int conn = (int)OpenConnection(1u, 1u);
    if (conn < 0) {
        return -1;
    }
    name_msg_t q = {0};
    name_reply_t r = {0};
    q.type = NAME_MSG_QUERY;
    strncpy(q.path, "/dev/net", NAME_MAX_PATH - 1u);
    for (int tries = 0; tries < 100; tries++) {
        memset(&r, 0, sizeof(r));
        if (SendMsg(conn, &q, sizeof(q), &r, sizeof(r)) >= 0 && r.err == 0 &&
            r.pid > 0) {
            g_net_pid = (uint32_t)r.pid;
            g_net_chan = (uint32_t)r.chanid;
            (void)CloseConnection(conn);
            return 0;
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        (void)nanosleep(&ts, NULL);
    }
    (void)CloseConnection(conn);
    return -1;
}

static int net_call(int conn, const net_req_t *req, uint32_t tx_data_len,
                    net_rep_t *rep, uint32_t rx_data_len) {
    memset(rep, 0, offsetof(net_rep_t, data));
    size_t slen = offsetof(net_req_t, data) + tx_data_len;
    size_t rlen = offsetof(net_rep_t, data) + rx_data_len;
    if (SendMsg(conn, req, slen, rep, rlen) < 0) {
        return -EIO;
    }
    return rep->err;
}

static net_addr_t make_v4(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                          uint16_t port) {
    net_addr_t out = {0};
    out.family = NET_AF_INET;
    out.port = port;
    out.addr[0] = a;
    out.addr[1] = b;
    out.addr[2] = c;
    out.addr[3] = d;
    return out;
}

static net_addr_t make_v6_lo(uint16_t port) {
    net_addr_t out = {0};
    out.family = NET_AF_INET6;
    out.port = port;
    out.addr[15] = 1u;
    return out;
}

typedef struct {
    net_addr_t target;
    const char *msg;
    int rc;
    char reply[64];
} tcp_cli_arg_t;

static void *tcp_client_thread(void *raw) {
    tcp_cli_arg_t *arg = (tcp_cli_arg_t *)raw;
    arg->rc = -1;
    int conn = (int)OpenConnection(g_net_pid, g_net_chan);
    if (conn < 0) {
        return NULL;
    }

    net_req_t req = {0};
    net_rep_t rep = {0};
    req.op = NET_OP_SOCK_CREATE;
    req.arg1 = (int32_t)arg->target.family;
    req.arg2 = NET_SOCK_STREAM;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        (void)CloseConnection(conn);
        return NULL;
    }
    uint32_t h = rep.handle;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CONNECT;
    req.handle = h;
    req.addr = arg->target;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto done;
    }

    size_t mlen = strlen(arg->msg);
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_SEND;
    req.handle = h;
    req.len = (uint32_t)mlen;
    memcpy(req.data, arg->msg, mlen);
    if (net_call(conn, &req, (uint32_t)mlen, &rep, 0u) != 0 ||
        rep.len != mlen) {
        goto done;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = h;
    req.len = sizeof(arg->reply) - 1u;
    if (net_call(conn, &req, 0u, &rep, sizeof(arg->reply) - 1u) != 0 ||
        rep.len == 0u) {
        goto done;
    }
    memcpy(arg->reply, rep.data, rep.len);
    arg->reply[rep.len] = '\0';

    /* Graceful FIN half-close then wait for peer EOF. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_SHUTDOWN;
    req.handle = h;
    req.arg1 = SHUT_WR;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto done;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = h;
    req.len = 16u;
    if (net_call(conn, &req, 0u, &rep, 16u) != 0 || rep.len != 0u) {
        goto done;
    }
    arg->rc = 0;

done:
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = h;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    (void)CloseConnection(conn);
    return NULL;
}

static int test_udp_roundtrip(int conn, net_addr_t srv_addr, const char *tag) {
    net_req_t req = {0};
    net_rep_t rep = {0};

    req.op = NET_OP_SOCK_CREATE;
    req.arg1 = (int32_t)srv_addr.family;
    req.arg2 = NET_SOCK_DGRAM;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        return -1;
    }
    uint32_t srv = rep.handle;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_BIND;
    req.handle = srv;
    req.addr = srv_addr;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto fail_srv;
    }
    net_addr_t bound = rep.addr;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CREATE;
    req.arg1 = (int32_t)srv_addr.family;
    req.arg2 = NET_SOCK_DGRAM;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto fail_srv;
    }
    uint32_t cli = rep.handle;

    size_t tlen = strlen(tag);
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_SEND;
    req.handle = cli;
    req.addr = bound;
    req.len = (uint32_t)tlen;
    memcpy(req.data, tag, tlen);
    if (net_call(conn, &req, (uint32_t)tlen, &rep, 0u) != 0 ||
        rep.len != tlen) {
        goto fail_both;
    }

    /* Check poll reports POLLIN on srv. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_POLL;
    req.poll_nfds = 1u;
    req.poll_timeout_ns = 100000000LL;
    req.poll_items[0].handle = srv;
    req.poll_items[0].events = POLLIN;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 || rep.handle != 1u ||
        (rep.u.poll_items[0].revents & POLLIN) == 0) {
        goto fail_both;
    }

    /* Receive on srv and echo back to sender's ephemeral port. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = srv;
    req.len = 64u;
    if (net_call(conn, &req, 0u, &rep, 64u) != 0 || rep.len != tlen ||
        memcmp(rep.data, tag, tlen) != 0 || rep.addr.port == 0u) {
        goto fail_both;
    }
    net_addr_t cli_ep = rep.addr;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_SEND;
    req.handle = srv;
    req.addr = cli_ep;
    req.len = (uint32_t)tlen;
    memcpy(req.data, tag, tlen);
    if (net_call(conn, &req, (uint32_t)tlen, &rep, 0u) != 0 ||
        rep.len != tlen) {
        goto fail_both;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = cli;
    req.len = 64u;
    if (net_call(conn, &req, 0u, &rep, 64u) != 0 || rep.len != tlen ||
        memcmp(rep.data, tag, tlen) != 0 || rep.addr.port != bound.port) {
        goto fail_both;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = cli;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    req.handle = srv;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    return 0;

fail_both:
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = cli;
    (void)net_call(conn, &req, 0u, &rep, 0u);
fail_srv:
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = srv;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    return -1;
}

static int test_tcp_roundtrip(int conn, net_addr_t bind_addr, const char *req_s,
                              const char *rep_s) {
    net_req_t req = {0};
    net_rep_t rep = {0};

    req.op = NET_OP_SOCK_CREATE;
    req.arg1 = (int32_t)bind_addr.family;
    req.arg2 = NET_SOCK_STREAM;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        return -1;
    }
    uint32_t srv = rep.handle;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_BIND;
    req.handle = srv;
    req.addr = bind_addr;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto fail_srv;
    }
    net_addr_t bound = rep.addr;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_LISTEN;
    req.handle = srv;
    req.arg1 = 4;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        goto fail_srv;
    }

    tcp_cli_arg_t carg = {0};
    carg.target = bound;
    carg.msg = req_s;
    pthread_t th;
    if (pthread_create(&th, NULL, tcp_client_thread, &carg) != 0) {
        goto fail_srv;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_ACCEPT;
    req.handle = srv;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        (void)pthread_join(th, NULL);
        goto fail_srv;
    }
    uint32_t acc = rep.handle;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = acc;
    req.len = 64u;
    size_t expect_len = strlen(req_s);
    if (net_call(conn, &req, 0u, &rep, 64u) != 0 || rep.len != expect_len ||
        memcmp(rep.data, req_s, expect_len) != 0) {
        goto fail_acc;
    }

    size_t rlen = strlen(rep_s);
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_SEND;
    req.handle = acc;
    req.len = (uint32_t)rlen;
    memcpy(req.data, rep_s, rlen);
    if (net_call(conn, &req, (uint32_t)rlen, &rep, 0u) != 0 ||
        rep.len != rlen) {
        goto fail_acc;
    }

    /* Receive client FIN EOF, then close server accepted socket. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_RECV;
    req.handle = acc;
    req.len = 16u;
    if (net_call(conn, &req, 0u, &rep, 16u) != 0 || rep.len != 0u) {
        goto fail_acc;
    }

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = acc;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    req.handle = srv;
    (void)net_call(conn, &req, 0u, &rep, 0u);

    (void)pthread_join(th, NULL);
    if (carg.rc != 0 || strcmp(carg.reply, rep_s) != 0) {
        return -1;
    }
    return 0;

fail_acc:
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = acc;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    (void)pthread_join(th, NULL);
fail_srv:
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = srv;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    return -1;
}

int main(void) {
    if (resolve_net() != 0) {
        printf("[FAIL] nettest: failed to resolve /dev/net\n");
        return 1;
    }
    int conn = (int)OpenConnection(g_net_pid, g_net_chan);
    if (conn < 0) {
        printf("[FAIL] nettest: OpenConnection(/dev/net) failed\n");
        return 1;
    }

    static net_req_t req;
    static net_rep_t rep;

    /* 1. Verify virtio-net NIC and interface configuration (lo0 + eth0). */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_INFO;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 || !rep.u.info.nic_present ||
        rep.u.info.if_count != 2u ||
        strcmp(rep.u.info.ifs[0].name, "lo0") != 0 ||
        strcmp(rep.u.info.ifs[1].name, "eth0") != 0) {
        printf("[FAIL] nettest: NET_OP_INFO failed (nic=%u ifs=%u)\n",
               rep.u.info.nic_present, rep.u.info.if_count);
        (void)CloseConnection(conn);
        return 1;
    }
    uint64_t irq_before = rep.u.info.irq_count;

    /* 2. Real PCI virtio-net hardware ARP + ICMPv4 Echo against SLIRP gateway
     *    10.0.2.2, verifying TX/RX virtqueues and IOAPIC IRQ pulses. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_ARP_RESOLVE;
    req.addr = make_v4(10u, 0u, 2u, 2u, 0u);
    req.arg2 = 3000;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        printf("[FAIL] nettest: ARP resolve 10.0.2.2 failed (err=%d)\n",
               rep.err);
        (void)CloseConnection(conn);
        return 1;
    }
    uint8_t gw_mac[6];
    memcpy(gw_mac, rep.mac, 6u);

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_PING;
    req.addr = make_v4(10u, 0u, 2u, 2u, 0u);
    req.arg1 = 1;
    req.arg2 = 3000;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 || rep.flags != 1u) {
        printf("[FAIL] nettest: ICMPv4 ping 10.0.2.2 failed (err=%d)\n",
               rep.err);
        (void)CloseConnection(conn);
        return 1;
    }
    uint32_t gw_rtt = rep.len;

    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_INFO;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 ||
        rep.u.info.irq_count <= irq_before || rep.u.info.rx_packets < 2u ||
        rep.u.info.tx_packets < 2u) {
        printf("[FAIL] nettest: expected virtio-net IRQ/RX/TX activity "
               "(irqs=%llu rx=%llu tx=%llu)\n",
               (unsigned long long)rep.u.info.irq_count,
               (unsigned long long)rep.u.info.rx_packets,
               (unsigned long long)rep.u.info.tx_packets);
        (void)CloseConnection(conn);
        return 1;
    }
    printf("[OK] nettest virtio-net ARP and ICMPv4 ping to 10.0.2.2 verified "
           "(gw=%02x:%02x:%02x:%02x:%02x:%02x rtt=%ums irqs=%llu)\n",
           gw_mac[0], gw_mac[1], gw_mac[2], gw_mac[3], gw_mac[4], gw_mac[5],
           gw_rtt, (unsigned long long)rep.u.info.irq_count);

    /* 3. Loopback ICMPv4 (127.0.0.1) and ICMPv6 (::1) Echo Request/Reply. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_PING;
    req.addr = make_v4(127u, 0u, 0u, 1u, 0u);
    req.arg1 = 7;
    req.arg2 = 1000;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 || rep.flags != 7u) {
        printf("[FAIL] nettest: loopback ICMPv4 ping failed\n");
        (void)CloseConnection(conn);
        return 1;
    }
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_PING;
    req.addr = make_v6_lo(0u);
    req.arg1 = 8;
    req.arg2 = 1000;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 || rep.flags != 8u) {
        printf("[FAIL] nettest: loopback ICMPv6 ping failed\n");
        (void)CloseConnection(conn);
        return 1;
    }
    printf("[OK] nettest loopback ICMPv4 (127.0.0.1) and ICMPv6 (::1) echo "
           "verified\n");

    /* 4. Loopback IPv4 and IPv6 UDP datagrams + poll. */
    if (test_udp_roundtrip(conn, make_v4(127u, 0u, 0u, 1u, 0u),
                           "udp-v4-hello") != 0 ||
        test_udp_roundtrip(conn, make_v6_lo(0u), "udp-v6-hello") != 0) {
        printf("[FAIL] nettest: IPv4/IPv6 UDP roundtrip failed\n");
        (void)CloseConnection(conn);
        return 1;
    }
    printf("[OK] nettest IPv4 and IPv6 UDP bind/send/recv/poll verified\n");

    /* 5. Loopback IPv4 and IPv6 TCP 3-way handshake, data transfer, and FIN. */
    if (test_tcp_roundtrip(conn, make_v4(127u, 0u, 0u, 1u, 0u), "tcp-v4-req",
                           "tcp-v4-ack") != 0 ||
        test_tcp_roundtrip(conn, make_v6_lo(0u), "tcp-v6-req", "tcp-v6-ack") !=
            0) {
        printf("[FAIL] nettest: IPv4/IPv6 TCP roundtrip failed\n");
        (void)CloseConnection(conn);
        return 1;
    }

    /* Verify RST -> ECONNREFUSED on unbound TCP port. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CREATE;
    req.arg1 = (int32_t)NET_AF_INET;
    req.arg2 = NET_SOCK_STREAM;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0) {
        (void)CloseConnection(conn);
        return 1;
    }
    uint32_t rst_h = rep.handle;
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CONNECT;
    req.handle = rst_h;
    req.addr = make_v4(127u, 0u, 0u, 1u, 9u);
    int rst_rc = net_call(conn, &req, 0u, &rep, 0u);
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_CLOSE;
    req.handle = rst_h;
    (void)net_call(conn, &req, 0u, &rep, 0u);
    if (rst_rc != -ECONNREFUSED) {
        printf("[FAIL] nettest: expected ECONNREFUSED on closed TCP port, "
               "got %d\n",
               rst_rc);
        (void)CloseConnection(conn);
        return 1;
    }

    /* Verify zero leaked sockets or waiters in netserv. */
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_STATS;
    if (net_call(conn, &req, 0u, &rep, 0u) != 0 ||
        rep.u.stats.active_socks != 0u || rep.u.stats.active_waiters != 0u) {
        printf("[FAIL] nettest: leaked netserv handles (socks=%u waiters=%u)\n",
               rep.u.stats.active_socks, rep.u.stats.active_waiters);
        (void)CloseConnection(conn);
        return 1;
    }

    (void)CloseConnection(conn);
    printf("[OK] nettest IPv4 and IPv6 TCP handshake, data, FIN, and RST "
           "verified\n");
    return 0;
}
