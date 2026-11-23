/**
 * @file     ping.c
 * @brief    Ring 3 ICMPv4/ICMPv6 ping CLI utility over /dev/net
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/net.h>
#include <sys/socket.h>
#include <unistd.h>

/* need:: COMP_IPC_0019 */

static int open_netserv(void) {
    int ns = (int)OpenConnection(1u, 1u);
    if (ns < 0) {
        return -1;
    }
    name_msg_t req = {.type = NAME_MSG_QUERY};
    name_reply_t rep = {0};
    (void)snprintf(req.path, sizeof(req.path), "/dev/net");
    long r = SendMsg(ns, &req, sizeof(req), &rep, sizeof(rep));
    (void)CloseConnection(ns);
    if (r < 0 || rep.err != 0) {
        return -1;
    }
    return (int)OpenConnection(rep.pid, rep.chanid);
}

static int resolve_target(const char *host, net_addr_t *out, char *disp,
                          size_t dcap) {
    (void)memset(out, 0, sizeof(*out));
    if (inet_pton(AF_INET, host, out->addr) == 1) {
        out->family = NET_AF_INET;
        (void)snprintf(disp, dcap, "%s", host);
        return 0;
    }
    if (inet_pton(AF_INET6, host, out->addr) == 1) {
        out->family = NET_AF_INET6;
        (void)snprintf(disp, dcap, "%s", host);
        return 0;
    }
    struct addrinfo hints = {.ai_family = AF_UNSPEC};
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) {
        return -1;
    }
    int rc = -1;
    if (res->ai_family == AF_INET) {
        const struct sockaddr_in *s4 = (const struct sockaddr_in *)res->ai_addr;
        out->family = NET_AF_INET;
        (void)memcpy(out->addr, &s4->sin_addr, 4u);
        (void)inet_ntop(AF_INET, &s4->sin_addr, disp, (socklen_t)dcap);
        rc = 0;
    } else if (res->ai_family == AF_INET6) {
        const struct sockaddr_in6 *s6 =
            (const struct sockaddr_in6 *)res->ai_addr;
        out->family = NET_AF_INET6;
        (void)memcpy(out->addr, &s6->sin6_addr, 16u);
        (void)inet_ntop(AF_INET6, &s6->sin6_addr, disp, (socklen_t)dcap);
        rc = 0;
    }
    freeaddrinfo(res);
    return rc;
}

int main(int argc, char **argv) {
    long count = 3;
    long timeout_ms = 2000;
    int opt = 0;
    while ((opt = getopt(argc, argv, "c:W:")) != -1) {
        if (opt == 'c') {
            count = strtol(optarg, NULL, 10);
            if (count <= 0) {
                (void)fprintf(stderr, "ping: invalid count: %s\n", optarg);
                return 1;
            }
        } else if (opt == 'W') {
            timeout_ms = strtol(optarg, NULL, 10);
            if (timeout_ms <= 0) {
                timeout_ms = 1000;
            }
        } else {
            (void)fprintf(stderr, "usage: ping [-c count] [-W ms] host\n");
            return 1;
        }
    }
    if (optind >= argc) {
        (void)fprintf(stderr, "usage: ping [-c count] [-W ms] host\n");
        return 1;
    }

    const char *host = argv[optind];
    net_addr_t target;
    char disp[64];
    if (resolve_target(host, &target, disp, sizeof(disp)) != 0) {
        (void)fprintf(stderr, "ping: cannot resolve %s\n", host);
        return 1;
    }

    int conn = open_netserv();
    if (conn < 0) {
        (void)fprintf(stderr, "ping: /dev/net unavailable\n");
        return 1;
    }

    (void)printf("PING %s (%s): 24 data bytes\n", host, disp);
    long rx = 0;
    for (long seq = 1; seq <= count; seq++) {
        net_req_t req = {0};
        net_rep_t rep = {0};
        req.op = NET_OP_PING;
        req.addr = target;
        req.arg1 = (int32_t)seq;
        req.arg2 = (int32_t)timeout_ms;
        long r = SendMsg(conn, &req, offsetof(net_req_t, data), &rep,
                         offsetof(net_rep_t, data));
        if (r >= 0 && rep.err == 0) {
            rx++;
            (void)printf("24 bytes from %s: icmp_seq=%ld time=%u ms\n", disp,
                         seq, (unsigned)rep.len);
        } else {
            (void)printf("Request timeout for icmp_seq %ld\n", seq);
        }
    }
    (void)CloseConnection(conn);
    (void)printf("--- %s ping statistics ---\n"
                 "%ld packets transmitted, %ld packets received\n",
                 host, count, rx);
    return (rx > 0) ? 0 : 1;
}
