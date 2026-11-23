/**
 * @file     net_proto.c
 * @brief    Ring 3 Ethernet, ARP, IPv4, IPv6, ICMP, UDP, TCP, and loopback
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/net.h>
#include <sys/socket.h>
#include "net_internal.h"

/* need:: COMP_IPC_0018 */

#define NET_SOCK_MAX 48u
#define TCP_RX_BUF_CAP 32768u
#define TCP_MSS 1400u
#define UDP_Q_MAX 16u
#define ACCEPT_Q_MAX 16u
#define ARP_TABLE_MAX 16u
#define ARP_PENDING_MAX 8u
#define LO_Q_MAX 32u

#define ARP_WAIT_MAX 8u
#define PING_WAIT_MAX 8u
#define CONN_WAIT_MAX 16u
#define ACC_WAIT_MAX 16u
#define RECV_WAIT_MAX 32u
#define POLL_WAIT_MAX 16u

#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u

enum {
    TCP_CLOSED = 0,
    TCP_LISTEN = 1,
    TCP_SYN_SENT = 2,
    TCP_SYN_RCVD = 3,
    TCP_ESTABLISHED = 4,
    TCP_FIN_WAIT_1 = 5,
    TCP_FIN_WAIT_2 = 6,
    TCP_CLOSE_WAIT = 7,
    TCP_LAST_ACK = 8,
    TCP_TIME_WAIT = 9,
};

typedef struct {
    uint16_t len;
    net_addr_t sender;
    uint8_t data[NET_DATA_MAX];
} udp_pkt_t;

typedef struct {
    uint8_t used;
    uint8_t user_closed;
    uint8_t bound;
    uint8_t connected;
    uint8_t shut_rd;
    uint8_t shut_wr;
    uint8_t peer_fin;
    uint8_t tcp_state;
    uint8_t so_reuseaddr;
    uint8_t so_keepalive;
    uint8_t tcp_nodelay;
    uint16_t family;
    int32_t type;
    int32_t proto;
    int32_t so_error;
    uint32_t refs;
    uint32_t backlog;
    uint32_t parent_idx;
    net_addr_t local;
    net_addr_t peer;

    /* TCP sequence state. */
    uint32_t snd_nxt;
    uint32_t snd_una;
    uint32_t rcv_nxt;
    uint64_t close_deadline_ns;

    /* TCP receive ring buffer. */
    uint32_t rx_head;
    uint32_t rx_len;
    uint8_t rx_buf[TCP_RX_BUF_CAP];

    /* TCP listening accept queue. */
    uint32_t accept_head;
    uint32_t accept_len;
    uint32_t accept_q[ACCEPT_Q_MAX];

    /* UDP datagram queue. */
    uint32_t dg_head;
    uint32_t dg_len;
    udp_pkt_t dg_q[UDP_Q_MAX];
} net_sock_t;

typedef struct {
    uint8_t valid;
    uint8_t ip4[4];
    uint8_t mac[6];
} arp_entry_t;

typedef struct {
    uint8_t used;
    uint8_t next_hop[4];
    uint16_t len;
    uint8_t pkt[ETH_MTU];
} arp_pending_t;

typedef struct {
    uint16_t ethertype;
    uint16_t len;
    uint8_t pkt[ETH_MTU];
} lo_pkt_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint8_t ip4[4];
    uint64_t deadline_ns;
    uint64_t next_retry_ns;
} arp_waiter_t;

typedef struct {
    uint8_t used;
    uint16_t family;
    uint16_t id;
    uint16_t seq;
    uint32_t msgid;
    uint64_t start_ns;
    uint64_t deadline_ns;
} ping_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t sock_idx;
    uint64_t deadline_ns;
    uint64_t next_retry_ns;
} conn_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t sock_idx;
} acc_waiter_t;

typedef struct {
    uint8_t used;
    uint32_t msgid;
    uint32_t sock_idx;
    uint32_t want_len;
    uint32_t flags;
} recv_waiter_t;

typedef struct {
    uint8_t used;
    uint8_t has_deadline;
    uint32_t msgid;
    uint32_t nfds;
    uint64_t deadline_ns;
    net_poll_item_t items[NET_POLL_MAX];
} poll_waiter_t;

/* Local interface configuration (lo0 + QEMU SLIRP eth0). */
static int g_nic_ready;
static uint8_t g_mac[6];
static const uint8_t g_ip4[4] = {10u, 0u, 2u, 15u};
static const uint8_t g_mask4[4] = {255u, 255u, 255u, 0u};
static const uint8_t g_gw4[4] = {10u, 0u, 2u, 2u};
static const uint8_t g_lo4[4] = {127u, 0u, 0u, 1u};
static const uint8_t g_lo6[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 1u};
static uint8_t g_ip6[16];

static uint16_t g_ip4_id = 1u;
static uint16_t g_ping_id = 0x4D4Bu;
static uint16_t g_ephemeral_port = 49152u;
static uint32_t g_iss_seed = 0x10203040u;

static net_sock_t g_socks[NET_SOCK_MAX];
static arp_entry_t g_arp[ARP_TABLE_MAX];
static arp_pending_t g_arp_pend[ARP_PENDING_MAX];
static lo_pkt_t g_lo_q[LO_Q_MAX];
static uint32_t g_lo_head;
static uint32_t g_lo_len;
static int g_draining_lo;

static arp_waiter_t g_arp_wait[ARP_WAIT_MAX];
static ping_waiter_t g_ping_wait[PING_WAIT_MAX];
static conn_waiter_t g_conn_wait[CONN_WAIT_MAX];
static acc_waiter_t g_acc_wait[ACC_WAIT_MAX];
static recv_waiter_t g_recv_wait[RECV_WAIT_MAX];
static poll_waiter_t g_poll_wait[POLL_WAIT_MAX];

static void wake_all_waiters(void);
static void drain_loopback(void);
static void net_rx_ipv4(const uint8_t *pkt, uint32_t len);
static void net_rx_ipv6(const uint8_t *pkt, uint32_t len);
static int tx_ipv4(const uint8_t dst[4], uint8_t proto, const uint8_t *payload,
                   uint32_t plen);
static int tx_ipv6(const uint8_t src[16], const uint8_t dst[16],
                   uint8_t next_hdr, const uint8_t *payload, uint32_t plen);

/* --- Checksum helpers (RFC 1071) --- */

static uint32_t csum_add(uint32_t sum, const uint8_t *buf, uint32_t len) {
    uint32_t i = 0u;
    while (i + 1u < len) {
        sum += ((uint32_t)buf[i] << 8) | (uint32_t)buf[i + 1u];
        i += 2u;
    }
    if (i < len) {
        sum += (uint32_t)buf[i] << 8;
    }
    return sum;
}

static uint16_t csum_finish(uint32_t sum) {
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xFFFFu);
}

static uint16_t csum_buf(const uint8_t *buf, uint32_t len) {
    return csum_finish(csum_add(0u, buf, len));
}

static uint16_t csum_v4_pseudo(const uint8_t src[4], const uint8_t dst[4],
                               uint8_t proto, const uint8_t *seg,
                               uint32_t len) {
    uint8_t ph[12];
    memcpy(ph, src, 4u);
    memcpy(ph + 4u, dst, 4u);
    ph[8] = 0u;
    ph[9] = proto;
    net_wr_be16(ph + 10u, (uint16_t)len);
    uint32_t sum = csum_add(0u, ph, 12u);
    sum = csum_add(sum, seg, len);
    return csum_finish(sum);
}

static uint16_t csum_v6_pseudo(const uint8_t src[16], const uint8_t dst[16],
                               uint8_t next_hdr, const uint8_t *seg,
                               uint32_t len) {
    uint8_t ph[40];
    memcpy(ph, src, 16u);
    memcpy(ph + 16u, dst, 16u);
    net_wr_be32(ph + 32u, len);
    ph[36] = 0u;
    ph[37] = 0u;
    ph[38] = 0u;
    ph[39] = next_hdr;
    uint32_t sum = csum_add(0u, ph, 40u);
    sum = csum_add(sum, seg, len);
    return csum_finish(sum);
}

/* --- Address helpers --- */

static int ip4_is_zero(const uint8_t ip[4]) {
    return (ip[0] | ip[1] | ip[2] | ip[3]) == 0u;
}

static int ip4_is_local(const uint8_t ip[4]) {
    return ip[0] == 127u || memcmp(ip, g_ip4, 4u) == 0;
}

static int ip6_is_zero(const uint8_t ip[16]) {
    uint8_t acc = 0u;
    for (int i = 0; i < 16; i++) {
        acc |= ip[i];
    }
    return acc == 0u;
}

static int ip6_is_local(const uint8_t ip[16]) {
    return memcmp(ip, g_lo6, 16u) == 0 || memcmp(ip, g_ip6, 16u) == 0;
}

static int addr_is_zero(const net_addr_t *a) {
    return (a->family == NET_AF_INET6) ? ip6_is_zero(a->addr)
                                       : ip4_is_zero(a->addr);
}

static int addr_ip_eq(const net_addr_t *a, const net_addr_t *b) {
    if (a->family != b->family) {
        return 0;
    }
    size_t n = (a->family == NET_AF_INET6) ? 16u : 4u;
    return memcmp(a->addr, b->addr, n) == 0;
}

static void choose_src_ip(const net_addr_t *dst, net_addr_t *src_out) {
    memset(src_out, 0, sizeof(*src_out));
    src_out->family = dst->family;
    if (dst->family == NET_AF_INET6) {
        if (memcmp(dst->addr, g_lo6, 16u) == 0) {
            memcpy(src_out->addr, g_lo6, 16u);
        } else {
            memcpy(src_out->addr, g_ip6, 16u);
        }
    } else {
        if (dst->addr[0] == 127u) {
            memcpy(src_out->addr, g_lo4, 4u);
        } else {
            memcpy(src_out->addr, g_ip4, 4u);
        }
    }
}

/* --- Ethernet & ARP --- */

static int eth_tx(const uint8_t dst_mac[6], uint16_t ethertype,
                  const uint8_t *payload, uint32_t plen) {
    if (plen > ETH_MTU) {
        return -EMSGSIZE;
    }
    uint8_t frame[ETH_FRAME_MAX];
    memcpy(frame, dst_mac, 6u);
    memcpy(frame + 6u, g_mac, 6u);
    net_wr_be16(frame + 12u, ethertype);
    memcpy(frame + ETH_HDR_LEN, payload, plen);
    uint32_t flen = ETH_HDR_LEN + plen;
    if (flen < 60u) {
        memset(frame + flen, 0, 60u - flen);
        flen = 60u;
    }
    return virtio_net_tx(frame, flen);
}

static const uint8_t *arp_lookup(const uint8_t ip4[4]) {
    for (uint32_t i = 0; i < ARP_TABLE_MAX; i++) {
        if (g_arp[i].valid && memcmp(g_arp[i].ip4, ip4, 4u) == 0) {
            return g_arp[i].mac;
        }
    }
    return NULL;
}

