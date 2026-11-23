/**
 * @file     socket.c
 * @brief    POSIX.1-2024 AF_UNIX, AF_INET, and AF_INET6 sockets over IPC
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/net.h>
#include <sys/sock.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0013 COMP_LIBC_0019 */

static int g_sock_conn = -1;
static long g_sock_conn_pid = -1;
static uint32_t g_sock_srv_pid;
static uint32_t g_sock_srv_chan;

static int g_net_conn = -1;
static long g_net_conn_pid = -1;
static uint32_t g_net_srv_pid;
static uint32_t g_net_srv_chan;

static int resolve_service(const char *path, uint32_t *pid_out,
                           uint32_t *chan_out) {
    if (*pid_out != 0u && *chan_out != 0u) {
        return 0;
    }
    for (int tries = 0; tries < 200; tries++) {
        long ns = OpenConnection(1, 1);
        if (ns >= 0) {
            name_msg_t q = {0};
            q.type = NAME_MSG_QUERY;
            size_t i = 0u;
            while (path[i] != '\0' && i + 1u < NAME_MAX_PATH) {
                q.path[i] = path[i];
                i++;
            }
            q.path[i] = '\0';
            name_reply_t rep = {0};
            long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
            CloseConnection((int)ns);
            if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                *pid_out = (uint32_t)rep.pid;
                *chan_out = (uint32_t)rep.chanid;
                return 0;
            }
        }
        struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
        __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0, (long)(uintptr_t)&d, 0);
    }
    return -ENXIO;
}

static int ensure_sockserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_sock_conn >= 0 && g_sock_conn_pid == cur_pid) {
        return g_sock_conn;
    }
    int rr = resolve_service("/dev/sock", &g_sock_srv_pid, &g_sock_srv_chan);
    if (rr < 0) {
        return rr;
    }
    long conn = OpenConnection(g_sock_srv_pid, g_sock_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_sock_conn = (int)conn;
    g_sock_conn_pid = cur_pid;
    return g_sock_conn;
}

static int ensure_netserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_net_conn >= 0 && g_net_conn_pid == cur_pid) {
        return g_net_conn;
    }
    int rr = resolve_service("/dev/net", &g_net_srv_pid, &g_net_srv_chan);
    if (rr < 0) {
        return rr;
    }
    long conn = OpenConnection(g_net_srv_pid, g_net_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_net_conn = (int)conn;
    g_net_conn_pid = cur_pid;
    return g_net_conn;
}

int __libc_sock_is_net(uint32_t srv_pid, uint32_t srv_chanid) {
    if (g_sock_srv_pid != 0u && srv_pid == g_sock_srv_pid &&
        srv_chanid == g_sock_srv_chan) {
        return 0;
    }
    if (g_net_srv_pid == 0u || g_net_srv_chan == 0u) {
        (void)resolve_service("/dev/net", &g_net_srv_pid, &g_net_srv_chan);
    }
    return (g_net_srv_pid != 0u && srv_pid == g_net_srv_pid &&
            srv_chanid == g_net_srv_chan)
               ? 1
               : 0;
}

static long sock_rpc(const sock_req_t *req, sock_rep_t *rep) {
    int conn = ensure_sockserv();
    if (conn < 0) {
        return conn;
    }
    long r = SendMsg(conn, req, sizeof(*req), rep, sizeof(*rep));
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static long net_rpc(const net_req_t *req, uint32_t tx_data, net_rep_t *rep,
                    uint32_t rx_data) {
    int conn = ensure_netserv();
    if (conn < 0) {
        return conn;
    }
    memset(rep, 0, offsetof(net_rep_t, data));
    size_t slen = offsetof(net_req_t, data) + tx_data;
    size_t rlen = offsetof(net_rep_t, data) + rx_data;
    long r = SendMsg(conn, req, slen, rep, rlen);
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static int get_sock_fd(int fd, struct __k_fd_entry *out) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)out, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (out->kind != FD_KIND_SOCK) {
        errno = ENOTSOCK;
        return -1;
    }
    return 0;
}

