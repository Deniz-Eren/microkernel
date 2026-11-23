/**
 * @file     net.h
 * @brief    Ring 3 network stack and PCI virtio-net IPC protocol (/dev/net)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0017 */

#include <stddef.h>
#include <stdint.h>

/* Address families matching standard values for AF_INET / AF_INET6. */
#define NET_AF_INET 2u
#define NET_AF_INET6 10u

/* Socket types matching <sys/socket.h>. */
#define NET_SOCK_STREAM 1
#define NET_SOCK_DGRAM 2

/* IP protocol numbers. */
#define NET_IPPROTO_IP 0
#define NET_IPPROTO_ICMP 1
#define NET_IPPROTO_TCP 6
#define NET_IPPROTO_UDP 17
#define NET_IPPROTO_IPV6 41
#define NET_IPPROTO_ICMPV6 58

/* Interface flags. */
#define NET_IFF_UP 0x0001u
#define NET_IFF_BROADCAST 0x0002u
#define NET_IFF_LOOPBACK 0x0008u
#define NET_IFF_RUNNING 0x0040u

/* Internal timer pulse code for netserv event loop. */
#define PULSE_CODE_NET_TICK 0x4E54u

/* /dev/net IPC operations. */
#define NET_OP_INFO 1u
#define NET_OP_ARP_RESOLVE 2u
#define NET_OP_PING 3u
#define NET_OP_SOCK_CREATE 4u
#define NET_OP_SOCK_DUP 5u
#define NET_OP_SOCK_CLOSE 6u
#define NET_OP_SOCK_BIND 7u
#define NET_OP_SOCK_LISTEN 8u
#define NET_OP_SOCK_ACCEPT 9u
#define NET_OP_SOCK_CONNECT 10u
#define NET_OP_SOCK_SEND 11u
#define NET_OP_SOCK_RECV 12u
#define NET_OP_SOCK_SHUTDOWN 13u
#define NET_OP_SOCK_GETNAME 14u
#define NET_OP_SOCK_GETOPT 15u
#define NET_OP_SOCK_SETOPT 16u
#define NET_OP_SOCK_POLL 17u
#define NET_OP_STATS 18u

/* Socket option names (aligned with <sys/socket.h> and TCP). */
#define NET_SOL_SOCKET 1
#define NET_SO_REUSEADDR 2
#define NET_SO_TYPE 3
#define NET_SO_ERROR 4
#define NET_SO_SNDBUF 7
#define NET_SO_RCVBUF 8
#define NET_SO_KEEPALIVE 9
#define NET_SO_ACCEPTCONN 30
#define NET_TCP_NODELAY 1

#define NET_MAX_IFS 2u
#define NET_POLL_MAX 16u
#define NET_DATA_MAX 1472u

typedef struct {
    uint16_t family;  /* NET_AF_INET or NET_AF_INET6 */
    uint16_t port;    /* host byte order */
    uint8_t addr[16]; /* IPv4 in addr[0..3] (net order), IPv6 in addr[0..15] */
} net_addr_t;

typedef struct {
    uint32_t handle;
    int16_t events;
    int16_t revents;
} net_poll_item_t;

typedef struct {
    uint32_t index; /* 1 = lo0, 2 = eth0 */
    uint32_t mtu;
    uint32_t flags;
    char name[16];
    uint8_t mac[6];
    uint8_t pad[2];
    uint8_t ipv4[4];
    uint8_t netmask4[4];
    uint8_t gw4[4];
    uint8_t ipv6[16];
} net_if_info_t;

typedef struct {
    uint32_t nic_present;
    uint32_t if_count;
    uint64_t irq_count;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    net_if_info_t ifs[NET_MAX_IFS];
} net_info_t;

typedef struct {
    uint32_t active_socks;
    uint32_t active_waiters;
    uint32_t arp_entries;
    uint32_t nic_present;
    uint64_t irq_count;
    uint64_t rx_packets;
    uint64_t tx_packets;
} net_stats_t;

typedef struct {
    uint32_t op;
    uint32_t handle;
    uint32_t flags;
    uint32_t len;
    int32_t arg1; /* family / backlog / how / level / seq / peer_flag */
    int32_t arg2; /* type / opt_name / timeout_ms */
    int32_t arg3; /* proto / opt_val */
    int32_t reserved;
    net_addr_t addr;
    uint32_t poll_nfds;
    uint32_t poll_pad;
    int64_t poll_timeout_ns;
    net_poll_item_t poll_items[NET_POLL_MAX];
    uint8_t data[NET_DATA_MAX];
} net_req_t;

typedef struct {
    int32_t err;     /* 0 on success, negative errno on error */
    uint32_t handle; /* new socket handle, or ready count for POLL */
    uint32_t len;    /* bytes transferred, opt_val, or ping RTT (ms) */
    uint32_t flags;  /* reply flags or ping sequence */
    net_addr_t addr; /* local/peer/sender address */
    uint8_t mac[6];  /* resolved MAC for NET_OP_ARP_RESOLVE */
    uint8_t pad[2];
    union {
        net_info_t info;
        net_stats_t stats;
        net_poll_item_t poll_items[NET_POLL_MAX];
    } u;
    uint8_t data[NET_DATA_MAX];
} net_rep_t;

_Static_assert(offsetof(net_req_t, data) <= 512u,
               "net_req_t header must fit in IPC_MSG_MAX");
_Static_assert(offsetof(net_rep_t, data) <= 512u,
               "net_rep_t header must fit in IPC_MSG_MAX");