static void arp_send_packet(uint16_t op, const uint8_t target_mac[6],
                            const uint8_t target_ip[4]) {
    if (!g_nic_ready) {
        return;
    }
    uint8_t arp[28];
    net_wr_be16(arp + 0u, 1u);            /* HTYPE = Ethernet */
    net_wr_be16(arp + 2u, ETH_TYPE_IPV4); /* PTYPE = IPv4 */
    arp[4] = 6u;                          /* HLEN */
    arp[5] = 4u;                          /* PLEN */
    net_wr_be16(arp + 6u, op);            /* OPER */
    memcpy(arp + 8u, g_mac, 6u);          /* SHA */
    memcpy(arp + 14u, g_ip4, 4u);         /* SPA */
    memcpy(arp + 18u, target_mac, 6u);    /* THA */
    memcpy(arp + 24u, target_ip, 4u);     /* TPA */
    static const uint8_t bcast[6] = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu};
    const uint8_t *eth_dst = (op == 1u) ? bcast : target_mac;
    (void)eth_tx(eth_dst, ETH_TYPE_ARP, arp, sizeof(arp));
}

static void arp_update(const uint8_t ip4[4], const uint8_t mac[6]) {
    uint32_t slot = ARP_TABLE_MAX;
    for (uint32_t i = 0; i < ARP_TABLE_MAX; i++) {
        if (g_arp[i].valid && memcmp(g_arp[i].ip4, ip4, 4u) == 0) {
            memcpy(g_arp[i].mac, mac, 6u);
            slot = i;
            break;
        }
        if (!g_arp[i].valid && slot == ARP_TABLE_MAX) {
            slot = i;
        }
    }
    if (slot == ARP_TABLE_MAX) {
        slot = 0u;
    }
    g_arp[slot].valid = 1u;
    memcpy(g_arp[slot].ip4, ip4, 4u);
    memcpy(g_arp[slot].mac, mac, 6u);

    /* Flush any IPv4 frames waiting on this next-hop MAC. */
    for (uint32_t i = 0; i < ARP_PENDING_MAX; i++) {
        if (g_arp_pend[i].used &&
            memcmp(g_arp_pend[i].next_hop, ip4, 4u) == 0) {
            (void)eth_tx(mac, ETH_TYPE_IPV4, g_arp_pend[i].pkt,
                         g_arp_pend[i].len);
            g_arp_pend[i].used = 0u;
        }
    }

    /* Wake any NET_OP_ARP_RESOLVE waiters for this IPv4 address. */
    for (uint32_t i = 0; i < ARP_WAIT_MAX; i++) {
        if (g_arp_wait[i].used && memcmp(g_arp_wait[i].ip4, ip4, 4u) == 0) {
            net_rep_t rep = {0};
            rep.err = 0;
            memcpy(rep.mac, mac, 6u);
            rep.addr.family = NET_AF_INET;
            memcpy(rep.addr.addr, ip4, 4u);
            uint32_t mid = g_arp_wait[i].msgid;
            g_arp_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        }
    }
}

static void net_rx_arp(const uint8_t *pkt, uint32_t len) {
    if (len < 28u) {
        return;
    }
    uint16_t htype = net_rd_be16(pkt + 0u);
    uint16_t ptype = net_rd_be16(pkt + 2u);
    uint16_t oper = net_rd_be16(pkt + 6u);
    if (htype != 1u || ptype != ETH_TYPE_IPV4 || pkt[4] != 6u || pkt[5] != 4u) {
        return;
    }
    const uint8_t *sha = pkt + 8u;
    const uint8_t *spa = pkt + 14u;
    const uint8_t *tpa = pkt + 24u;
    arp_update(spa, sha);
    if (oper == 1u && memcmp(tpa, g_ip4, 4u) == 0) {
        arp_send_packet(2u, sha, spa);
    }
}

/* --- Loopback packet queue --- */

static void lo_enqueue(uint16_t ethertype, const uint8_t *pkt, uint32_t len) {
    if (len > ETH_MTU || g_lo_len >= LO_Q_MAX) {
        return;
    }
    uint32_t idx = (g_lo_head + g_lo_len) % LO_Q_MAX;
    g_lo_q[idx].ethertype = ethertype;
    g_lo_q[idx].len = (uint16_t)len;
    memcpy(g_lo_q[idx].pkt, pkt, len);
    g_lo_len++;
}

static void drain_loopback(void) {
    if (g_draining_lo) {
        return;
    }
    g_draining_lo = 1;
    while (g_lo_len > 0u) {
        lo_pkt_t item = g_lo_q[g_lo_head];
        g_lo_head = (g_lo_head + 1u) % LO_Q_MAX;
        g_lo_len--;
        if (item.ethertype == ETH_TYPE_IPV4) {
            net_rx_ipv4(item.pkt, item.len);
        } else if (item.ethertype == ETH_TYPE_IPV6) {
            net_rx_ipv6(item.pkt, item.len);
        }
    }
    g_draining_lo = 0;
    wake_all_waiters();
}

/* --- Socket helpers --- */

static net_sock_t *get_sock(uint32_t h) {
    if (h == 0u || h >= NET_SOCK_MAX || !g_socks[h].used) {
        return NULL;
    }
    return &g_socks[h];
}

static uint32_t alloc_sock(void) {
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        if (!g_socks[i].used) {
            memset(&g_socks[i], 0, sizeof(g_socks[i]));
            g_socks[i].used = 1u;
            g_socks[i].refs = 1u;
            return i;
        }
    }
    return 0u;
}

static int port_in_use(uint16_t family, int32_t type, uint16_t port,
                       const uint8_t ip[16], uint32_t skip_idx) {
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        if (i == skip_idx || !g_socks[i].used || g_socks[i].user_closed ||
            !g_socks[i].bound || g_socks[i].family != family ||
            g_socks[i].type != type || g_socks[i].local.port != port) {
            continue;
        }
        if (g_socks[i].type == NET_SOCK_STREAM &&
            g_socks[i].tcp_state == TCP_TIME_WAIT) {
            continue;
        }
        size_t n = (family == NET_AF_INET6) ? 16u : 4u;
        net_addr_t tmp = {.family = family};
        memcpy(tmp.addr, ip, n);
        if (addr_is_zero(&tmp) || addr_is_zero(&g_socks[i].local) ||
            memcmp(g_socks[i].local.addr, ip, n) == 0) {
            return 1;
        }
    }
    return 0;
}

static uint16_t alloc_ephemeral_port(uint16_t family, int32_t type) {
    uint8_t zero[16] = {0};
    for (uint32_t tries = 0; tries < 16384u; tries++) {
        uint16_t p = g_ephemeral_port++;
        if (g_ephemeral_port < 49152u) {
            g_ephemeral_port = 49152u;
        }
        if (!port_in_use(family, type, p, zero, 0u)) {
            return p;
        }
    }
    return 0u;
}

static void free_sock_slot(uint32_t idx) {
    if (idx == 0u || idx >= NET_SOCK_MAX || !g_socks[idx].used) {
        return;
    }
    net_sock_t *s = &g_socks[idx];
    if (s->close_deadline_ns != 0ULL) {
        s->close_deadline_ns = 0ULL;
        net_tick_ref_dec();
    }
    /* Reap any unaccepted children if a listening socket is freed. */
    while (s->accept_len > 0u) {
        uint32_t child = s->accept_q[s->accept_head];
        s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
        s->accept_len--;
        if (child > 0u && child < NET_SOCK_MAX && g_socks[child].used) {
            free_sock_slot(child);
        }
    }
    memset(s, 0, sizeof(*s));
}

static void tcp_rx_push(net_sock_t *s, const uint8_t *data, uint32_t len) {
    uint32_t avail = TCP_RX_BUF_CAP - s->rx_len;
    if (len > avail) {
        len = avail;
    }
    for (uint32_t i = 0; i < len; i++) {
        uint32_t pos = (s->rx_head + s->rx_len) % TCP_RX_BUF_CAP;
        s->rx_buf[pos] = data[i];
        s->rx_len++;
    }
}

static uint32_t tcp_rx_pop(net_sock_t *s, uint8_t *out, uint32_t max_len,
                           int peek) {
    uint32_t n = (max_len < s->rx_len) ? max_len : s->rx_len;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = s->rx_buf[(s->rx_head + i) % TCP_RX_BUF_CAP];
    }
    if (!peek) {
        s->rx_head = (s->rx_head + n) % TCP_RX_BUF_CAP;
        s->rx_len -= n;
    }
    return n;
}

/* --- TCP transmission & reception --- */

static int tcp_send_seg(const net_addr_t *src, const net_addr_t *dst,
                        uint32_t seq, uint32_t ack, uint8_t flags,
                        uint16_t window, const uint8_t *data, uint32_t dlen) {
    if (dlen > TCP_MSS) {
        return -EMSGSIZE;
    }
    uint8_t seg[20u + TCP_MSS];
    net_wr_be16(seg + 0u, src->port);
    net_wr_be16(seg + 2u, dst->port);
    net_wr_be32(seg + 4u, seq);
    net_wr_be32(seg + 8u, ack);
    seg[12] = 0x50u; /* data offset = 5 (20 bytes) */
    seg[13] = flags;
    net_wr_be16(seg + 14u, window);
    seg[16] = 0u;
    seg[17] = 0u;
    seg[18] = 0u;
    seg[19] = 0u;
    if (dlen > 0u && data) {
        memcpy(seg + 20u, data, dlen);
    }
    uint32_t slen = 20u + dlen;
    if (dst->family == NET_AF_INET6) {
        uint16_t cs =
            csum_v6_pseudo(src->addr, dst->addr, NET_IPPROTO_TCP, seg, slen);
        net_wr_be16(seg + 16u, cs);
        return tx_ipv6(src->addr, dst->addr, NET_IPPROTO_TCP, seg, slen);
    }
    uint16_t cs =
        csum_v4_pseudo(src->addr, dst->addr, NET_IPPROTO_TCP, seg, slen);
    net_wr_be16(seg + 16u, cs);
    return tx_ipv4(dst->addr, NET_IPPROTO_TCP, seg, slen);
}

static uint16_t tcp_window(const net_sock_t *s) {
    uint32_t free_sp =
        (s->rx_len < TCP_RX_BUF_CAP) ? (TCP_RX_BUF_CAP - s->rx_len) : 0u;
    return (uint16_t)(free_sp > 65535u ? 65535u : free_sp);
}