static int parse_unix_addr(const struct sockaddr *addr, socklen_t addrlen,
                           char out[SOCK_PATH_MAX]) {
    if (!addr) {
        errno = EFAULT;
        return -1;
    }
    if (addrlen < (socklen_t)sizeof(sa_family_t)) {
        errno = EINVAL;
        return -1;
    }
    if (addr->sa_family != AF_UNIX) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
    size_t hdr_sz = offsetof(struct sockaddr_un, sun_path);
    if ((size_t)addrlen <= hdr_sz) {
        errno = EINVAL;
        return -1;
    }
    size_t max_copy = (size_t)addrlen - hdr_sz;
    if (max_copy >= SOCK_PATH_MAX) {
        max_copy = SOCK_PATH_MAX - 1u;
    }
    size_t len = 0;
    while (len < max_copy && un->sun_path[len] != '\0') {
        out[len] = un->sun_path[len];
        len++;
    }
    out[len] = '\0';
    if (len == 0u) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static void fill_unix_addr(const char *path, struct sockaddr *addr,
                           socklen_t *addrlen) {
    if (!addr || !addrlen) {
        return;
    }
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    size_t hdr_sz = offsetof(struct sockaddr_un, sun_path);
    size_t real_len = hdr_sz;
    if (path && path[0] != '\0') {
        size_t plen = strlen(path);
        if (plen >= sizeof(un.sun_path)) {
            plen = sizeof(un.sun_path) - 1u;
        }
        memcpy(un.sun_path, path, plen);
        un.sun_path[plen] = '\0';
        real_len = hdr_sz + plen + 1u;
    }
    size_t copy = (size_t)(*addrlen) < real_len ? (size_t)(*addrlen) : real_len;
    if (copy > 0u) {
        memcpy(addr, &un, copy);
    }
    *addrlen = (socklen_t)real_len;
}

static int parse_net_addr(const struct sockaddr *addr, socklen_t addrlen,
                          net_addr_t *out) {
    if (!addr) {
        errno = EFAULT;
        return -1;
    }
    if (addrlen < (socklen_t)sizeof(sa_family_t)) {
        errno = EINVAL;
        return -1;
    }
    memset(out, 0, sizeof(*out));
    if (addr->sa_family == AF_INET) {
        if (addrlen < (socklen_t)sizeof(struct sockaddr_in)) {
            errno = EINVAL;
            return -1;
        }
        const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
        out->family = NET_AF_INET;
        out->port = ntohs(sin->sin_port);
        memcpy(out->addr, &sin->sin_addr.s_addr, 4u);
        return 0;
    }
    if (addr->sa_family == AF_INET6) {
        if (addrlen < (socklen_t)sizeof(struct sockaddr_in6)) {
            errno = EINVAL;
            return -1;
        }
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)addr;
        out->family = NET_AF_INET6;
        out->port = ntohs(sin6->sin6_port);
        memcpy(out->addr, sin6->sin6_addr.s6_addr, 16u);
        return 0;
    }
    errno = EAFNOSUPPORT;
    return -1;
}

static void fill_net_addr(const net_addr_t *na, struct sockaddr *addr,
                          socklen_t *addrlen) {
    if (!addr || !addrlen) {
        return;
    }
    if (na->family == NET_AF_INET6) {
        struct sockaddr_in6 sin6;
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_port = htons(na->port);
        memcpy(sin6.sin6_addr.s6_addr, na->addr, 16u);
        size_t real_len = sizeof(sin6);
        size_t copy =
            (size_t)(*addrlen) < real_len ? (size_t)(*addrlen) : real_len;
        if (copy > 0u) {
            memcpy(addr, &sin6, copy);
        }
        *addrlen = (socklen_t)real_len;
        return;
    }
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(na->port);
    memcpy(&sin.sin_addr.s_addr, na->addr, 4u);
    size_t real_len = sizeof(sin);
    size_t copy = (size_t)(*addrlen) < real_len ? (size_t)(*addrlen) : real_len;
    if (copy > 0u) {
        memcpy(addr, &sin, copy);
    }
    *addrlen = (socklen_t)real_len;
}

