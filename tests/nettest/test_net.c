/**
 * @file     test_net.c
 * @brief    Ring 3 PCI virtio-net and TCP/IP/loopback end-to-end QEMU test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_IPC_0011 VER_LIBC_0020 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/net.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

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

typedef struct {
    struct sockaddr_in target;
    int rc;
    char reply[64];
} posix_tcp_arg_t;

static void *posix_tcp_client(void *raw) {
    posix_tcp_arg_t *arg = (posix_tcp_arg_t *)raw;
    arg->rc = -1;
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0) {
        return NULL;
    }
    int one = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) != 0) {
        close(fd);
        return NULL;
    }
    if (connect(fd, (const struct sockaddr *)&arg->target,
                sizeof(arg->target)) != 0) {
        close(fd);
        return NULL;
    }
    /* Scatter-gather sendmsg across two iovecs. */
    char p1[] = "posix-";
    char p2[] = "sendmsg!";
    struct iovec iov[2] = {
        {.iov_base = p1, .iov_len = 6u},
        {.iov_base = p2, .iov_len = 8u},
    };
    struct msghdr msg = {0};
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    if (sendmsg(fd, &msg, 0) != 14) {
        close(fd);
        return NULL;
    }
    /* Read peer reply via read() on the socket FD. */
    ssize_t n = read(fd, arg->reply, sizeof(arg->reply) - 1u);
    if (n <= 0) {
        close(fd);
        return NULL;
    }
    arg->reply[n] = '\0';
    if (sockatmark(fd) != 0) {
        close(fd);
        return NULL;
    }
    (void)shutdown(fd, SHUT_WR);
    char eof_buf[8];
    if (recv(fd, eof_buf, sizeof(eof_buf), 0) != 0) {
        close(fd);
        return NULL;
    }
    close(fd);
    arg->rc = 0;
    return NULL;
}