static uint32_t find_tcp_sock(const net_addr_t *src, const net_addr_t *dst) {
    /* 1. Exact 4-tuple match for active/connecting sockets. */
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        net_sock_t *s = &g_socks[i];
        if (!s->used || s->type != NET_SOCK_STREAM ||
            s->family != dst->family || s->tcp_state == TCP_CLOSED ||
            s->tcp_state == TCP_LISTEN) {
            continue;
        }
        if (s->local.port == dst->port && s->peer.port == src->port &&
            addr_ip_eq(&s->peer, src) &&
            (addr_is_zero(&s->local) || addr_ip_eq(&s->local, dst))) {
            return i;
        }
    }
    /* 2. Exact local-IP listening socket match. */
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        net_sock_t *s = &g_socks[i];
        if (!s->used || s->user_closed || s->type != NET_SOCK_STREAM ||
            s->family != dst->family || s->tcp_state != TCP_LISTEN) {
            continue;
        }
        if (s->local.port == dst->port && !addr_is_zero(&s->local) &&
            addr_ip_eq(&s->local, dst)) {
            return i;
        }
    }
    /* 3. Wildcard listening socket match. */
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        net_sock_t *s = &g_socks[i];
        if (!s->used || s->user_closed || s->type != NET_SOCK_STREAM ||
            s->family != dst->family || s->tcp_state != TCP_LISTEN) {
            continue;
        }
        if (s->local.port == dst->port && addr_is_zero(&s->local)) {
            return i;
        }
    }
    return 0u;
}

static void net_rx_tcp(const net_addr_t *src_ip, const net_addr_t *dst_ip,
                       const uint8_t *seg, uint32_t len) {
    if (len < 20u) {
        return;
    }
    uint32_t hdr_len = (uint32_t)(seg[12] >> 4) * 4u;
    if (hdr_len < 20u || hdr_len > len) {
        return;
    }
    uint16_t check = (dst_ip->family == NET_AF_INET6)
                         ? csum_v6_pseudo(src_ip->addr, dst_ip->addr,
                                          NET_IPPROTO_TCP, seg, len)
                         : csum_v4_pseudo(src_ip->addr, dst_ip->addr,
                                          NET_IPPROTO_TCP, seg, len);
    if (check != 0u) {
        return;
    }

    net_addr_t src = *src_ip;
    net_addr_t dst = *dst_ip;
    src.port = net_rd_be16(seg + 0u);
    dst.port = net_rd_be16(seg + 2u);
    uint32_t seq = net_rd_be32(seg + 4u);
    uint32_t ack = net_rd_be32(seg + 8u);
    uint8_t flags = seg[13];
    const uint8_t *payload = seg + hdr_len;
    uint32_t plen = len - hdr_len;

    uint32_t sidx = find_tcp_sock(&src, &dst);
    if (sidx == 0u) {
        if ((flags & TCP_FLAG_RST) == 0u) {
            uint32_t rst_seq = (flags & TCP_FLAG_ACK) ? ack : 0u;
            uint32_t rst_ack = seq + plen + ((flags & TCP_FLAG_SYN) ? 1u : 0u) +
                               ((flags & TCP_FLAG_FIN) ? 1u : 0u);
            uint8_t rf = (flags & TCP_FLAG_ACK) ? TCP_FLAG_RST
                                                : (TCP_FLAG_RST | TCP_FLAG_ACK);
            (void)tcp_send_seg(&dst, &src, rst_seq, rst_ack, rf, 0u, NULL, 0u);
        }
        return;
    }

    net_sock_t *s = &g_socks[sidx];
    if ((flags & TCP_FLAG_RST) != 0u) {
        s->so_error = ECONNREFUSED;
        if (s->tcp_state == TCP_ESTABLISHED) {
            s->so_error = ECONNRESET;
        }
        s->peer_fin = 1u;
        s->tcp_state = TCP_CLOSED;
        if (s->user_closed) {
            free_sock_slot(sidx);
        }
        return;
    }

    if (s->tcp_state == TCP_LISTEN) {
        if ((flags & TCP_FLAG_SYN) == 0u || (flags & TCP_FLAG_ACK) != 0u) {
            return;
        }
        if (s->accept_len >= ACCEPT_Q_MAX || s->accept_len >= s->backlog) {
            return;
        }
        uint32_t cidx = alloc_sock();
        if (cidx == 0u) {
            return;
        }
        net_sock_t *c = &g_socks[cidx];
        c->family = s->family;
        c->type = NET_SOCK_STREAM;
        c->proto = NET_IPPROTO_TCP;
        c->bound = 1u;
        c->connected = 1u;
        c->parent_idx = sidx;
        c->local = dst;
        c->peer = src;
        c->tcp_state = TCP_SYN_RCVD;
        c->rcv_nxt = seq + 1u;
        c->snd_una = g_iss_seed;
        g_iss_seed += 64000u;
        c->snd_nxt = c->snd_una + 1u;
        (void)tcp_send_seg(&c->local, &c->peer, c->snd_una, c->rcv_nxt,
                           TCP_FLAG_SYN | TCP_FLAG_ACK, tcp_window(c), NULL,
                           0u);
        return;
    }

    if (s->tcp_state == TCP_SYN_SENT) {
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
                (TCP_FLAG_SYN | TCP_FLAG_ACK) &&
            ack == s->snd_nxt) {
            s->snd_una = ack;
            s->rcv_nxt = seq + 1u;
            s->tcp_state = TCP_ESTABLISHED;
            s->connected = 1u;
            (void)tcp_send_seg(&s->local, &s->peer, s->snd_nxt, s->rcv_nxt,
                               TCP_FLAG_ACK, tcp_window(s), NULL, 0u);
        }
        return;
    }

    if (s->tcp_state == TCP_SYN_RCVD) {
        if ((flags & TCP_FLAG_SYN) != 0u && (flags & TCP_FLAG_ACK) == 0u) {
            (void)tcp_send_seg(&s->local, &s->peer, s->snd_una, s->rcv_nxt,
                               TCP_FLAG_SYN | TCP_FLAG_ACK, tcp_window(s), NULL,
                               0u);
            return;
        }
        if ((flags & TCP_FLAG_ACK) != 0u && ack == s->snd_nxt) {
            s->snd_una = ack;
            s->tcp_state = TCP_ESTABLISHED;
            net_sock_t *parent = get_sock(s->parent_idx);
            if (parent && parent->tcp_state == TCP_LISTEN &&
                parent->accept_len < ACCEPT_Q_MAX) {
                uint32_t qpos =
                    (parent->accept_head + parent->accept_len) % ACCEPT_Q_MAX;
                parent->accept_q[qpos] = sidx;
                parent->accept_len++;
            } else {
                free_sock_slot(sidx);
                return;
            }
            if (plen > 0u && seq == s->rcv_nxt) {
                tcp_rx_push(s, payload, plen);
                s->rcv_nxt += plen;
                (void)tcp_send_seg(&s->local, &s->peer, s->snd_nxt, s->rcv_nxt,
                                   TCP_FLAG_ACK, tcp_window(s), NULL, 0u);
            }
        }
        return;
    }

    /* Update ACK tracking in established / closing states. */
    if ((flags & TCP_FLAG_ACK) != 0u) {
        s->snd_una = ack;
        if (s->tcp_state == TCP_FIN_WAIT_1 && ack == s->snd_nxt) {
            s->tcp_state = TCP_FIN_WAIT_2;
        } else if (s->tcp_state == TCP_LAST_ACK && ack == s->snd_nxt) {
            s->tcp_state = TCP_CLOSED;
            if (s->user_closed) {
                free_sock_slot(sidx);
                return;
            }
        }
    }

    int need_ack = 0;
    if (plen > 0u) {
        if (s->shut_rd) {
            if (seq == s->rcv_nxt) {
                s->rcv_nxt += plen;
            }
            need_ack = 1;
        } else if (seq == s->rcv_nxt) {
            uint32_t avail = (s->rx_len < TCP_RX_BUF_CAP)
                                 ? (TCP_RX_BUF_CAP - s->rx_len)
                                 : 0u;
            uint32_t take = (plen < avail) ? plen : avail;
            if (take > 0u) {
                tcp_rx_push(s, payload, take);
                s->rcv_nxt += take;
            }
            need_ack = 1;
        } else {
            need_ack = 1;
        }
    }

    if ((flags & TCP_FLAG_FIN) != 0u) {
        if (seq + plen == s->rcv_nxt) {
            s->rcv_nxt++;
        }
        s->peer_fin = 1u;
        need_ack = 1;
        if (s->tcp_state == TCP_ESTABLISHED) {
            s->tcp_state = TCP_CLOSE_WAIT;
        } else if (s->tcp_state == TCP_FIN_WAIT_1 ||
                   s->tcp_state == TCP_FIN_WAIT_2) {
            s->tcp_state = TCP_TIME_WAIT;
        }
    }

    if (need_ack) {
        (void)tcp_send_seg(&s->local, &s->peer, s->snd_nxt, s->rcv_nxt,
                           TCP_FLAG_ACK, tcp_window(s), NULL, 0u);
    }

    if (s->user_closed &&
        (s->tcp_state == TCP_TIME_WAIT || s->tcp_state == TCP_CLOSED)) {
        free_sock_slot(sidx);
    }
}

/* --- UDP transmission & reception --- */

static int udp_send_pkt(const net_addr_t *src, const net_addr_t *dst,
                        const uint8_t *data, uint32_t dlen) {
    if (dlen > NET_DATA_MAX) {
        return -EMSGSIZE;
    }
    uint8_t seg[8u + NET_DATA_MAX];
    uint32_t ulen = 8u + dlen;
    net_wr_be16(seg + 0u, src->port);
    net_wr_be16(seg + 2u, dst->port);
    net_wr_be16(seg + 4u, (uint16_t)ulen);
    seg[6] = 0u;
    seg[7] = 0u;
    if (dlen > 0u && data) {
        memcpy(seg + 8u, data, dlen);
    }
    uint16_t cs =
        (dst->family == NET_AF_INET6)
            ? csum_v6_pseudo(src->addr, dst->addr, NET_IPPROTO_UDP, seg, ulen)
            : csum_v4_pseudo(src->addr, dst->addr, NET_IPPROTO_UDP, seg, ulen);
    if (cs == 0u) {
        cs = 0xFFFFu;
    }
    net_wr_be16(seg + 6u, cs);
    if (dst->family == NET_AF_INET6) {
        return tx_ipv6(src->addr, dst->addr, NET_IPPROTO_UDP, seg, ulen);
    }
    return tx_ipv4(dst->addr, NET_IPPROTO_UDP, seg, ulen);
}

