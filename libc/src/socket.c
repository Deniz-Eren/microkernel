/**
 * @file     socket.c
 * @brief    POSIX.1-2024 AF_UNIX socket client wrappers over /dev/sock IPC
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/sock.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0013 */

static int g_sock_conn = -1;
static long g_sock_conn_pid = -1;
static uint32_t g_sock_srv_pid;
static uint32_t g_sock_srv_chan;

static int ensure_sockserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_sock_conn >= 0 && g_sock_conn_pid == cur_pid) {
        return g_sock_conn;
    }
    if (g_sock_srv_pid == 0u || g_sock_srv_chan == 0u) {
        for (int tries = 0; tries < 200; tries++) {
            long ns = OpenConnection(1, 1);
            if (ns >= 0) {
                name_msg_t q = {0};
                q.type = NAME_MSG_QUERY;
                q.path[0] = '/';
                q.path[1] = 'd';
                q.path[2] = 'e';
                q.path[3] = 'v';
                q.path[4] = '/';
                q.path[5] = 's';
                q.path[6] = 'o';
                q.path[7] = 'c';
                q.path[8] = 'k';
                q.path[9] = '\0';
                name_reply_t rep = {0};
                long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
                CloseConnection((int)ns);
                if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                    g_sock_srv_pid = (uint32_t)rep.pid;
                    g_sock_srv_chan = (uint32_t)rep.chanid;
                    break;
                }
            }
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0, (long)(uintptr_t)&d,
                   0);
        }
    }
    if (g_sock_srv_pid == 0u || g_sock_srv_chan == 0u) {
        return -ENXIO;
    }
    long conn = OpenConnection(g_sock_srv_pid, g_sock_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_sock_conn = (int)conn;
    g_sock_conn_pid = cur_pid;
    return g_sock_conn;
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

long __libc_sock_dup_handle(uint32_t handle) {
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_DUP;
    req.handle = handle;
    return sock_rpc(&req, &rep);
}

void __libc_sock_close_handle(uint32_t handle) {
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_CLOSE;
    req.handle = handle;
    (void)sock_rpc(&req, &rep);
}

ssize_t __libc_sock_read(uint32_t handle, void *buf, size_t n,
                         uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    size_t chunk = n > SOCK_DATA_MAX ? SOCK_DATA_MAX : n;
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_RECV;
    req.handle = handle;
    req.len = (uint32_t)chunk;
    req.flags = status_flags & (uint32_t)O_NONBLOCK;
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

ssize_t __libc_sock_write(uint32_t handle, const void *buf, size_t n,
                          uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    size_t chunk = n > SOCK_DATA_MAX ? SOCK_DATA_MAX : n;
    sock_req_t req = {0};
    sock_rep_t rep = {0};
    req.op = SOCK_OP_SEND;
    req.handle = handle;
    req.len = (uint32_t)chunk;
    req.flags = status_flags & (uint32_t)O_NONBLOCK;
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

static int alloc_sock_fd(uint32_t handle, int flags) {
    struct __k_fd_entry e = {
        .kind = FD_KIND_SOCK,
        .fd_flags = (flags & SOCK_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
        .status_flags =
            (uint16_t)(O_RDWR | ((flags & SOCK_NONBLOCK) ? O_NONBLOCK : 0)),
        .srv_pid = g_sock_srv_pid,
        .srv_chanid = g_sock_srv_chan,
        .srv_handle = handle,
    };
    long fd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&e, 0, 0);
    if (fd < 0) {
        __libc_sock_close_handle(handle);
        errno = (int)(-fd);
        return -1;
    }
    return (int)fd;
}

int socket(int domain, int type, int protocol) {
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
    req.op = SOCK_OP_CREATE;
    req.arg1 = (uint32_t)base_type;
    long r = sock_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return alloc_sock_fd(rep.handle, sflags);
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

    int fd0 = alloc_sock_fd(rep.handle, sflags);
    if (fd0 < 0) {
        __libc_sock_close_handle(rep.handle2);
        return -1;
    }
    int fd1 = alloc_sock_fd(rep.handle2, sflags);
    if (fd1 < 0) {
        (void)__sys5(SYS_FD_OP, FD_OP_CLOSE, fd0, 0, 0, 0);
        __libc_sock_close_handle(rep.handle);
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
    int nfd = alloc_sock_fd(rep.handle, flag);
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

int shutdown(int sockfd, int how) {
    struct __k_fd_entry e = {0};
    if (get_sock_fd(sockfd, &e) != 0) {
        return -1;
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