static int test_posix_inet_and_if(void) {
    /* 1. Byte order and in6addr constants/macros. */
    if (htons(0x1234u) != 0x3412u || ntohs(0x3412u) != 0x1234u ||
        htonl(0x12345678u) != 0x78563412u ||
        ntohl(0x78563412u) != 0x12345678u ||
        !IN6_IS_ADDR_UNSPECIFIED(&in6addr_any) ||
        !IN6_IS_ADDR_LOOPBACK(&in6addr_loopback)) {
        printf("[FAIL] nettest: byte-order or in6addr macro mismatch\n");
        return -1;
    }

    /* 2. inet_addr, inet_ntoa, inet_pton, inet_ntop (IPv4 & IPv6). */
    struct in_addr a4 = {.s_addr = inet_addr("10.0.2.15")};
    if (a4.s_addr == INADDR_NONE || strcmp(inet_ntoa(a4), "10.0.2.15") != 0) {
        printf("[FAIL] nettest: inet_addr/inet_ntoa failed\n");
        return -1;
    }
    uint8_t b4[4] = {0};
    if (inet_pton(AF_INET, "127.0.0.1", b4) != 1 ||
        inet_pton(AF_INET, "127.00.0.1", b4) != 0 ||
        inet_pton(AF_INET, "999.0.0.1", b4) != 0) {
        printf("[FAIL] nettest: inet_pton(AF_INET) validation failed\n");
        return -1;
    }
    char s4[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, b4, s4, sizeof(s4)) ||
        strcmp(s4, "127.0.0.1") != 0) {
        printf("[FAIL] nettest: inet_ntop(AF_INET) failed\n");
        return -1;
    }

    struct in6_addr a6 = {0};
    char s6[INET6_ADDRSTRLEN] = {0};
    if (inet_pton(AF_INET6, "fe80::1", &a6) != 1 ||
        !IN6_IS_ADDR_LINKLOCAL(&a6) ||
        !inet_ntop(AF_INET6, &a6, s6, sizeof(s6)) ||
        strcmp(s6, "fe80::1") != 0) {
        printf("[FAIL] nettest: inet_pton/ntop(fe80::1) failed\n");
        return -1;
    }
    if (inet_pton(AF_INET6, "::ffff:127.0.0.1", &a6) != 1 ||
        !IN6_IS_ADDR_V4MAPPED(&a6) ||
        !inet_ntop(AF_INET6, &a6, s6, sizeof(s6)) ||
        strcmp(s6, "::ffff:127.0.0.1") != 0) {
        printf("[FAIL] nettest: inet_pton/ntop(v4mapped) failed (%s)\n", s6);
        return -1;
    }
    if (inet_ntop(AF_INET, b4, s4, 4u) != NULL || errno != ENOSPC) {
        printf("[FAIL] nettest: inet_ntop ENOSPC check failed\n");
        return -1;
    }

    /* 3. <net/if.h> backed by live /dev/net. */
    unsigned lo_idx = if_nametoindex("lo0");
    unsigned eth_idx = if_nametoindex("eth0");
    char ifbuf[IF_NAMESIZE] = {0};
    if (lo_idx != 1u || eth_idx != 2u || if_nametoindex("nope9") != 0u ||
        !if_indextoname(lo_idx, ifbuf) || strcmp(ifbuf, "lo0") != 0 ||
        !if_indextoname(eth_idx, ifbuf) || strcmp(ifbuf, "eth0") != 0) {
        printf("[FAIL] nettest: if_nametoindex/if_indextoname failed\n");
        return -1;
    }
    struct if_nameindex *ifs = if_nameindex();
    if (!ifs || ifs[0].if_index != 1u || strcmp(ifs[0].if_name, "lo0") != 0 ||
        ifs[1].if_index != 2u || strcmp(ifs[1].if_name, "eth0") != 0 ||
        ifs[2].if_index != 0u || ifs[2].if_name != NULL) {
        printf("[FAIL] nettest: if_nameindex failed\n");
        if_freenameindex(ifs);
        return -1;
    }
    if_freenameindex(ifs);

    /* 4. <netdb.h> host/net/proto/serv databases + getaddrinfo/getnameinfo. */
    sethostent(0);
    struct hostent *he = gethostent();
    if (!he || strcmp(he->h_name, "localhost") != 0) {
        printf("[FAIL] nettest: gethostent failed\n");
        return -1;
    }
    endhostent();

    setnetent(0);
    struct netent *ne = getnetbyname("loopback");
    if (!ne || ne->n_net != 0x7F000000u) {
        printf("[FAIL] nettest: getnetbyname failed\n");
        return -1;
    }
    ne = getnetbyaddr(0x7F000000u, AF_INET);
    if (!ne || strcmp(ne->n_name, "loopback") != 0) {
        printf("[FAIL] nettest: getnetbyaddr failed\n");
        return -1;
    }
    if (!getnetent()) {
        printf("[FAIL] nettest: getnetent failed\n");
        return -1;
    }
    endnetent();

    setprotoent(0);
    struct protoent *pe = getprotobyname("tcp");
    if (!pe || pe->p_proto != IPPROTO_TCP) {
        printf("[FAIL] nettest: getprotobyname failed\n");
        return -1;
    }
    pe = getprotobynumber(IPPROTO_UDP);
    if (!pe || strcmp(pe->p_name, "udp") != 0) {
        printf("[FAIL] nettest: getprotobynumber failed\n");
        return -1;
    }
    if (!getprotoent()) {
        printf("[FAIL] nettest: getprotoent failed\n");
        return -1;
    }
    endprotoent();

    setservent(0);
    struct servent *se = getservbyname("http", "tcp");
    if (!se || ntohs((uint16_t)se->s_port) != 80u) {
        printf("[FAIL] nettest: getservbyname failed\n");
        return -1;
    }
    se = getservbyport((int)htons(53u), "udp");
    if (!se || strcmp(se->s_name, "domain") != 0) {
        printf("[FAIL] nettest: getservbyport failed\n");
        return -1;
    }
    if (!getservent()) {
        printf("[FAIL] nettest: getservent failed\n");
        return -1;
    }
    endservent();

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;
    struct addrinfo *res = NULL;
    if (getaddrinfo("localhost", "http", &hints, &res) != 0 || !res ||
        !res->ai_canonname || strcmp(res->ai_canonname, "localhost") != 0) {
        printf("[FAIL] nettest: getaddrinfo(localhost, http) failed\n");
        freeaddrinfo(res);
        return -1;
    }
    char host_s[64] = {0};
    char serv_s[32] = {0};
    if (getnameinfo(res->ai_addr, res->ai_addrlen, host_s, sizeof(host_s),
                    serv_s, sizeof(serv_s),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0 ||
        strcmp(host_s, "127.0.0.1") != 0 || strcmp(serv_s, "80") != 0) {
        printf("[FAIL] nettest: getnameinfo numeric failed (%s:%s)\n", host_s,
               serv_s);
        freeaddrinfo(res);
        return -1;
    }
    if (getnameinfo(res->ai_addr, res->ai_addrlen, host_s, sizeof(host_s),
                    serv_s, sizeof(serv_s), 0) != 0 ||
        strcmp(host_s, "localhost") != 0 || strcmp(serv_s, "http") != 0) {
        printf("[FAIL] nettest: getnameinfo symbolic failed (%s:%s)\n", host_s,
               serv_s);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (getaddrinfo("no-such-host.invalid", NULL, NULL, &res) != EAI_NONAME ||
        !gai_strerror(EAI_NONAME) || gai_strerror(EAI_NONAME)[0] == '\0') {
        printf("[FAIL] nettest: EAI_NONAME / gai_strerror failed\n");
        return -1;
    }
    printf("[OK] <netinet/in.h>, <netinet/tcp.h>, <arpa/inet.h>, <net/if.h>, "
           "and <netdb.h> verified\n");
    return 0;
}

static int test_posix_inet_sockets(void) {
    /* 1. POSIX AF_INET TCP + sendmsg/recvmsg + read/write + select + dup. */
    int srv = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (srv < 0) {
        printf("[FAIL] nettest: socket(AF_INET) failed errno=%d\n", errno);
        return -1;
    }
    int one = 1;
    int got_opt = 0;
    socklen_t opt_len = sizeof(got_opt);
    if (setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0 ||
        getsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &got_opt, &opt_len) != 0 ||
        got_opt != 1) {
        printf("[FAIL] nettest: AF_INET setsockopt/getsockopt failed\n");
        close(srv);
        return -1;
    }

    struct sockaddr_in saddr = {0};
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_port = 0u;
    if (bind(srv, (const struct sockaddr *)&saddr, sizeof(saddr)) != 0) {
        printf("[FAIL] nettest: AF_INET bind failed errno=%d\n", errno);
        close(srv);
        return -1;
    }
    socklen_t slen = sizeof(saddr);
    if (getsockname(srv, (struct sockaddr *)&saddr, &slen) != 0 ||
        saddr.sin_port == 0u) {
        printf("[FAIL] nettest: AF_INET getsockname failed\n");
        close(srv);
        return -1;
    }
    if (listen(srv, 4) != 0) {
        printf("[FAIL] nettest: AF_INET listen failed\n");
        close(srv);
        return -1;
    }

    posix_tcp_arg_t carg = {0};
    carg.target = saddr;
    pthread_t th;
    if (pthread_create(&th, NULL, posix_tcp_client, &carg) != 0) {
        close(srv);
        return -1;
    }

    struct sockaddr_in paddr = {0};
    socklen_t plen = sizeof(paddr);
    int acc = accept4(srv, (struct sockaddr *)&paddr, &plen, SOCK_CLOEXEC);
    if (acc < 0 || paddr.sin_family != AF_INET || paddr.sin_port == 0u) {
        printf("[FAIL] nettest: AF_INET accept4 failed\n");
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }

    /* Multi-server select() combining /dev/net socket + /dev/pipe FD. */
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }
    if (sockatmark(pfd[0]) != -1 || errno != ENOTSOCK) {
        printf("[FAIL] nettest: sockatmark(pipe) expected ENOTSOCK\n");
        close(pfd[0]);
        close(pfd[1]);
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(acc, &rfds);
    FD_SET(pfd[0], &rfds);
    int maxfd = (acc > pfd[0] ? acc : pfd[0]) + 1;
    struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
    if (select(maxfd, &rfds, NULL, NULL, &tv) < 1 || !FD_ISSET(acc, &rfds)) {
        printf("[FAIL] nettest: select(/dev/net + /dev/pipe) failed\n");
        close(pfd[0]);
        close(pfd[1]);
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }
    close(pfd[0]);
    close(pfd[1]);

    /* Scatter-gather recvmsg across two iovecs. */
    char r1[7] = {0};
    char r2[16] = {0};
    struct iovec riov[2] = {
        {.iov_base = r1, .iov_len = 6u},
        {.iov_base = r2, .iov_len = 15u},
    };
    struct msghdr rmsg = {0};
    rmsg.msg_iov = riov;
    rmsg.msg_iovlen = 2;
    ssize_t rn = recvmsg(acc, &rmsg, 0);
    if (rn != 14 || strcmp(r1, "posix-") != 0 || strcmp(r2, "sendmsg!") != 0) {
        printf("[FAIL] nettest: AF_INET recvmsg mismatch (n=%ld %s|%s)\n",
               (long)rn, r1, r2);
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }

    int acc_dup = dup(acc);
    if (acc_dup < 0 || write(acc_dup, "ok-v4", 5u) != 5) {
        printf("[FAIL] nettest: dup/write on AF_INET socket failed\n");
        if (acc_dup >= 0) {
            close(acc_dup);
        }
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }
    close(acc_dup);

    char eof_b[8];
    if (read(acc, eof_b, sizeof(eof_b)) != 0) {
        printf("[FAIL] nettest: expected EOF on AF_INET socket\n");
        close(acc);
        (void)pthread_join(th, NULL);
        close(srv);
        return -1;
    }
    close(acc);
    close(srv);
    (void)pthread_join(th, NULL);
    if (carg.rc != 0 || strcmp(carg.reply, "ok-v4") != 0) {
        printf("[FAIL] nettest: AF_INET client thread failed\n");
        return -1;
    }

    /* 2. POSIX AF_INET6 UDP sendmsg/recvmsg with peer sockaddr_in6. */
    int u6s = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    int u6c = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    if (u6s < 0 || u6c < 0) {
        if (u6s >= 0) {
            close(u6s);
        }
        if (u6c >= 0) {
            close(u6c);
        }
        return -1;
    }
    struct sockaddr_in6 a6 = {0};
    a6.sin6_family = AF_INET6;
    a6.sin6_addr = in6addr_loopback;
    a6.sin6_port = 0u;
    socklen_t a6len = sizeof(a6);
    if (bind(u6s, (const struct sockaddr *)&a6, sizeof(a6)) != 0 ||
        getsockname(u6s, (struct sockaddr *)&a6, &a6len) != 0 ||
        a6.sin6_port == 0u) {
        close(u6s);
        close(u6c);
        return -1;
    }
    char d1[] = "v6-";
    char d2[] = "dgram";
    struct iovec siov[2] = {{d1, 3u}, {d2, 5u}};
    struct msghdr smsg = {0};
    smsg.msg_name = &a6;
    smsg.msg_namelen = sizeof(a6);
    smsg.msg_iov = siov;
    smsg.msg_iovlen = 2;
    if (sendmsg(u6c, &smsg, 0) != 8) {
        close(u6s);
        close(u6c);
        return -1;
    }
    char rb[16] = {0};
    struct sockaddr_in6 from6 = {0};
    struct iovec r6iov = {rb, sizeof(rb) - 1u};
    struct msghdr r6msg = {0};
    r6msg.msg_name = &from6;
    r6msg.msg_namelen = sizeof(from6);
    r6msg.msg_iov = &r6iov;
    r6msg.msg_iovlen = 1;
    if (recvmsg(u6s, &r6msg, 0) != 8 || strcmp(rb, "v6-dgram") != 0 ||
        from6.sin6_family != AF_INET6 || from6.sin6_port == 0u) {
        close(u6s);
        close(u6c);
        return -1;
    }
    close(u6s);
    close(u6c);

    printf("[OK] POSIX AF_INET/AF_INET6 sockets, sendmsg/recvmsg, sockatmark, "
           "and multi-server select verified\n");
    return 0;
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

    /* 6. POSIX.1-2024 network headers, resolver APIs, and AF_INET/AF_INET6
     *    sockets with sendmsg/recvmsg/sockatmark over /dev/net. */
    if (test_posix_inet_and_if() != 0 || test_posix_inet_sockets() != 0) {
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