static void net_rx_udp(const net_addr_t *src_ip, const net_addr_t *dst_ip,
                       const uint8_t *seg, uint32_t len) {
    if (len < 8u) {
        return;
    }
    uint16_t ulen = net_rd_be16(seg + 4u);
    if (ulen < 8u || ulen > len) {
        return;
    }
    uint16_t cs_hdr = net_rd_be16(seg + 6u);
    if (cs_hdr != 0u || dst_ip->family == NET_AF_INET6) {
        uint16_t check = (dst_ip->family == NET_AF_INET6)
                             ? csum_v6_pseudo(src_ip->addr, dst_ip->addr,
                                              NET_IPPROTO_UDP, seg, ulen)
                             : csum_v4_pseudo(src_ip->addr, dst_ip->addr,
                                              NET_IPPROTO_UDP, seg, ulen);
        if (check != 0u) {
            return;
        }
    }

    net_addr_t sender = *src_ip;
    sender.port = net_rd_be16(seg + 0u);
    uint16_t dport = net_rd_be16(seg + 2u);
    uint32_t dlen = (uint32_t)ulen - 8u;
    if (dlen > NET_DATA_MAX) {
        dlen = NET_DATA_MAX;
    }

    uint32_t target = 0u;
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        net_sock_t *s = &g_socks[i];
        if (!s->used || s->user_closed || s->type != NET_SOCK_DGRAM ||
            !s->bound || s->family != dst_ip->family ||
            s->local.port != dport) {
            continue;
        }
        if (!addr_is_zero(&s->local) && addr_ip_eq(&s->local, dst_ip)) {
            target = i;
            break;
        }
        if (addr_is_zero(&s->local) && target == 0u) {
            target = i;
        }
    }
    if (target == 0u) {
        return;
    }
    net_sock_t *s = &g_socks[target];
    if (s->shut_rd || s->dg_len >= UDP_Q_MAX) {
        return;
    }
    uint32_t pos = (s->dg_head + s->dg_len) % UDP_Q_MAX;
    s->dg_q[pos].len = (uint16_t)dlen;
    s->dg_q[pos].sender = sender;
    if (dlen > 0u) {
        memcpy(s->dg_q[pos].data, seg + 8u, dlen);
    }
    s->dg_len++;
}

/* --- ICMPv4 & ICMPv6 --- */

static void complete_ping_waiter(uint16_t family, uint16_t id, uint16_t seq,
                                 const net_addr_t *src) {
    uint64_t now = net_now_mono_ns();
    for (uint32_t i = 0; i < PING_WAIT_MAX; i++) {
        if (g_ping_wait[i].used && g_ping_wait[i].family == family &&
            g_ping_wait[i].id == id && g_ping_wait[i].seq == seq) {
            net_rep_t rep = {0};
            rep.err = 0;
            uint64_t dt_ms =
                (now > g_ping_wait[i].start_ns)
                    ? ((now - g_ping_wait[i].start_ns) / 1000000ULL)
                    : 0ULL;
            rep.len = (uint32_t)dt_ms;
            rep.flags = seq;
            rep.addr = *src;
            uint32_t mid = g_ping_wait[i].msgid;
            g_ping_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
            return;
        }
    }
}

static void net_rx_icmp4(const uint8_t src_ip[4], const uint8_t *pkt,
                         uint32_t len) {
    if (len < 8u || csum_buf(pkt, len) != 0u) {
        return;
    }
    uint8_t type = pkt[0];
    uint16_t id = net_rd_be16(pkt + 4u);
    uint16_t seq = net_rd_be16(pkt + 6u);
    if (type == 8u && pkt[1] == 0u) {
        /* Echo Request -> Echo Reply. */
        uint8_t rep[ETH_MTU];
        uint32_t rlen = len > (ETH_MTU - 20u) ? (ETH_MTU - 20u) : len;
        memcpy(rep, pkt, rlen);
        rep[0] = 0u;
        rep[2] = 0u;
        rep[3] = 0u;
        net_wr_be16(rep + 2u, csum_buf(rep, rlen));
        (void)tx_ipv4(src_ip, NET_IPPROTO_ICMP, rep, rlen);
    } else if (type == 0u && pkt[1] == 0u) {
        net_addr_t src = {.family = NET_AF_INET};
        memcpy(src.addr, src_ip, 4u);
        complete_ping_waiter(NET_AF_INET, id, seq, &src);
    }
}

static void net_rx_icmp6(const uint8_t src_ip[16], const uint8_t dst_ip[16],
                         const uint8_t *pkt, uint32_t len) {
    if (len < 8u) {
        return;
    }
    if (csum_v6_pseudo(src_ip, dst_ip, NET_IPPROTO_ICMPV6, pkt, len) != 0u) {
        return;
    }
    uint8_t type = pkt[0];
    uint16_t id = net_rd_be16(pkt + 4u);
    uint16_t seq = net_rd_be16(pkt + 6u);
    if (type == 128u && pkt[1] == 0u) {
        /* ICMPv6 Echo Request -> Echo Reply. */
        uint8_t rep[ETH_MTU];
        uint32_t rlen = len > (ETH_MTU - 40u) ? (ETH_MTU - 40u) : len;
        memcpy(rep, pkt, rlen);
        rep[0] = 129u;
        rep[2] = 0u;
        rep[3] = 0u;
        uint16_t cs =
            csum_v6_pseudo(dst_ip, src_ip, NET_IPPROTO_ICMPV6, rep, rlen);
        net_wr_be16(rep + 2u, cs);
        (void)tx_ipv6(dst_ip, src_ip, NET_IPPROTO_ICMPV6, rep, rlen);
    } else if (type == 129u && pkt[1] == 0u) {
        net_addr_t src = {.family = NET_AF_INET6};
        memcpy(src.addr, src_ip, 16u);
        complete_ping_waiter(NET_AF_INET6, id, seq, &src);
    } else if (type == 135u && len >= 24u &&
               memcmp(pkt + 8u, g_ip6, 16u) == 0) {
        /* Neighbor Solicitation for our link-local IPv6 -> send NA. */
        uint8_t na[32];
        memset(na, 0, sizeof(na));
        na[0] = 136u;
        na[4] = 0x60u; /* Solicited + Override */
        memcpy(na + 8u, g_ip6, 16u);
        na[24] = 2u; /* Target Link-Layer Address option */
        na[25] = 1u; /* length = 8 bytes */
        memcpy(na + 26u, g_mac, 6u);
        uint16_t cs =
            csum_v6_pseudo(g_ip6, src_ip, NET_IPPROTO_ICMPV6, na, sizeof(na));
        net_wr_be16(na + 2u, cs);
        (void)tx_ipv6(g_ip6, src_ip, NET_IPPROTO_ICMPV6, na, sizeof(na));
    }
}

/* --- IPv4 & IPv6 layer --- */

static int tx_ipv4(const uint8_t dst[4], uint8_t proto, const uint8_t *payload,
                   uint32_t plen) {
    if (plen > ETH_MTU - 20u) {
        return -EMSGSIZE;
    }
    uint8_t pkt[ETH_MTU];
    uint32_t total = 20u + plen;
    const uint8_t *src = (dst[0] == 127u) ? g_lo4 : g_ip4;

    pkt[0] = 0x45u;
    pkt[1] = 0u;
    net_wr_be16(pkt + 2u, (uint16_t)total);
    net_wr_be16(pkt + 4u, g_ip4_id++);
    net_wr_be16(pkt + 6u, 0x4000u); /* DF */
    pkt[8] = 64u;                   /* TTL */
    pkt[9] = proto;
    pkt[10] = 0u;
    pkt[11] = 0u;
    memcpy(pkt + 12u, src, 4u);
    memcpy(pkt + 16u, dst, 4u);
    net_wr_be16(pkt + 10u, csum_buf(pkt, 20u));
    memcpy(pkt + 20u, payload, plen);

    if (ip4_is_local(dst)) {
        lo_enqueue(ETH_TYPE_IPV4, pkt, total);
        return 0;
    }
    if (!g_nic_ready) {
        return -ENETUNREACH;
    }

    uint8_t nh[4];
    int same_sub = 1;
    for (int i = 0; i < 4; i++) {
        if ((dst[i] & g_mask4[i]) != (g_ip4[i] & g_mask4[i])) {
            same_sub = 0;
        }
    }
    memcpy(nh, same_sub ? dst : g_gw4, 4u);

    const uint8_t *dmac = arp_lookup(nh);
    if (dmac) {
        return eth_tx(dmac, ETH_TYPE_IPV4, pkt, total);
    }

    /* Queue packet pending ARP resolution and send ARP request. */
    for (uint32_t i = 0; i < ARP_PENDING_MAX; i++) {
        if (!g_arp_pend[i].used) {
            g_arp_pend[i].used = 1u;
            memcpy(g_arp_pend[i].next_hop, nh, 4u);
            g_arp_pend[i].len = (uint16_t)total;
            memcpy(g_arp_pend[i].pkt, pkt, total);
            break;
        }
    }
    static const uint8_t zero_mac[6] = {0};
    arp_send_packet(1u, zero_mac, nh);
    return 0;
}

static int tx_ipv6(const uint8_t src[16], const uint8_t dst[16],
                   uint8_t next_hdr, const uint8_t *payload, uint32_t plen) {
    if (plen > ETH_MTU - 40u) {
        return -EMSGSIZE;
    }
    uint8_t pkt[ETH_MTU];
    pkt[0] = 0x60u;
    pkt[1] = 0u;
    pkt[2] = 0u;
    pkt[3] = 0u;
    net_wr_be16(pkt + 4u, (uint16_t)plen);
    pkt[6] = next_hdr;
    pkt[7] = 64u; /* Hop Limit */
    memcpy(pkt + 8u, src, 16u);
    memcpy(pkt + 24u, dst, 16u);
    memcpy(pkt + 40u, payload, plen);

    if (ip6_is_local(dst)) {
        lo_enqueue(ETH_TYPE_IPV6, pkt, 40u + plen);
        return 0;
    }
    if (!g_nic_ready) {
        return -ENETUNREACH;
    }
    uint8_t dmac[6] = {0x33u, 0x33u, dst[12], dst[13], dst[14], dst[15]};
    if (dst[0] != 0xFFu) {
        const uint8_t *gw_mac = arp_lookup(g_gw4);
        if (gw_mac) {
            memcpy(dmac, gw_mac, 6u);
        } else {
            memset(dmac, 0xFFu, 6u);
        }
    }
    return eth_tx(dmac, ETH_TYPE_IPV6, pkt, 40u + plen);
}

static void net_rx_ipv4(const uint8_t *pkt, uint32_t len) {
    if (len < 20u || (pkt[0] >> 4) != 4u) {
        return;
    }
    uint32_t ihl = (uint32_t)(pkt[0] & 0x0Fu) * 4u;
    uint16_t total = net_rd_be16(pkt + 2u);
    if (ihl < 20u || total < ihl || total > len) {
        return;
    }
    if (csum_buf(pkt, ihl) != 0u) {
        return;
    }
    const uint8_t *src_ip = pkt + 12u;
    const uint8_t *dst_ip = pkt + 16u;
    if (!ip4_is_local(dst_ip)) {
        return;
    }
    uint8_t proto = pkt[9];
    const uint8_t *payload = pkt + ihl;
    uint32_t plen = (uint32_t)total - ihl;

    net_addr_t src = {.family = NET_AF_INET};
    net_addr_t dst = {.family = NET_AF_INET};
    memcpy(src.addr, src_ip, 4u);
    memcpy(dst.addr, dst_ip, 4u);

    if (proto == NET_IPPROTO_ICMP) {
        net_rx_icmp4(src_ip, payload, plen);
    } else if (proto == NET_IPPROTO_UDP) {
        net_rx_udp(&src, &dst, payload, plen);
    } else if (proto == NET_IPPROTO_TCP) {
        net_rx_tcp(&src, &dst, payload, plen);
    }
}

