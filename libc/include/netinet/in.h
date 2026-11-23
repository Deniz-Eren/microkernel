/**
 * @file     in.h
 * @brief    POSIX.1-2024 Internet address family definitions (<netinet/in.h>)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

/* need:: ICD_LIBC_0017 */

#include <inttypes.h>
#include <sys/_posix_opts.h>
#include <sys/socket.h>

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned ipv6mr_interface;
};

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_ICMPV6 58
#define IPPROTO_RAW 255

#define INADDR_ANY ((in_addr_t)0x00000000u)
#define INADDR_BROADCAST ((in_addr_t)0xFFFFFFFFu)
#define INADDR_LOOPBACK ((in_addr_t)0x7F000001u)
#define INADDR_NONE ((in_addr_t)0xFFFFFFFFu)

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

#define IN6ADDR_ANY_INIT                                                       \
    {                                                                          \
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }                     \
    }
#define IN6ADDR_LOOPBACK_INIT                                                  \
    {                                                                          \
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }                     \
    }

#if defined(_POSIX_IPV6)
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#endif

#define IPV6_UNICAST_HOPS 16
#define IPV6_MULTICAST_IF 17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP 20
#define IPV6_LEAVE_GROUP 21
#define IPV6_V6ONLY 26

#define IN6_IS_ADDR_UNSPECIFIED(a)                                             \
    (((a)->s6_addr[0] | (a)->s6_addr[1] | (a)->s6_addr[2] | (a)->s6_addr[3] |  \
      (a)->s6_addr[4] | (a)->s6_addr[5] | (a)->s6_addr[6] | (a)->s6_addr[7] |  \
      (a)->s6_addr[8] | (a)->s6_addr[9] | (a)->s6_addr[10] |                   \
      (a)->s6_addr[11] | (a)->s6_addr[12] | (a)->s6_addr[13] |                 \
      (a)->s6_addr[14] | (a)->s6_addr[15]) == 0u)

#define IN6_IS_ADDR_LOOPBACK(a)                                                \
    (((a)->s6_addr[0] | (a)->s6_addr[1] | (a)->s6_addr[2] | (a)->s6_addr[3] |  \
      (a)->s6_addr[4] | (a)->s6_addr[5] | (a)->s6_addr[6] | (a)->s6_addr[7] |  \
      (a)->s6_addr[8] | (a)->s6_addr[9] | (a)->s6_addr[10] |                   \
      (a)->s6_addr[11] | (a)->s6_addr[12] | (a)->s6_addr[13] |                 \
      (a)->s6_addr[14]) == 0u &&                                               \
     (a)->s6_addr[15] == 1u)

#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xFFu)

#define IN6_IS_ADDR_LINKLOCAL(a)                                               \
    ((a)->s6_addr[0] == 0xFEu && ((a)->s6_addr[1] & 0xC0u) == 0x80u)

#define IN6_IS_ADDR_SITELOCAL(a)                                               \
    ((a)->s6_addr[0] == 0xFEu && ((a)->s6_addr[1] & 0xC0u) == 0xC0u)

#define IN6_IS_ADDR_V4MAPPED(a)                                                \
    (((a)->s6_addr[0] | (a)->s6_addr[1] | (a)->s6_addr[2] | (a)->s6_addr[3] |  \
      (a)->s6_addr[4] | (a)->s6_addr[5] | (a)->s6_addr[6] | (a)->s6_addr[7] |  \
      (a)->s6_addr[8] | (a)->s6_addr[9]) == 0u &&                              \
     (a)->s6_addr[10] == 0xFFu && (a)->s6_addr[11] == 0xFFu)

#define IN6_IS_ADDR_V4COMPAT(a)                                                \
    (((a)->s6_addr[0] | (a)->s6_addr[1] | (a)->s6_addr[2] | (a)->s6_addr[3] |  \
      (a)->s6_addr[4] | (a)->s6_addr[5] | (a)->s6_addr[6] | (a)->s6_addr[7] |  \
      (a)->s6_addr[8] | (a)->s6_addr[9] | (a)->s6_addr[10] |                   \
      (a)->s6_addr[11]) == 0u &&                                               \
     !IN6_IS_ADDR_UNSPECIFIED(a) && !IN6_IS_ADDR_LOOPBACK(a))

#define IN6_IS_ADDR_MC_NODELOCAL(a)                                            \
    (IN6_IS_ADDR_MULTICAST(a) && (((a)->s6_addr[1] & 0x0Fu) == 0x01u))

#define IN6_IS_ADDR_MC_LINKLOCAL(a)                                            \
    (IN6_IS_ADDR_MULTICAST(a) && (((a)->s6_addr[1] & 0x0Fu) == 0x02u))

#define IN6_IS_ADDR_MC_SITELOCAL(a)                                            \
    (IN6_IS_ADDR_MULTICAST(a) && (((a)->s6_addr[1] & 0x0Fu) == 0x05u))

#define IN6_IS_ADDR_MC_ORGLOCAL(a)                                             \
    (IN6_IS_ADDR_MULTICAST(a) && (((a)->s6_addr[1] & 0x0Fu) == 0x08u))

#define IN6_IS_ADDR_MC_GLOBAL(a)                                               \
    (IN6_IS_ADDR_MULTICAST(a) && (((a)->s6_addr[1] & 0x0Fu) == 0x0Eu))

uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

#endif /* _NETINET_IN_H */