long __libc_sock_dup_handle(const struct __k_fd_entry *e) {
    if (__libc_sock_is_net(e->srv_pid, e->srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_DUP;
        req.handle = e->srv_handle;
        return net_rpc(&req, 0u, &rep, 0u);
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_DUP;
    req.handle = e->srv_handle;
    return sock_rpc(&req, &rep);
}

void __libc_sock_close_handle(const struct __k_fd_entry *e) {
    if (__libc_sock_is_net(e->srv_pid, e->srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_CLOSE;
        req.handle = e->srv_handle;
        (void)net_rpc(&req, 0u, &rep, 0u);
        return;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_CLOSE;
    req.handle = e->srv_handle;
    (void)sock_rpc(&req, &rep);
}

ssize_t __libc_sock_read(const struct __k_fd_entry *e, void *buf, size_t n) {
    if (n == 0u) {
        return 0;
    }
    if (__libc_sock_is_net(e->srv_pid, e->srv_chanid)) {
        size_t chunk = n > NET_DATA_MAX ? NET_DATA_MAX : n;
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_RECV;
        req.handle = e->srv_handle;
        req.len = (uint32_t)chunk;
        req.flags =
            (e->status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u;
        long r = net_rpc(&req, 0u, &rep, (uint32_t)chunk);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        if (rep.len > 0u) {
            memcpy(buf, rep.data, rep.len);
        }
        return (ssize_t)rep.len;
    }
    size_t chunk = n > SOCK_DATA_MAX ? SOCK_DATA_MAX : n;
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_RECV;
    req.handle = e->srv_handle;
    req.len = (uint32_t)chunk;
    req.flags = e->status_flags & (uint32_t)O_NONBLOCK;
    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (rep.len > 0u) {
        memcpy(buf, rep.u.data, rep.len);
    }
    return (ssize_t)rep.len;
}

ssize_t __libc_sock_write(const struct __k_fd_entry *e, const void *buf,
                          size_t n) {
    if (n == 0u) {
        return 0;
    }
    if (__libc_sock_is_net(e->srv_pid, e->srv_chanid)) {
        size_t chunk = n > NET_DATA_MAX ? NET_DATA_MAX : n;
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_SEND;
        req.handle = e->srv_handle;
        req.len = (uint32_t)chunk;
        req.flags =
            (e->status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u;
        memcpy(req.data, buf, chunk);
        long r = net_rpc(&req, (uint32_t)chunk, &rep, 0u);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            if (r == -EPIPE) {
                (void)raise(SIGPIPE);
            }
            errno = (int)(-r);
            return -1;
        }
        return (ssize_t)rep.len;
    }
    size_t chunk = n > SOCK_DATA_MAX ? SOCK_DATA_MAX : n;
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_SEND;
    req.handle = e->srv_handle;
    req.len = (uint32_t)chunk;
    req.flags = e->status_flags & (uint32_t)O_NONBLOCK;
    memcpy(req.u.data, buf, chunk);
    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        if (r == -EPIPE) {
            (void)raise(SIGPIPE);
        }
        errno = (int)(-r);
        return -1;
    }
    return (ssize_t)rep.len;
}

long __libc_sock_poll(const sock_poll_item_t *items, uint32_t nfds,
                      int64_t timeout_ns, sock_poll_item_t *out_items) {
    int conn = ensure_sockserv();
    if (conn < 0) {
        return conn;
    }
    sock_poll_req_t req = {0};
    sock_poll_rep_t rep = {0};
    req.op = SOCK_OP_POLL;
    req.nfds = nfds;
    req.timeout_ns = timeout_ns;
    if (nfds > 0u) {
        memcpy(req.items, items, nfds * sizeof(sock_poll_item_t));
    }
    long r = SendMsg(conn, &req, sizeof(req), &rep, sizeof(rep));
    if (r < 0) {
        return r;
    }
    if (rep.err < 0) {
        return rep.err;
    }
    if (nfds > 0u) {
        memcpy(out_items, rep.items, nfds * sizeof(sock_poll_item_t));
    }
    return (long)rep.ready;
}

long __libc_net_poll(const net_poll_item_t *items, uint32_t nfds,
                     int64_t timeout_ns, net_poll_item_t *out_items) {
    net_req_t req;
    net_rep_t rep;
    memset(&req, 0, offsetof(net_req_t, data));
    req.op = NET_OP_SOCK_POLL;
    req.poll_nfds = nfds;
    req.poll_timeout_ns = timeout_ns;
    if (nfds > 0u) {
        memcpy(req.poll_items, items, nfds * sizeof(net_poll_item_t));
    }
    long r = net_rpc(&req, 0u, &rep, 0u);
    if (r < 0) {
        return r;
    }
    if (nfds > 0u) {
        memcpy(out_items, rep.u.poll_items, nfds * sizeof(net_poll_item_t));
    }
    return (long)rep.handle;
}

static int alloc_sock_fd_srv(uint32_t srv_pid, uint32_t srv_chanid,
                             uint32_t handle, int flags) {
    struct __k_fd_entry e = {
        .kind = FD_KIND_SOCK,
        .fd_flags = (flags & SOCK_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
        .status_flags =
            (uint16_t)(O_RDWR | ((flags & SOCK_NONBLOCK) ? O_NONBLOCK : 0)),
        .srv_pid = srv_pid,
        .srv_chanid = srv_chanid,
        .srv_handle = handle,
    };
    long fd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&e, 0, 0);
    if (fd < 0) {
        __libc_sock_close_handle(&e);
        errno = (int)(-fd);
        return -1;
    }
    return (int)fd;
}

int socket(int domain, int type, int protocol) {
    int base_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    int sflags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (base_type != SOCK_STREAM && base_type != SOCK_DGRAM) {
        errno = EPROTOTYPE;
        return -1;
    }
    if (domain == AF_UNIX) {
        if (protocol != 0) {
            errno = EPROTONOSUPPORT;
            return -1;
        }
        sock_req_t req = {0};
        sock_rep_t rep = {0};
        req.op = SOCK_OP_CREATE;
        req.arg1 = (uint32_t)base_type;
        long r = sock_rpc(&req, &rep);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return alloc_sock_fd_srv(g_sock_srv_pid, g_sock_srv_chan, rep.handle,
                                 sflags);
    }
    if (domain == AF_INET || domain == AF_INET6) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_CREATE;
        req.arg1 = domain;
        req.arg2 = base_type;
        req.arg3 = protocol;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return alloc_sock_fd_srv(g_net_srv_pid, g_net_srv_chan, rep.handle,
                                 sflags);
    }
    errno = EAFNOSUPPORT;
    return -1;
}

int socketpair(int domain, int type, int protocol, int socket_vector[2]) {
    if (!socket_vector) {
        errno = EFAULT;
        return -1;
    }
    if (domain != AF_UNIX) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    int base_type = type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    int sflags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (base_type != SOCK_STREAM && base_type != SOCK_DGRAM) {
        errno = EPROTOTYPE;
        return -1;
    }
    if (protocol != 0) {
        errno = EPROTONOSUPPORT;
        return -1;
    }

    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_PAIR;
    req.arg1 = (uint32_t)base_type;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }

    int fd0 =
        alloc_sock_fd_srv(g_sock_srv_pid, g_sock_srv_chan, rep.handle, sflags);
    if (fd0 < 0) {
        struct __k_fd_entry e2 = {.srv_pid = g_sock_srv_pid,
                                  .srv_chanid = g_sock_srv_chan,
                                  .srv_handle = rep.handle2};
        __libc_sock_close_handle(&e2);
        return -1;
    }
    int fd1 =
        alloc_sock_fd_srv(g_sock_srv_pid, g_sock_srv_chan, rep.handle2, sflags);
    if (fd1 < 0) {
        (void)__sys5(SYS_FD_OP, FD_OP_CLOSE, fd0, 0, 0, 0);
        struct __k_fd_entry e1 = {.srv_pid = g_sock_srv_pid,
                                  .srv_chanid = g_sock_srv_chan,
                                  .srv_handle = rep.handle};
        __libc_sock_close_handle(&e1);
        return -1;
    }
    socket_vector[0] = fd0;
    socket_vector[1] = fd1;
    return 0;
}

int bind(int sockfd, const struct sockaddr *address, socklen_t address_len) {
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_BIND;
        req.handle = e.srv_handle;
        if (parse_net_addr(address, address_len, &req.addr) != 0) {
            return -1;
        }
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_BIND;
    req.handle = e.srv_handle;
    if (parse_unix_addr(address, address_len, req.u.path) != 0) {
        return -1;
    }
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int listen(int sockfd, int backlog) {
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_LISTEN;
        req.handle = e.srv_handle;
        req.arg1 = backlog > 0 ? backlog : 1;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_LISTEN;
    req.handle = e.srv_handle;
    req.arg1 = backlog > 0 ? (uint32_t)backlog : 1u;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int accept4(int sockfd, struct sockaddr *restrict address,
            socklen_t *restrict address_len, int flag) {
    __libc_cancel_point();
    if (flag & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_ACCEPT;
        req.handle = e.srv_handle;
        req.flags = (e.status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        int nfd =
            alloc_sock_fd_srv(g_net_srv_pid, g_net_srv_chan, rep.handle, flag);
        if (nfd < 0) {
            return -1;
        }
        fill_net_addr(&rep.addr, address, address_len);
        return nfd;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_ACCEPT;
    req.handle = e.srv_handle;
    req.flags = (uint32_t)e.status_flags;
    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    int nfd =
        alloc_sock_fd_srv(g_sock_srv_pid, g_sock_srv_chan, rep.handle, flag);
    if (nfd < 0) {
        return -1;
    }
    fill_unix_addr(rep.u.path, address, address_len);
    return nfd;
}

int accept(int sockfd, struct sockaddr *restrict address,
           socklen_t *restrict address_len) {
    return accept4(sockfd, address, address_len, 0);
}

int connect(int sockfd, const struct sockaddr *address, socklen_t address_len) {
    __libc_cancel_point();
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_CONNECT;
        req.handle = e.srv_handle;
        req.flags = (e.status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u;
        if (parse_net_addr(address, address_len, &req.addr) != 0) {
            return -1;
        }
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_CONNECT;
    req.handle = e.srv_handle;
    req.flags = (uint32_t)e.status_flags;
    if (parse_unix_addr(address, address_len, req.u.path) != 0) {
        return -1;
    }
    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

ssize_t sendto(int sockfd, const void *message, size_t length, int flags,
               const struct sockaddr *dest_addr, socklen_t dest_len) {
    __libc_cancel_point();
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (length > 0u && !message) {
        errno = EFAULT;
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_SEND;
        req.handle = e.srv_handle;
        req.flags =
            (uint32_t)flags |
            ((e.status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u);
        if (dest_addr != NULL) {
            if (length > NET_DATA_MAX) {
                errno = EMSGSIZE;
                return -1;
            }
            if (parse_net_addr(dest_addr, dest_len, &req.addr) != 0) {
                return -1;
            }
        }
        size_t chunk = length > NET_DATA_MAX ? NET_DATA_MAX : length;
        req.len = (uint32_t)chunk;
        if (chunk > 0u) {
            memcpy(req.data, message, chunk);
        }
        long r = net_rpc(&req, (uint32_t)chunk, &rep, 0u);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            if (r == -EPIPE && !(flags & MSG_NOSIGNAL)) {
                (void)raise(SIGPIPE);
            }
            errno = (int)(-r);
            return -1;
        }
        return (ssize_t)rep.len;
    }

    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_SEND;
    req.handle = e.srv_handle;
    req.flags =
        ((uint32_t)e.status_flags & (uint32_t)O_NONBLOCK) | (uint32_t)flags;

    if (dest_addr != NULL) {
        if (length > SOCK_DGRAM_DATA_MAX) {
            errno = EMSGSIZE;
            return -1;
        }
        if (parse_unix_addr(dest_addr, dest_len, req.u.dgram.path) != 0) {
            return -1;
        }
        req.arg1 = 1u;
        req.len = (uint32_t)length;
        if (length > 0u) {
            memcpy(req.u.dgram.data, message, length);
        }
    } else {
        size_t chunk = length > SOCK_DATA_MAX ? SOCK_DATA_MAX : length;
        req.arg1 = 0u;
        req.len = (uint32_t)chunk;
        if (chunk > 0u) {
            memcpy(req.u.data, message, chunk);
        }
    }

    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        if (r == -EPIPE && !(flags & MSG_NOSIGNAL)) {
            (void)raise(SIGPIPE);
        }
        errno = (int)(-r);
        return -1;
    }
    return (ssize_t)rep.len;
}

ssize_t send(int sockfd, const void *buffer, size_t length, int flags) {
    return sendto(sockfd, buffer, length, flags, NULL, 0);
}

ssize_t recvfrom(int sockfd, void *restrict buffer, size_t length, int flags,
                 struct sockaddr *restrict address,
                 socklen_t *restrict address_len) {
    __libc_cancel_point();
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (length > 0u && !buffer) {
        errno = EFAULT;
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        size_t chunk = length > NET_DATA_MAX ? NET_DATA_MAX : length;
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_RECV;
        req.handle = e.srv_handle;
        req.len = (uint32_t)chunk;
        req.flags =
            (uint32_t)flags |
            ((e.status_flags & (uint32_t)O_NONBLOCK) ? MSG_DONTWAIT : 0u);
        long r = net_rpc(&req, 0u, &rep, (uint32_t)chunk);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        if (rep.len > 0u) {
            memcpy(buffer, rep.data, rep.len);
        }
        if (address && address_len && rep.addr.family != 0u) {
            fill_net_addr(&rep.addr, address, address_len);
        }
        return (ssize_t)rep.len;
    }

    size_t chunk = length > SOCK_DATA_MAX ? SOCK_DATA_MAX : length;
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_RECV;
    req.handle = e.srv_handle;
    req.len = (uint32_t)chunk;
    req.flags =
        ((uint32_t)e.status_flags & (uint32_t)O_NONBLOCK) | (uint32_t)flags;

    long r = sock_rpc(&req, &rep);
    if (r == -EINTR) {
        __libc_cancel_point();
    }
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }

    /* Check socket type to select union branch (dgram vs stream). */
    sock_req_t treq = {0};
    sock_rep_t trep = {0};
    treq.op = SOCK_OP_GETOPT;
    treq.handle = e.srv_handle;
    treq.arg1 = (uint32_t)SOL_SOCKET;
    treq.arg2 = (uint32_t)SO_TYPE;
    int is_dgram = (sock_rpc(&treq, &trep) == 0 && trep.val == SOCK_DGRAM);

    if (rep.len > 0u) {
        if (is_dgram) {
            memcpy(buffer, rep.u.dgram.data, rep.len);
        } else {
            memcpy(buffer, rep.u.data, rep.len);
        }
    }
    if (address && address_len) {
        if (is_dgram) {
            fill_unix_addr(rep.u.dgram.path, address, address_len);
        } else {
            (void)getpeername(sockfd, address, address_len);
        }
    }
    return (ssize_t)rep.len;
}

ssize_t recv(int sockfd, void *buffer, size_t length, int flags) {
    return recvfrom(sockfd, buffer, length, flags, NULL, NULL);
}

ssize_t sendmsg(int sockfd, const struct msghdr *message, int flags) {
    __libc_cancel_point();
    if (!message) {
        errno = EFAULT;
        return -1;
    }
    if (message->msg_iovlen < 0) {
        errno = EMSGSIZE;
        return -1;
    }
    if (message->msg_iovlen > 0 && !message->msg_iov) {
        errno = EFAULT;
        return -1;
    }
    uint8_t flat[NET_DATA_MAX];
    size_t total = 0u;
    for (int i = 0; i < message->msg_iovlen; i++) {
        size_t ilen = message->msg_iov[i].iov_len;
        if (ilen == 0u) {
            continue;
        }
        if (!message->msg_iov[i].iov_base) {
            errno = EFAULT;
            return -1;
        }
        if (total >= sizeof(flat)) {
            break;
        }
        size_t copy =
            (ilen > sizeof(flat) - total) ? (sizeof(flat) - total) : ilen;
        memcpy(flat + total, message->msg_iov[i].iov_base, copy);
        total += copy;
    }
    return sendto(sockfd, flat, total, flags,
                  (const struct sockaddr *)message->msg_name,
                  message->msg_namelen);
}

ssize_t recvmsg(int sockfd, struct msghdr *message, int flags) {
    __libc_cancel_point();
    if (!message) {
        errno = EFAULT;
        return -1;
    }
    if (message->msg_iovlen < 0) {
        errno = EMSGSIZE;
        return -1;
    }
    if (message->msg_iovlen > 0 && !message->msg_iov) {
        errno = EFAULT;
        return -1;
    }
    size_t cap = 0u;
    for (int i = 0; i < message->msg_iovlen; i++) {
        if (message->msg_iov[i].iov_len > 0u && !message->msg_iov[i].iov_base) {
            errno = EFAULT;
            return -1;
        }
        if (cap < NET_DATA_MAX) {
            size_t rem = NET_DATA_MAX - cap;
            cap += (message->msg_iov[i].iov_len > rem)
                       ? rem
                       : message->msg_iov[i].iov_len;
        }
    }
    uint8_t flat[NET_DATA_MAX];
    socklen_t namelen = message->msg_namelen;
    ssize_t got =
        recvfrom(sockfd, flat, cap, flags, (struct sockaddr *)message->msg_name,
                 message->msg_name ? &namelen : NULL);
    if (got < 0) {
        return -1;
    }
    if (message->msg_name) {
        message->msg_namelen = namelen;
    }
    message->msg_controllen = 0u;
    message->msg_flags = 0;

    size_t off = 0u;
    size_t remain = (size_t)got;
    for (int i = 0; i < message->msg_iovlen && remain > 0u; i++) {
        size_t ilen = message->msg_iov[i].iov_len;
        if (ilen == 0u) {
            continue;
        }
        size_t copy = (remain < ilen) ? remain : ilen;
        memcpy(message->msg_iov[i].iov_base, flat + off, copy);
        off += copy;
        remain -= copy;
    }
    return got;
}

int sockatmark(int s) {
    struct __k_fd_entry e = {0};
    if (get_sock_fd(s, &e) != 0) {
        return -1;
    }
    return 0;
}

int shutdown(int sockfd, int how) {
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_SHUTDOWN;
        req.handle = e.srv_handle;
        req.arg1 = how;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_SHUTDOWN;
    req.handle = e.srv_handle;
    req.arg1 = (uint32_t)how;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int getsockname(int sockfd, struct sockaddr *restrict address,
                socklen_t *restrict address_len) {
    if (!address || !address_len) {
        errno = EFAULT;
        return -1;
    }
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_GETNAME;
        req.handle = e.srv_handle;
        req.arg1 = 0;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        fill_net_addr(&rep.addr, address, address_len);
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_GETNAME;
    req.handle = e.srv_handle;
    req.arg1 = 0u;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    fill_unix_addr(rep.u.path, address, address_len);
    return 0;
}

int getpeername(int sockfd, struct sockaddr *restrict address,
                socklen_t *restrict address_len) {
    if (!address || !address_len) {
        errno = EFAULT;
        return -1;
    }
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_GETNAME;
        req.handle = e.srv_handle;
        req.arg1 = 1;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        fill_net_addr(&rep.addr, address, address_len);
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_GETNAME;
    req.handle = e.srv_handle;
    req.arg1 = 1u;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    fill_unix_addr(rep.u.path, address, address_len);
    return 0;
}

int getsockopt(int sockfd, int level, int option_name,
               void *restrict option_value, socklen_t *restrict option_len) {
    if (!option_value || !option_len) {
        errno = EFAULT;
        return -1;
    }
    if (*option_len < (socklen_t)sizeof(int)) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_GETOPT;
        req.handle = e.srv_handle;
        req.arg1 = level;
        req.arg2 = option_name;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        int v = (int)rep.len;
        memcpy(option_value, &v, sizeof(v));
        *option_len = (socklen_t)sizeof(v);
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_GETOPT;
    req.handle = e.srv_handle;
    req.arg1 = (uint32_t)level;
    req.arg2 = (uint32_t)option_name;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    int v = (int)rep.val;
    memcpy(option_value, &v, sizeof(v));
    *option_len = (socklen_t)sizeof(v);
    return 0;
}

int setsockopt(int sockfd, int level, int option_name, const void *option_value,
               socklen_t option_len) {
    if (!option_value) {
        errno = EFAULT;
        return -1;
    }
    if (option_len < (socklen_t)sizeof(int)) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
    }
    int v = 0;
    memcpy(&v, option_value, sizeof(v));
    if (__libc_sock_is_net(e.srv_pid, e.srv_chanid)) {
        net_req_t req;
        net_rep_t rep;
        memset(&req, 0, offsetof(net_req_t, data));
        req.op = NET_OP_SOCK_SETOPT;
        req.handle = e.srv_handle;
        req.arg1 = level;
        req.arg2 = option_name;
        req.arg3 = v;
        long r = net_rpc(&req, 0u, &rep, 0u);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_SETOPT;
    req.handle = e.srv_handle;
    req.arg1 = (uint32_t)level;
    req.arg2 = (uint32_t)option_name;
    req.len = (uint32_t)v;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}