static void net_rx_ipv6(const uint8_t *pkt, uint32_t len) {
    if (len < 40u || (pkt[0] >> 4) != 6u) {
        return;
    }
    uint16_t plen = net_rd_be16(pkt + 4u);
    if (40u + (uint32_t)plen > len) {
        return;
    }
    uint8_t next_hdr = pkt[6];
    const uint8_t *src_ip = pkt + 8u;
    const uint8_t *dst_ip = pkt + 24u;
    if (!ip6_is_local(dst_ip) && dst_ip[0] != 0xFFu) {
        return;
    }
    const uint8_t *payload = pkt + 40u;

    net_addr_t src = {.family = NET_AF_INET6};
    net_addr_t dst = {.family = NET_AF_INET6};
    memcpy(src.addr, src_ip, 16u);
    memcpy(dst.addr, dst_ip, 16u);

    if (next_hdr == NET_IPPROTO_ICMPV6) {
        net_rx_icmp6(src_ip, dst_ip, payload, plen);
    } else if (next_hdr == NET_IPPROTO_UDP) {
        net_rx_udp(&src, &dst, payload, plen);
    } else if (next_hdr == NET_IPPROTO_TCP) {
        net_rx_tcp(&src, &dst, payload, plen);
    }
}

void net_rx_eth(const uint8_t *frame, uint32_t len) {
    if (!frame || len < ETH_HDR_LEN) {
        return;
    }
    uint16_t etype = net_rd_be16(frame + 12u);
    const uint8_t *payload = frame + ETH_HDR_LEN;
    uint32_t plen = len - ETH_HDR_LEN;
    if (etype == ETH_TYPE_ARP) {
        net_rx_arp(payload, plen);
    } else if (etype == ETH_TYPE_IPV4) {
        if (plen >= 20u && (payload[0] >> 4) == 4u) {
            const uint8_t *src_ip = payload + 12u;
            int same_sub = 1;
            for (int i = 0; i < 4; i++) {
                if ((src_ip[i] & g_mask4[i]) != (g_ip4[i] & g_mask4[i])) {
                    same_sub = 0;
                }
            }
            if (same_sub && !ip4_is_local(src_ip) && !ip4_is_zero(src_ip)) {
                arp_update(src_ip, frame + 6u);
            }
        }
        net_rx_ipv4(payload, plen);
    } else if (etype == ETH_TYPE_IPV6) {
        net_rx_ipv6(payload, plen);
    }
    drain_loopback();
    wake_all_waiters();
}

/* --- Poll evaluation & waiter wakeup --- */

static int16_t eval_sock_poll(uint32_t handle, int16_t events) {
    net_sock_t *s = get_sock(handle);
    if (!s || s->user_closed) {
        return (int16_t)POLLNVAL;
    }
    int16_t rev = 0;
    if (s->so_error != 0) {
        rev |= (int16_t)POLLERR;
    }
    if (s->type == NET_SOCK_DGRAM) {
        if ((events & POLLIN) != 0 && (s->dg_len > 0u || s->shut_rd)) {
            rev |= (int16_t)POLLIN;
        }
        if ((events & POLLOUT) != 0 && !s->shut_wr) {
            rev |= (int16_t)POLLOUT;
        }
        return rev;
    }
    if (s->tcp_state == TCP_LISTEN) {
        if ((events & POLLIN) != 0 && s->accept_len > 0u) {
            rev |= (int16_t)POLLIN;
        }
        return rev;
    }
    if ((events & POLLIN) != 0 &&
        (s->rx_len > 0u || s->peer_fin || s->shut_rd)) {
        rev |= (int16_t)POLLIN;
    }
    if ((events & POLLOUT) != 0 && s->tcp_state == TCP_ESTABLISHED &&
        !s->shut_wr) {
        rev |= (int16_t)POLLOUT;
    }
    if (s->peer_fin && s->rx_len == 0u) {
        rev |= (int16_t)POLLHUP;
    }
    return rev;
}

static void wake_all_waiters(void) {
    /* 1. Connect waiters. */
    for (uint32_t i = 0; i < CONN_WAIT_MAX; i++) {
        if (!g_conn_wait[i].used) {
            continue;
        }
        net_sock_t *s = get_sock(g_conn_wait[i].sock_idx);
        if (!s || s->user_closed) {
            net_rep_t rep = {.err = -EBADF};
            uint32_t mid = g_conn_wait[i].msgid;
            g_conn_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
            continue;
        }
        if (s->tcp_state == TCP_ESTABLISHED || s->tcp_state == TCP_CLOSED) {
            net_rep_t rep = {0};
            if (s->tcp_state == TCP_ESTABLISHED) {
                rep.err = 0;
            } else {
                rep.err = (s->so_error != 0) ? -s->so_error : -ECONNREFUSED;
                s->so_error = 0;
            }
            uint32_t mid = g_conn_wait[i].msgid;
            g_conn_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        }
    }

    /* 2. Accept waiters. */
    for (uint32_t i = 0; i < ACC_WAIT_MAX; i++) {
        if (!g_acc_wait[i].used) {
            continue;
        }
        net_sock_t *s = get_sock(g_acc_wait[i].sock_idx);
        if (!s || s->user_closed || s->tcp_state != TCP_LISTEN) {
            net_rep_t rep = {.err = -EINVAL};
            uint32_t mid = g_acc_wait[i].msgid;
            g_acc_wait[i].used = 0u;
            net_reply(mid, &rep, 0u);
            continue;
        }
        if (s->accept_len > 0u) {
            uint32_t cidx = s->accept_q[s->accept_head];
            s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
            s->accept_len--;
            net_rep_t rep = {0};
            net_sock_t *c = get_sock(cidx);
            if (c) {
                rep.err = 0;
                rep.handle = cidx;
                rep.addr = c->peer;
            } else {
                rep.err = -ECONNABORTED;
            }
            uint32_t mid = g_acc_wait[i].msgid;
            g_acc_wait[i].used = 0u;
            net_reply(mid, &rep, 0u);
        }
    }

    /* 3. Recv waiters. */
    for (uint32_t i = 0; i < RECV_WAIT_MAX; i++) {
        if (!g_recv_wait[i].used) {
            continue;
        }
        net_sock_t *s = get_sock(g_recv_wait[i].sock_idx);
        if (!s || s->user_closed) {
            net_rep_t rep = {.err = -EBADF};
            uint32_t mid = g_recv_wait[i].msgid;
            g_recv_wait[i].used = 0u;
            net_reply(mid, &rep, 0u);
            continue;
        }
        int peek = (g_recv_wait[i].flags & MSG_PEEK) != 0u;
        uint32_t want = g_recv_wait[i].want_len;
        if (want > NET_DATA_MAX) {
            want = NET_DATA_MAX;
        }
        if (s->type == NET_SOCK_DGRAM) {
            if (s->dg_len > 0u) {
                static net_rep_t rep;
                memset(&rep, 0, offsetof(net_rep_t, data));
                udp_pkt_t *p = &s->dg_q[s->dg_head];
                uint32_t n = (want < p->len) ? want : p->len;
                if (n > 0u) {
                    memcpy(rep.data, p->data, n);
                }
                rep.err = 0;
                rep.len = n;
                rep.addr = p->sender;
                if (!peek) {
                    s->dg_head = (s->dg_head + 1u) % UDP_Q_MAX;
                    s->dg_len--;
                }
                uint32_t mid = g_recv_wait[i].msgid;
                g_recv_wait[i].used = 0u;
                net_reply(mid, &rep, n);
            } else if (s->shut_rd) {
                net_rep_t rep = {.err = 0, .len = 0u};
                uint32_t mid = g_recv_wait[i].msgid;
                g_recv_wait[i].used = 0u;
                net_reply(mid, &rep, 0u);
            }
        } else {
            if (s->rx_len > 0u) {
                static net_rep_t rep;
                memset(&rep, 0, offsetof(net_rep_t, data));
                uint32_t n = tcp_rx_pop(s, rep.data, want, peek);
                rep.err = 0;
                rep.len = n;
                rep.addr = s->peer;
                uint32_t mid = g_recv_wait[i].msgid;
                g_recv_wait[i].used = 0u;
                net_reply(mid, &rep, n);
            } else if (s->peer_fin || s->shut_rd ||
                       s->tcp_state == TCP_CLOSED) {
                net_rep_t rep = {0};
                rep.err = (s->so_error != 0) ? -s->so_error : 0;
                s->so_error = 0;
                rep.len = 0u;
                uint32_t mid = g_recv_wait[i].msgid;
                g_recv_wait[i].used = 0u;
                net_reply(mid, &rep, 0u);
            }
        }
    }

    /* 4. Poll waiters. */
    for (uint32_t i = 0; i < POLL_WAIT_MAX; i++) {
        if (!g_poll_wait[i].used) {
            continue;
        }
        net_rep_t rep = {0};
        uint32_t ready = 0u;
        for (uint32_t j = 0; j < g_poll_wait[i].nfds; j++) {
            rep.u.poll_items[j] = g_poll_wait[i].items[j];
            int16_t rev = eval_sock_poll(g_poll_wait[i].items[j].handle,
                                         g_poll_wait[i].items[j].events);
            rep.u.poll_items[j].revents = rev;
            if (rev != 0) {
                ready++;
            }
        }
        if (ready > 0u) {
            rep.err = 0;
            rep.handle = ready;
            uint32_t mid = g_poll_wait[i].msgid;
            if (g_poll_wait[i].has_deadline) {
                net_tick_ref_dec();
            }
            g_poll_wait[i].used = 0u;
            net_reply(mid, &rep, 0u);
        }
    }
}

void net_on_tick(uint64_t now_ns) {
    /* 1. ARP timeouts & retries. */
    for (uint32_t i = 0; i < ARP_WAIT_MAX; i++) {
        if (!g_arp_wait[i].used) {
            continue;
        }
        if (now_ns >= g_arp_wait[i].deadline_ns) {
            net_rep_t rep = {.err = -ETIMEDOUT};
            uint32_t mid = g_arp_wait[i].msgid;
            g_arp_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        } else if (now_ns >= g_arp_wait[i].next_retry_ns) {
            static const uint8_t zero_mac[6] = {0};
            arp_send_packet(1u, zero_mac, g_arp_wait[i].ip4);
            g_arp_wait[i].next_retry_ns = now_ns + 250000000ULL;
        }
    }

    /* 2. Ping timeouts. */
    for (uint32_t i = 0; i < PING_WAIT_MAX; i++) {
        if (!g_ping_wait[i].used) {
            continue;
        }
        if (now_ns >= g_ping_wait[i].deadline_ns) {
            net_rep_t rep = {.err = -ETIMEDOUT};
            uint32_t mid = g_ping_wait[i].msgid;
            g_ping_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        }
    }

    /* 3. TCP SYN retries & connect timeouts. */
    for (uint32_t i = 0; i < CONN_WAIT_MAX; i++) {
        if (!g_conn_wait[i].used) {
            continue;
        }
        net_sock_t *s = get_sock(g_conn_wait[i].sock_idx);
        if (!s || now_ns >= g_conn_wait[i].deadline_ns) {
            if (s && s->tcp_state == TCP_SYN_SENT) {
                s->tcp_state = TCP_CLOSED;
            }
            net_rep_t rep = {.err = -ETIMEDOUT};
            uint32_t mid = g_conn_wait[i].msgid;
            g_conn_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        } else if (s->tcp_state == TCP_SYN_SENT &&
                   now_ns >= g_conn_wait[i].next_retry_ns) {
            (void)tcp_send_seg(&s->local, &s->peer, s->snd_una, 0u,
                               TCP_FLAG_SYN, tcp_window(s), NULL, 0u);
            g_conn_wait[i].next_retry_ns = now_ns + 250000000ULL;
        }
    }

    /* 4. Timed poll deadlines. */
    for (uint32_t i = 0; i < POLL_WAIT_MAX; i++) {
        if (!g_poll_wait[i].used || !g_poll_wait[i].has_deadline) {
            continue;
        }
        if (now_ns >= g_poll_wait[i].deadline_ns) {
            net_rep_t rep = {0};
            rep.err = 0;
            rep.handle = 0u;
            for (uint32_t j = 0; j < g_poll_wait[i].nfds; j++) {
                rep.u.poll_items[j] = g_poll_wait[i].items[j];
                rep.u.poll_items[j].revents = 0;
            }
            uint32_t mid = g_poll_wait[i].msgid;
            g_poll_wait[i].used = 0u;
            net_tick_ref_dec();
            net_reply(mid, &rep, 0u);
        }
    }

    /* 5. Reap lingering closed TCP sockets past their short close timeout. */
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        if (g_socks[i].used && g_socks[i].user_closed &&
            g_socks[i].close_deadline_ns != 0ULL &&
            now_ns >= g_socks[i].close_deadline_ns) {
            free_sock_slot(i);
        }
    }

    drain_loopback();
    wake_all_waiters();
}

/* --- Close / FD cleanup --- */

static void close_socket_handle(uint32_t handle) {
    net_sock_t *s = get_sock(handle);
    if (!s || s->user_closed) {
        return;
    }
    if (s->refs > 1u) {
        s->refs--;
        return;
    }
    s->refs = 0u;
    s->user_closed = 1u;
    s->shut_rd = 1u;

    if (s->type == NET_SOCK_STREAM &&
        (s->tcp_state == TCP_ESTABLISHED || s->tcp_state == TCP_CLOSE_WAIT)) {
        if (!s->shut_wr) {
            s->shut_wr = 1u;
            uint32_t fseq = s->snd_nxt++;
            s->tcp_state = (s->tcp_state == TCP_ESTABLISHED) ? TCP_FIN_WAIT_1
                                                             : TCP_LAST_ACK;
            (void)tcp_send_seg(&s->local, &s->peer, fseq, s->rcv_nxt,
                               TCP_FLAG_FIN | TCP_FLAG_ACK, tcp_window(s), NULL,
                               0u);
            drain_loopback();
        }
    }

    if (g_socks[handle].used) {
        if (s->type == NET_SOCK_DGRAM || s->tcp_state == TCP_CLOSED ||
            s->tcp_state == TCP_LISTEN || s->tcp_state == TCP_SYN_SENT ||
            s->tcp_state == TCP_TIME_WAIT) {
            free_sock_slot(handle);
        } else if (s->close_deadline_ns == 0ULL) {
            s->close_deadline_ns = net_now_mono_ns() + 100000000ULL;
            net_tick_ref_inc();
        }
    }
    wake_all_waiters();
}

void net_handle_fd_close(uint32_t handle) {
    close_socket_handle(handle);
}

/* --- IPC request dispatch --- */

static void handle_info(uint32_t msgid) {
    net_rep_t rep = {0};
    rep.err = 0;
    net_info_t *info = &rep.u.info;
    info->nic_present = g_nic_ready ? 1u : 0u;
    info->if_count = g_nic_ready ? 2u : 1u;
    virtio_net_get_counters(&info->irq_count, &info->rx_packets,
                            &info->tx_packets, &info->rx_bytes,
                            &info->tx_bytes);

    /* Interface 0: lo0 */
    net_if_info_t *lo = &info->ifs[0];
    lo->index = 1u;
    lo->mtu = 16384u;
    lo->flags = NET_IFF_UP | NET_IFF_LOOPBACK | NET_IFF_RUNNING;
    memcpy(lo->name, "lo0", 4u);
    memcpy(lo->ipv4, g_lo4, 4u);
    lo->netmask4[0] = 255u;
    memcpy(lo->ipv6, g_lo6, 16u);

    /* Interface 1: eth0 */
    if (g_nic_ready) {
        net_if_info_t *eth = &info->ifs[1];
        eth->index = 2u;
        eth->mtu = ETH_MTU;
        eth->flags = NET_IFF_UP | NET_IFF_BROADCAST | NET_IFF_RUNNING;
        memcpy(eth->name, "eth0", 5u);
        memcpy(eth->mac, g_mac, 6u);
        memcpy(eth->ipv4, g_ip4, 4u);
        memcpy(eth->netmask4, g_mask4, 4u);
        memcpy(eth->gw4, g_gw4, 4u);
        memcpy(eth->ipv6, g_ip6, 16u);
    }
    net_reply(msgid, &rep, 0u);
}

static void handle_stats(uint32_t msgid) {
    net_rep_t rep = {0};
    net_stats_t *st = &rep.u.stats;
    for (uint32_t i = 1u; i < NET_SOCK_MAX; i++) {
        if (g_socks[i].used && !g_socks[i].user_closed) {
            st->active_socks++;
        }
    }
    for (uint32_t i = 0; i < ARP_WAIT_MAX; i++) {
        st->active_waiters += g_arp_wait[i].used;
    }
    for (uint32_t i = 0; i < PING_WAIT_MAX; i++) {
        st->active_waiters += g_ping_wait[i].used;
    }
    for (uint32_t i = 0; i < CONN_WAIT_MAX; i++) {
        st->active_waiters += g_conn_wait[i].used;
    }
    for (uint32_t i = 0; i < ACC_WAIT_MAX; i++) {
        st->active_waiters += g_acc_wait[i].used;
    }
    for (uint32_t i = 0; i < RECV_WAIT_MAX; i++) {
        st->active_waiters += g_recv_wait[i].used;
    }
    for (uint32_t i = 0; i < POLL_WAIT_MAX; i++) {
        st->active_waiters += g_poll_wait[i].used;
    }
    for (uint32_t i = 0; i < ARP_TABLE_MAX; i++) {
        st->arp_entries += g_arp[i].valid;
    }
    st->nic_present = g_nic_ready ? 1u : 0u;
    virtio_net_get_counters(&st->irq_count, &st->rx_packets, &st->tx_packets,
                            NULL, NULL);
    net_reply(msgid, &rep, 0u);
}

static void handle_arp_resolve(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    if (req->addr.family != NET_AF_INET) {
        rep.err = -EAFNOSUPPORT;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (ip4_is_local(req->addr.addr)) {
        rep.err = 0;
        rep.addr = req->addr;
        memcpy(rep.mac, g_mac, 6u);
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (!g_nic_ready) {
        rep.err = -ENETDOWN;
        net_reply(msgid, &rep, 0u);
        return;
    }
    const uint8_t *cached = arp_lookup(req->addr.addr);
    if (cached && (req->flags & 1u) == 0u) {
        rep.err = 0;
        rep.addr = req->addr;
        memcpy(rep.mac, cached, 6u);
        net_reply(msgid, &rep, 0u);
        return;
    }
    for (uint32_t i = 0; i < ARP_WAIT_MAX; i++) {
        if (!g_arp_wait[i].used) {
            uint64_t now = net_now_mono_ns();
            uint64_t tmo_ms = (req->arg2 > 0) ? (uint64_t)req->arg2 : 2000ULL;
            g_arp_wait[i].used = 1u;
            g_arp_wait[i].msgid = msgid;
            memcpy(g_arp_wait[i].ip4, req->addr.addr, 4u);
            g_arp_wait[i].deadline_ns = now + tmo_ms * 1000000ULL;
            g_arp_wait[i].next_retry_ns = now + 250000000ULL;
            net_tick_ref_inc();
            static const uint8_t zero_mac[6] = {0};
            arp_send_packet(1u, zero_mac, req->addr.addr);
            return;
        }
    }
    rep.err = -EAGAIN;
    net_reply(msgid, &rep, 0u);
}

static void handle_ping(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    uint16_t fam = req->addr.family;
    if (fam != NET_AF_INET && fam != NET_AF_INET6) {
        rep.err = -EAFNOSUPPORT;
        net_reply(msgid, &rep, 0u);
        return;
    }
    uint32_t wslot = PING_WAIT_MAX;
    for (uint32_t i = 0; i < PING_WAIT_MAX; i++) {
        if (!g_ping_wait[i].used) {
            wslot = i;
            break;
        }
    }
    if (wslot == PING_WAIT_MAX) {
        rep.err = -EAGAIN;
        net_reply(msgid, &rep, 0u);
        return;
    }

    uint16_t id = g_ping_id++;
    uint16_t seq = (uint16_t)(req->arg1 > 0 ? req->arg1 : 1);
    uint64_t now = net_now_mono_ns();
    uint64_t tmo_ms = (req->arg2 > 0) ? (uint64_t)req->arg2 : 2000ULL;

    g_ping_wait[wslot].used = 1u;
    g_ping_wait[wslot].family = fam;
    g_ping_wait[wslot].id = id;
    g_ping_wait[wslot].seq = seq;
    g_ping_wait[wslot].msgid = msgid;
    g_ping_wait[wslot].start_ns = now;
    g_ping_wait[wslot].deadline_ns = now + tmo_ms * 1000000ULL;
    net_tick_ref_inc();

    uint8_t icmp[24];
    memset(icmp, 0, sizeof(icmp));
    net_wr_be16(icmp + 4u, id);
    net_wr_be16(icmp + 6u, seq);
    memcpy(icmp + 8u, "microkernel-ping", 16u);

    int rc = 0;
    if (fam == NET_AF_INET) {
        icmp[0] = 8u; /* Echo Request */
        net_wr_be16(icmp + 2u, csum_buf(icmp, sizeof(icmp)));
        rc = tx_ipv4(req->addr.addr, NET_IPPROTO_ICMP, icmp, sizeof(icmp));
    } else {
        net_addr_t src;
        choose_src_ip(&req->addr, &src);
        icmp[0] = 128u; /* ICMPv6 Echo Request */
        uint16_t cs = csum_v6_pseudo(src.addr, req->addr.addr,
                                     NET_IPPROTO_ICMPV6, icmp, sizeof(icmp));
        net_wr_be16(icmp + 2u, cs);
        rc = tx_ipv6(src.addr, req->addr.addr, NET_IPPROTO_ICMPV6, icmp,
                     sizeof(icmp));
    }
    if (rc < 0) {
        g_ping_wait[wslot].used = 0u;
        net_tick_ref_dec();
        rep.err = rc;
        net_reply(msgid, &rep, 0u);
        return;
    }
    drain_loopback();
}

static void handle_sock_create(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    if (req->arg1 != (int32_t)NET_AF_INET &&
        req->arg1 != (int32_t)NET_AF_INET6) {
        rep.err = -EAFNOSUPPORT;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->arg2 != NET_SOCK_STREAM && req->arg2 != NET_SOCK_DGRAM) {
        rep.err = -EPROTOTYPE;
        net_reply(msgid, &rep, 0u);
        return;
    }
    int32_t proto = req->arg3;
    if (proto == 0) {
        proto =
            (req->arg2 == NET_SOCK_STREAM) ? NET_IPPROTO_TCP : NET_IPPROTO_UDP;
    }
    if ((req->arg2 == NET_SOCK_STREAM && proto != NET_IPPROTO_TCP) ||
        (req->arg2 == NET_SOCK_DGRAM && proto != NET_IPPROTO_UDP)) {
        rep.err = -EPROTONOSUPPORT;
        net_reply(msgid, &rep, 0u);
        return;
    }
    uint32_t idx = alloc_sock();
    if (idx == 0u) {
        rep.err = -EMFILE;
        net_reply(msgid, &rep, 0u);
        return;
    }
    net_sock_t *s = &g_socks[idx];
    s->family = (uint16_t)req->arg1;
    s->type = req->arg2;
    s->proto = proto;
    s->local.family = s->family;
    s->peer.family = s->family;
    rep.err = 0;
    rep.handle = idx;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_bind(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->bound) {
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->addr.family != s->family) {
        rep.err = -EAFNOSUPPORT;
        net_reply(msgid, &rep, 0u);
        return;
    }
    /* Validate non-wildcard local IP is one of our configured addresses. */
    if (!addr_is_zero(&req->addr)) {
        int ok = (s->family == NET_AF_INET6) ? ip6_is_local(req->addr.addr)
                                             : ip4_is_local(req->addr.addr);
        if (!ok) {
            rep.err = -EADDRNOTAVAIL;
            net_reply(msgid, &rep, 0u);
            return;
        }
    }
    uint16_t port = req->addr.port;
    if (port == 0u) {
        port = alloc_ephemeral_port(s->family, s->type);
        if (port == 0u) {
            rep.err = -EADDRINUSE;
            net_reply(msgid, &rep, 0u);
            return;
        }
    } else if (port_in_use(s->family, s->type, port, req->addr.addr,
                           req->handle)) {
        rep.err = -EADDRINUSE;
        net_reply(msgid, &rep, 0u);
        return;
    }
    s->local = req->addr;
    s->local.port = port;
    s->bound = 1u;
    rep.err = 0;
    rep.addr = s->local;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_listen(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->type != NET_SOCK_STREAM) {
        rep.err = -EOPNOTSUPP;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (!s->bound) {
        uint16_t port = alloc_ephemeral_port(s->family, s->type);
        if (port == 0u) {
            rep.err = -EADDRINUSE;
            net_reply(msgid, &rep, 0u);
            return;
        }
        s->local.family = s->family;
        s->local.port = port;
        s->bound = 1u;
    }
    int32_t bl = req->arg1;
    if (bl <= 0) {
        bl = 1;
    }
    if ((uint32_t)bl > ACCEPT_Q_MAX) {
        bl = (int32_t)ACCEPT_Q_MAX;
    }
    s->backlog = (uint32_t)bl;
    s->tcp_state = TCP_LISTEN;
    rep.err = 0;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_accept(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->type != NET_SOCK_STREAM || s->tcp_state != TCP_LISTEN) {
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->accept_len > 0u) {
        uint32_t cidx = s->accept_q[s->accept_head];
        s->accept_head = (s->accept_head + 1u) % ACCEPT_Q_MAX;
        s->accept_len--;
        net_sock_t *c = get_sock(cidx);
        if (!c) {
            rep.err = -ECONNABORTED;
        } else {
            rep.err = 0;
            rep.handle = cidx;
            rep.addr = c->peer;
        }
        net_reply(msgid, &rep, 0u);
        return;
    }
    if ((req->flags & MSG_DONTWAIT) != 0u) {
        rep.err = -EAGAIN;
        net_reply(msgid, &rep, 0u);
        return;
    }
    for (uint32_t i = 0; i < ACC_WAIT_MAX; i++) {
        if (!g_acc_wait[i].used) {
            g_acc_wait[i].used = 1u;
            g_acc_wait[i].msgid = msgid;
            g_acc_wait[i].sock_idx = req->handle;
            return;
        }
    }
    rep.err = -EAGAIN;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_connect(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->addr.family != s->family || req->addr.port == 0u) {
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (!s->bound) {
        uint16_t port = alloc_ephemeral_port(s->family, s->type);
        if (port == 0u) {
            rep.err = -EADDRINUSE;
            net_reply(msgid, &rep, 0u);
            return;
        }
        choose_src_ip(&req->addr, &s->local);
        s->local.port = port;
        s->bound = 1u;
    } else if (addr_is_zero(&s->local)) {
        uint16_t p = s->local.port;
        choose_src_ip(&req->addr, &s->local);
        s->local.port = p;
    }

    s->peer = req->addr;
    if (s->type == NET_SOCK_DGRAM) {
        s->connected = 1u;
        rep.err = 0;
        net_reply(msgid, &rep, 0u);
        return;
    }

    if (s->tcp_state == TCP_ESTABLISHED) {
        rep.err = -EISCONN;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->tcp_state == TCP_SYN_SENT) {
        rep.err = -EALREADY;
        net_reply(msgid, &rep, 0u);
        return;
    }

    s->tcp_state = TCP_SYN_SENT;
    s->so_error = 0;
    s->snd_una = g_iss_seed;
    g_iss_seed += 64000u;
    s->snd_nxt = s->snd_una + 1u;

    int rc = tcp_send_seg(&s->local, &s->peer, s->snd_una, 0u, TCP_FLAG_SYN,
                          tcp_window(s), NULL, 0u);
    if (rc < 0) {
        s->tcp_state = TCP_CLOSED;
        rep.err = rc;
        net_reply(msgid, &rep, 0u);
        return;
    }
    drain_loopback();

    if (s->tcp_state == TCP_ESTABLISHED) {
        rep.err = 0;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->tcp_state == TCP_CLOSED) {
        rep.err = (s->so_error != 0) ? -s->so_error : -ECONNREFUSED;
        s->so_error = 0;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if ((req->flags & MSG_DONTWAIT) != 0u) {
        rep.err = -EINPROGRESS;
        net_reply(msgid, &rep, 0u);
        return;
    }
    for (uint32_t i = 0; i < CONN_WAIT_MAX; i++) {
        if (!g_conn_wait[i].used) {
            uint64_t now = net_now_mono_ns();
            g_conn_wait[i].used = 1u;
            g_conn_wait[i].msgid = msgid;
            g_conn_wait[i].sock_idx = req->handle;
            g_conn_wait[i].deadline_ns = now + 3000000000ULL;
            g_conn_wait[i].next_retry_ns = now + 250000000ULL;
            net_tick_ref_inc();
            return;
        }
    }
    rep.err = -EAGAIN;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_send(uint32_t msgid, const net_req_t *req,
                             uint32_t dlen) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (s->shut_wr) {
        rep.err = -EPIPE;
        net_reply(msgid, &rep, 0u);
        return;
    }

    if (s->type == NET_SOCK_DGRAM) {
        net_addr_t dst = req->addr;
        if (dst.family == 0u) {
            if (!s->connected) {
                rep.err = -EDESTADDRREQ;
                net_reply(msgid, &rep, 0u);
                return;
            }
            dst = s->peer;
        }
        if (dst.family != s->family || dst.port == 0u) {
            rep.err = -EINVAL;
            net_reply(msgid, &rep, 0u);
            return;
        }
        if (!s->bound) {
            uint16_t port = alloc_ephemeral_port(s->family, s->type);
            if (port == 0u) {
                rep.err = -EADDRINUSE;
                net_reply(msgid, &rep, 0u);
                return;
            }
            s->local.family = s->family;
            s->local.port = port;
            s->bound = 1u;
        }
        net_addr_t src = s->local;
        if (addr_is_zero(&src)) {
            uint16_t p = src.port;
            choose_src_ip(&dst, &src);
            src.port = p;
        }
        int rc = udp_send_pkt(&src, &dst, req->data, dlen);
        if (rc < 0) {
            rep.err = rc;
        } else {
            rep.err = 0;
            rep.len = dlen;
            drain_loopback();
        }
        net_reply(msgid, &rep, 0u);
        return;
    }

    if (s->tcp_state != TCP_ESTABLISHED && s->tcp_state != TCP_CLOSE_WAIT) {
        rep.err = -ENOTCONN;
        net_reply(msgid, &rep, 0u);
        return;
    }
    uint32_t sent = 0u;
    while (sent < dlen) {
        uint32_t chunk = dlen - sent;
        if (chunk > TCP_MSS) {
            chunk = TCP_MSS;
        }
        int rc = tcp_send_seg(&s->local, &s->peer, s->snd_nxt, s->rcv_nxt,
                              TCP_FLAG_PSH | TCP_FLAG_ACK, tcp_window(s),
                              req->data + sent, chunk);
        if (rc < 0) {
            rep.err = (sent > 0u) ? 0 : rc;
            rep.len = sent;
            net_reply(msgid, &rep, 0u);
            return;
        }
        s->snd_nxt += chunk;
        sent += chunk;
    }
    drain_loopback();
    rep.err = 0;
    rep.len = sent;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_recv(uint32_t msgid, const net_req_t *req) {
    static net_rep_t rep;
    memset(&rep, 0, offsetof(net_rep_t, data));
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    uint32_t want = req->len > NET_DATA_MAX ? NET_DATA_MAX : req->len;
    int peek = (req->flags & MSG_PEEK) != 0u;
    int nonblock = (req->flags & MSG_DONTWAIT) != 0u;

    if (s->type == NET_SOCK_DGRAM) {
        if (s->dg_len > 0u) {
            udp_pkt_t *p = &s->dg_q[s->dg_head];
            uint32_t n = (want < p->len) ? want : p->len;
            if (n > 0u) {
                memcpy(rep.data, p->data, n);
            }
            rep.err = 0;
            rep.len = n;
            rep.addr = p->sender;
            if (!peek) {
                s->dg_head = (s->dg_head + 1u) % UDP_Q_MAX;
                s->dg_len--;
            }
            net_reply(msgid, &rep, n);
            return;
        }
        if (s->shut_rd) {
            rep.err = 0;
            rep.len = 0u;
            net_reply(msgid, &rep, 0u);
            return;
        }
    } else {
        if (s->tcp_state != TCP_ESTABLISHED && s->tcp_state != TCP_CLOSE_WAIT &&
            s->tcp_state != TCP_FIN_WAIT_1 && s->tcp_state != TCP_FIN_WAIT_2 &&
            s->tcp_state != TCP_TIME_WAIT && s->rx_len == 0u && !s->peer_fin) {
            rep.err = -ENOTCONN;
            net_reply(msgid, &rep, 0u);
            return;
        }
        if (s->rx_len > 0u) {
            uint16_t win_before = tcp_window(s);
            uint32_t n = tcp_rx_pop(s, rep.data, want, peek);
            if (!peek && n > 0u && win_before < TCP_MSS &&
                tcp_window(s) >= TCP_MSS &&
                (s->tcp_state == TCP_ESTABLISHED ||
                 s->tcp_state == TCP_CLOSE_WAIT)) {
                (void)tcp_send_seg(&s->local, &s->peer, s->snd_nxt, s->rcv_nxt,
                                   TCP_FLAG_ACK, tcp_window(s), NULL, 0u);
            }
            rep.err = 0;
            rep.len = n;
            rep.addr = s->peer;
            net_reply(msgid, &rep, n);
            return;
        }
        if (s->peer_fin || s->shut_rd || s->tcp_state == TCP_CLOSED) {
            rep.err = (s->so_error != 0) ? -s->so_error : 0;
            s->so_error = 0;
            rep.len = 0u;
            net_reply(msgid, &rep, 0u);
            return;
        }
    }

    if (nonblock) {
        rep.err = -EAGAIN;
        net_reply(msgid, &rep, 0u);
        return;
    }
    for (uint32_t i = 0; i < RECV_WAIT_MAX; i++) {
        if (!g_recv_wait[i].used) {
            g_recv_wait[i].used = 1u;
            g_recv_wait[i].msgid = msgid;
            g_recv_wait[i].sock_idx = req->handle;
            g_recv_wait[i].want_len = want;
            g_recv_wait[i].flags = req->flags;
            return;
        }
    }
    rep.err = -EAGAIN;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_shutdown(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    int32_t how = req->arg1;
    if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR) {
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (how == SHUT_RD || how == SHUT_RDWR) {
        s->shut_rd = 1u;
    }
    if ((how == SHUT_WR || how == SHUT_RDWR) && !s->shut_wr) {
        s->shut_wr = 1u;
        if (s->type == NET_SOCK_STREAM && (s->tcp_state == TCP_ESTABLISHED ||
                                           s->tcp_state == TCP_CLOSE_WAIT)) {
            uint32_t fseq = s->snd_nxt++;
            s->tcp_state = (s->tcp_state == TCP_ESTABLISHED) ? TCP_FIN_WAIT_1
                                                             : TCP_LAST_ACK;
            (void)tcp_send_seg(&s->local, &s->peer, fseq, s->rcv_nxt,
                               TCP_FLAG_FIN | TCP_FLAG_ACK, tcp_window(s), NULL,
                               0u);
            drain_loopback();
        }
    }
    wake_all_waiters();
    rep.err = 0;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_getname(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->arg1 != 0) {
        if (!s->connected) {
            rep.err = -ENOTCONN;
            net_reply(msgid, &rep, 0u);
            return;
        }
        rep.addr = s->peer;
    } else {
        rep.addr = s->local;
    }
    rep.err = 0;
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_getopt(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->arg1 == NET_SOL_SOCKET) {
        switch (req->arg2) {
        case NET_SO_REUSEADDR:
            rep.len = s->so_reuseaddr;
            break;
        case NET_SO_TYPE:
            rep.len = (uint32_t)s->type;
            break;
        case NET_SO_ERROR:
            rep.len = (uint32_t)s->so_error;
            s->so_error = 0;
            break;
        case NET_SO_SNDBUF:
        case NET_SO_RCVBUF:
            rep.len = TCP_RX_BUF_CAP;
            break;
        case NET_SO_KEEPALIVE:
            rep.len = s->so_keepalive;
            break;
        case NET_SO_ACCEPTCONN:
            rep.len = (s->tcp_state == TCP_LISTEN) ? 1u : 0u;
            break;
        default:
            rep.err = -ENOPROTOOPT;
            break;
        }
    } else if (req->arg1 == NET_IPPROTO_TCP && req->arg2 == NET_TCP_NODELAY) {
        rep.len = s->tcp_nodelay;
    } else {
        rep.err = -ENOPROTOOPT;
    }
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_setopt(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    net_sock_t *s = get_sock(req->handle);
    if (!s || s->user_closed) {
        rep.err = -EBADF;
        net_reply(msgid, &rep, 0u);
        return;
    }
    if (req->arg1 == NET_SOL_SOCKET) {
        switch (req->arg2) {
        case NET_SO_REUSEADDR:
            s->so_reuseaddr = (req->arg3 != 0) ? 1u : 0u;
            break;
        case NET_SO_KEEPALIVE:
            s->so_keepalive = (req->arg3 != 0) ? 1u : 0u;
            break;
        case NET_SO_SNDBUF:
        case NET_SO_RCVBUF:
            break;
        default:
            rep.err = -ENOPROTOOPT;
            break;
        }
    } else if (req->arg1 == NET_IPPROTO_TCP && req->arg2 == NET_TCP_NODELAY) {
        s->tcp_nodelay = (req->arg3 != 0) ? 1u : 0u;
    } else {
        rep.err = -ENOPROTOOPT;
    }
    net_reply(msgid, &rep, 0u);
}

static void handle_sock_poll(uint32_t msgid, const net_req_t *req) {
    net_rep_t rep = {0};
    uint32_t nfds = req->poll_nfds;
    if (nfds > NET_POLL_MAX) {
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
    uint32_t ready = 0u;
    for (uint32_t i = 0; i < nfds; i++) {
        rep.u.poll_items[i] = req->poll_items[i];
        int16_t rev = eval_sock_poll(req->poll_items[i].handle,
                                     req->poll_items[i].events);
        rep.u.poll_items[i].revents = rev;
        if (rev != 0) {
            ready++;
        }
    }
    if (ready > 0u || req->poll_timeout_ns == 0) {
        rep.err = 0;
        rep.handle = ready;
        net_reply(msgid, &rep, 0u);
        return;
    }
    for (uint32_t i = 0; i < POLL_WAIT_MAX; i++) {
        if (!g_poll_wait[i].used) {
            g_poll_wait[i].used = 1u;
            g_poll_wait[i].msgid = msgid;
            g_poll_wait[i].nfds = nfds;
            for (uint32_t j = 0; j < nfds; j++) {
                g_poll_wait[i].items[j] = req->poll_items[j];
            }
            if (req->poll_timeout_ns > 0) {
                g_poll_wait[i].has_deadline = 1u;
                g_poll_wait[i].deadline_ns =
                    net_now_mono_ns() + (uint64_t)req->poll_timeout_ns;
                net_tick_ref_inc();
            } else {
                g_poll_wait[i].has_deadline = 0u;
                g_poll_wait[i].deadline_ns = 0ULL;
            }
            return;
        }
    }
    rep.err = -EAGAIN;
    net_reply(msgid, &rep, 0u);
}

void net_proto_init(int nic_ready, const uint8_t mac[6]) {
    g_nic_ready = nic_ready;
    memcpy(g_mac, mac, 6u);
    memset(g_ip6, 0, sizeof(g_ip6));
    /* Link-local fe80::EUI-64 derived from NIC MAC. */
    g_ip6[0] = 0xFEu;
    g_ip6[1] = 0x80u;
    g_ip6[8] = (uint8_t)(mac[0] ^ 0x02u);
    g_ip6[9] = mac[1];
    g_ip6[10] = mac[2];
    g_ip6[11] = 0xFFu;
    g_ip6[12] = 0xFEu;
    g_ip6[13] = mac[3];
    g_ip6[14] = mac[4];
    g_ip6[15] = mac[5];
}

void net_handle_ipc(uint32_t msgid, const net_req_t *req, uint32_t data_len) {
    net_rep_t rep = {0};
    switch (req->op) {
    case NET_OP_INFO:
        handle_info(msgid);
        return;
    case NET_OP_STATS:
        handle_stats(msgid);
        return;
    case NET_OP_ARP_RESOLVE:
        handle_arp_resolve(msgid, req);
        return;
    case NET_OP_PING:
        handle_ping(msgid, req);
        return;
    case NET_OP_SOCK_CREATE:
        handle_sock_create(msgid, req);
        return;
    case NET_OP_SOCK_DUP: {
        net_sock_t *s = get_sock(req->handle);
        if (!s || s->user_closed) {
            rep.err = -EBADF;
        } else {
            s->refs++;
            rep.err = 0;
            rep.handle = req->handle;
        }
        net_reply(msgid, &rep, 0u);
        return;
    }
    case NET_OP_SOCK_CLOSE:
        close_socket_handle(req->handle);
        rep.err = 0;
        net_reply(msgid, &rep, 0u);
        return;
    case NET_OP_SOCK_BIND:
        handle_sock_bind(msgid, req);
        return;
    case NET_OP_SOCK_LISTEN:
        handle_sock_listen(msgid, req);
        return;
    case NET_OP_SOCK_ACCEPT:
        handle_sock_accept(msgid, req);
        return;
    case NET_OP_SOCK_CONNECT:
        handle_sock_connect(msgid, req);
        return;
    case NET_OP_SOCK_SEND:
        handle_sock_send(msgid, req, data_len);
        return;
    case NET_OP_SOCK_RECV:
        handle_sock_recv(msgid, req);
        return;
    case NET_OP_SOCK_SHUTDOWN:
        handle_sock_shutdown(msgid, req);
        return;
    case NET_OP_SOCK_GETNAME:
        handle_sock_getname(msgid, req);
        return;
    case NET_OP_SOCK_GETOPT:
        handle_sock_getopt(msgid, req);
        return;
    case NET_OP_SOCK_SETOPT:
        handle_sock_setopt(msgid, req);
        return;
    case NET_OP_SOCK_POLL:
        handle_sock_poll(msgid, req);
        return;
    default:
        rep.err = -EINVAL;
        net_reply(msgid, &rep, 0u);
        return;
    }
}
