/**
 * @file     tty.c
 * @brief    POSIX.1-2024 termios, PTYs, ioctl, and /dev/tty IPC client wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/tty.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0011 COMP_IPC_0020 COMP_PROC_0007 COMP_PROC_0009 */

static int g_tty_conn = -1;
static long g_tty_conn_pid = -1;
static uint32_t g_tty_srv_pid;
static uint32_t g_tty_srv_chan;

static int ensure_ttyserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_tty_conn >= 0 && g_tty_conn_pid == cur_pid) {
        return g_tty_conn;
    }
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u) {
        for (int tries = 0; tries < 200; tries++) {
            long ns = OpenConnection(1, 1);
            if (ns >= 0) {
                name_msg_t q = {0};
                q.type = NAME_MSG_QUERY;
                (void)strlcpy(q.path, "/dev/tty", sizeof(q.path));
                name_reply_t rep = {0};
                long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
                CloseConnection((int)ns);
                if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                    g_tty_srv_pid = (uint32_t)rep.pid;
                    g_tty_srv_chan = (uint32_t)rep.chanid;
                    break;
                }
            }
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            (void)__sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0,
                         (long)(uintptr_t)&d, 0);
        }
    }
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u) {
        return -ENXIO;
    }
    long conn = OpenConnection(g_tty_srv_pid, g_tty_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_tty_conn = (int)conn;
    g_tty_conn_pid = cur_pid;
    return g_tty_conn;
}

static int ensure_ttyserv_fast(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 1) {
        /* PID 1 (sysproc) must not query itself; fall back to SYS_DEBUG_LOG. */
        return -EAGAIN;
    }
    if (g_tty_conn >= 0 && g_tty_conn_pid == cur_pid) {
        return g_tty_conn;
    }
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u) {
        long ns = OpenConnection(1, 1);
        if (ns >= 0) {
            name_msg_t q = {0};
            q.type = NAME_MSG_QUERY;
            (void)strlcpy(q.path, "/dev/tty", sizeof(q.path));
            name_reply_t rep = {0};
            long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
            CloseConnection((int)ns);
            if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                g_tty_srv_pid = (uint32_t)rep.pid;
                g_tty_srv_chan = (uint32_t)rep.chanid;
            }
        }
    }
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u ||
        (long)g_tty_srv_pid == cur_pid) {
        return -EAGAIN;
    }
    long conn = OpenConnection(g_tty_srv_pid, g_tty_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_tty_conn = (int)conn;
    g_tty_conn_pid = cur_pid;
    return g_tty_conn;
}

int __libc_tty_is_srv(uint32_t srv_pid, uint32_t srv_chanid) {
    if (srv_pid == 0u || srv_chanid == 0u) {
        return 0;
    }
    if (g_tty_srv_pid == 0u || g_tty_srv_chan == 0u) {
        (void)ensure_ttyserv_fast();
    }
    return (g_tty_srv_pid != 0u && srv_pid == g_tty_srv_pid &&
            srv_chanid == g_tty_srv_chan)
               ? 1
               : 0;
}

static long tty_rpc(const tty_req_t *req, tty_rep_t *rep) {
    int conn = ensure_ttyserv();
    if (conn < 0) {
        return conn;
    }
    long r = SendMsg(conn, req, sizeof(*req), rep, sizeof(*rep));
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static int check_tty_fd_handle_err(int fildes, uint32_t *handle_out) {
    if (fildes < 0) {
        return EBADF;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        return (int)(-gr);
    }
    if (e.kind == FD_KIND_CONSOLE || e.kind == FD_KIND_TTY) {
        if (handle_out) {
            *handle_out = 0u;
        }
        return 0;
    }
    if (e.kind == FD_KIND_SERVER &&
        __libc_tty_is_srv(e.srv_pid, e.srv_chanid)) {
        if (handle_out) {
            *handle_out = e.srv_handle;
        }
        return 0;
    }
    return ENOTTY;
}

static int check_tty_fd_handle(int fildes, uint32_t *handle_out) {
    int err = check_tty_fd_handle_err(fildes, handle_out);
    if (err != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

long __libc_tty_dup_handle(uint32_t handle) {
    if (handle == 0u) {
        return 0;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_DUP;
    req.handle = handle;
    return tty_rpc(&req, &rep);
}

void __libc_tty_close_handle(uint32_t handle) {
    if (handle == 0u) {
        return;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_CLOSE;
    req.handle = handle;
    (void)tty_rpc(&req, &rep);
}

int __libc_tty_open_ptmx(int oflag) {
    if (ensure_ttyserv() < 0) {
        errno = ENXIO;
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_PTY_OPEN_MASTER;
    req.flags = (uint32_t)oflag;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    struct __k_fd_entry e = {
        .kind = FD_KIND_SERVER,
        .fd_flags = (oflag & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
        .status_flags = (uint16_t)(O_RDWR | (oflag & O_NONBLOCK)),
        .srv_pid = g_tty_srv_pid,
        .srv_chanid = g_tty_srv_chan,
        .srv_handle = rep.handle,
    };
    long fd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&e, 0, 0);
    if (fd < 0) {
        __libc_tty_close_handle(rep.handle);
        errno = (int)(-fd);
        return -1;
    }
    return (int)fd;
}

int __libc_tty_open_pts(uint32_t pts_idx, int oflag) {
    if (ensure_ttyserv() < 0) {
        errno = ENXIO;
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_PTY_OPEN_SLAVE;
    req.arg = pts_idx;
    req.flags = (uint32_t)oflag;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    struct __k_fd_entry e = {
        .kind = FD_KIND_SERVER,
        .fd_flags = (oflag & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
        .status_flags = (uint16_t)((oflag & O_ACCMODE) | (oflag & O_NONBLOCK)),
        .srv_pid = g_tty_srv_pid,
        .srv_chanid = g_tty_srv_chan,
        .srv_handle = rep.handle,
    };
    long fd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&e, 0, 0);
    if (fd < 0) {
        __libc_tty_close_handle(rep.handle);
        errno = (int)(-fd);
        return -1;
    }
    return (int)fd;
}

ssize_t __libc_tty_read(uint32_t handle, void *buf, size_t n,
                        uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    size_t chunk = n > TTY_DATA_MAX ? TTY_DATA_MAX : n;
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_READ;
    req.handle = handle;
    req.flags = status_flags;
    req.len = (uint32_t)chunk;
    long r = tty_rpc(&req, &rep);
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

ssize_t __libc_tty_write(uint32_t handle, const void *buf, size_t n,
                         uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    if (handle != 0u) {
        const uint8_t *src = (const uint8_t *)buf;
        size_t sent = 0u;
        while (sent < n) {
            size_t chunk =
                (n - sent) > TTY_DATA_MAX ? TTY_DATA_MAX : (n - sent);
            tty_req_t req = {0};
            tty_rep_t rep = {0};
            req.op = TTY_OP_WRITE;
            req.handle = handle;
            req.flags = status_flags;
            req.len = (uint32_t)chunk;
            memcpy(req.u.data, src + sent, chunk);
            long r = tty_rpc(&req, &rep);
            if (r == -EINTR) {
                __libc_cancel_point();
            }
            if (r < 0) {
                if (sent > 0u) {
                    return (ssize_t)sent;
                }
                errno = (int)(-r);
                return -1;
            }
            sent += (size_t)rep.len;
            if (rep.len < chunk) {
                break;
            }
        }
        return (ssize_t)sent;
    }
    int conn = ensure_ttyserv_fast();
    if (conn >= 0) {
        const uint8_t *src = (const uint8_t *)buf;
        size_t sent = 0u;
        while (sent < n) {
            size_t chunk =
                (n - sent) > TTY_DATA_MAX ? TTY_DATA_MAX : (n - sent);
            tty_req_t req = {0};
            tty_rep_t rep = {0};
            req.op = TTY_OP_WRITE;
            req.handle = 0u;
            req.len = (uint32_t)chunk;
            memcpy(req.u.data, src + sent, chunk);
            long r = SendMsg(conn, &req, sizeof(req), &rep, sizeof(rep));
            if (r < 0 || rep.err < 0) {
                break;
            }
            sent += (size_t)rep.len;
            if (rep.len == 0u) {
                break;
            }
        }
        if (sent > 0u) {
            return (ssize_t)sent;
        }
    }
    long r = __sys2(SYS_DEBUG_LOG, (long)(uintptr_t)buf, (long)n);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return (ssize_t)r;
}

long __libc_tty_poll(const tty_poll_item_t *items, uint32_t nfds,
                     int64_t timeout_ns, tty_poll_item_t *out_items) {
    if (nfds == 0u || nfds > TTY_POLL_MAX) {
        return -EINVAL;
    }
    int conn = ensure_ttyserv();
    if (conn < 0) {
        return conn;
    }
    tty_poll_req_t req = {0};
    tty_poll_rep_t rep = {0};
    req.op = TTY_OP_POLL_MULTI;
    req.nfds = nfds;
    req.timeout_ns = timeout_ns;
    memcpy(req.items, items, nfds * sizeof(tty_poll_item_t));
    long r = SendMsg(conn, &req, sizeof(req), &rep, sizeof(rep));
    if (r < 0) {
        return r;
    }
    if (rep.err < 0) {
        return rep.err;
    }
    memcpy(out_items, rep.items, nfds * sizeof(tty_poll_item_t));
    return 0;
}

int isatty(int fildes) {
    uint32_t h = 0u;
    return (check_tty_fd_handle(fildes, &h) == 0) ? 1 : 0;
}

int tcgetattr(int fildes, struct termios *termios_p) {
    if (!termios_p) {
        errno = EFAULT;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_GETATTR;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    *termios_p = rep.u.tio;
    return 0;
}

int tcsetattr(int fildes, int optional_actions,
              const struct termios *termios_p) {
    if (!termios_p) {
        errno = EFAULT;
        return -1;
    }
    if (optional_actions != TCSANOW && optional_actions != TCSADRAIN &&
        optional_actions != TCSAFLUSH) {
        errno = EINVAL;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SETATTR;
    req.handle = h;
    req.arg = (uint32_t)optional_actions;
    req.u.tio = *termios_p;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int tcflush(int fildes, int queue_selector) {
    if (queue_selector != TCIFLUSH && queue_selector != TCOFLUSH &&
        queue_selector != TCIOFLUSH) {
        errno = EINVAL;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_FLUSH;
    req.handle = h;
    req.arg = (uint32_t)queue_selector;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int tcdrain(int fildes) {
    __libc_cancel_point();
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_DRAIN;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int tcflow(int fildes, int action) {
    if (action != TCOOFF && action != TCOON && action != TCIOFF &&
        action != TCION) {
        errno = EINVAL;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_FLOW;
    req.handle = h;
    req.arg = (uint32_t)action;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int tcsendbreak(int fildes, int duration) {
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SENDBREAK;
    req.handle = h;
    req.arg = (uint32_t)duration;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

speed_t cfgetispeed(const struct termios *termios_p) {
    return termios_p ? termios_p->__c_ispeed : B0;
}

speed_t cfgetospeed(const struct termios *termios_p) {
    return termios_p ? termios_p->__c_ospeed : B0;
}

int cfsetispeed(struct termios *termios_p, speed_t speed) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    termios_p->__c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios *termios_p, speed_t speed) {
    if (!termios_p) {
        errno = EINVAL;
        return -1;
    }
    termios_p->__c_ospeed = speed;
    return 0;
}

pid_t tcgetpgrp(int fildes) {
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return (pid_t)-1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_GETPGRP;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return (pid_t)-1;
    }
    return (pid_t)rep.len;
}

int tcsetpgrp(int fildes, pid_t pgid_id) {
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    if (pgid_id <= 0) {
        errno = EINVAL;
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SETPGRP;
    req.handle = h;
    req.arg = (uint32_t)pgid_id;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

pid_t tcgetsid(int fildes) {
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return (pid_t)-1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_GETSID;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return (pid_t)-1;
    }
    if (rep.len > 0u) {
        return (pid_t)rep.len;
    }
    return getsid(0);
}

int tcgetwinsize(int fildes, struct winsize *winsize_p) {
    if (!winsize_p) {
        errno = EFAULT;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_GETWINSIZE;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    *winsize_p = rep.u.ws;
    return 0;
}

int tcsetwinsize(int fildes, const struct winsize *winsize_p) {
    if (!winsize_p) {
        errno = EFAULT;
        return -1;
    }
    uint32_t h = 0u;
    if (check_tty_fd_handle(fildes, &h) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SETWINSIZE;
    req.handle = h;
    req.u.ws = *winsize_p;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

static int format_pts_path(uint32_t idx, char *buf, size_t cap) {
    static const char prefix[] = "/dev/pts/";
    char digits[12];
    size_t dlen = 0u;
    uint32_t v = idx;
    do {
        digits[dlen++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v > 0u && dlen < sizeof(digits));
    size_t plen = sizeof(prefix) - 1u;
    if (plen + dlen + 1u > cap) {
        return ERANGE;
    }
    memcpy(buf, prefix, plen);
    for (size_t i = 0; i < dlen; i++) {
        buf[plen + i] = digits[dlen - 1u - i];
    }
    buf[plen + dlen] = '\0';
    return 0;
}

int ttyname_r(int fildes, char *name, size_t namesize) {
    uint32_t h = 0u;
    int err = check_tty_fd_handle_err(fildes, &h);
    if (err != 0) {
        return err;
    }
    if (!name) {
        return EINVAL;
    }
    if (h == 0u) {
        static const char k_tty_path[] = "/dev/tty";
        if (namesize < sizeof(k_tty_path)) {
            return ERANGE;
        }
        memcpy(name, k_tty_path, sizeof(k_tty_path));
        return 0;
    }
    if ((h & 1u) == 0u) {
        static const char k_ptmx_path[] = "/dev/ptmx";
        if (namesize < sizeof(k_ptmx_path)) {
            return ERANGE;
        }
        memcpy(name, k_ptmx_path, sizeof(k_ptmx_path));
        return 0;
    }
    return format_pts_path((h >> 1) - 1u, name, namesize);
}

char *ttyname(int fildes) {
    static char g_ttyname_buf[L_ctermid];
    int err = ttyname_r(fildes, g_ttyname_buf, sizeof(g_ttyname_buf));
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return g_ttyname_buf;
}

char *ctermid(char *s) {
    static char g_ctermid_buf[L_ctermid];
    static const char k_ctermid[] = "/dev/tty";
    char *dst = s ? s : g_ctermid_buf;
    memcpy(dst, k_ctermid, sizeof(k_ctermid));
    return dst;
}

int posix_openpt(int oflag) {
    if ((oflag & ~(O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK)) != 0) {
        errno = EINVAL;
        return -1;
    }
    return __libc_tty_open_ptmx(oflag);
}

int grantpt(int fildes) {
    uint32_t h = 0u;
    int err = check_tty_fd_handle_err(fildes, &h);
    if (err != 0) {
        errno = (err == ENOTTY) ? EINVAL : err;
        return -1;
    }
    if (h == 0u || (h & 1u) != 0u) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int unlockpt(int fildes) {
    uint32_t h = 0u;
    int err = check_tty_fd_handle_err(fildes, &h);
    if (err != 0) {
        errno = (err == ENOTTY) ? EINVAL : err;
        return -1;
    }
    if (h == 0u || (h & 1u) != 0u) {
        errno = EINVAL;
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_PTY_UNLOCK;
    req.handle = h;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int ptsname_r(int fildes, char *name, size_t namesize) {
    uint32_t h = 0u;
    int err = check_tty_fd_handle_err(fildes, &h);
    if (err != 0) {
        return (err == ENOTTY) ? ENOTTY : err;
    }
    if (h == 0u || (h & 1u) != 0u) {
        return ENOTTY;
    }
    if (!name) {
        return EINVAL;
    }
    return format_pts_path((h >> 1) - 1u, name, namesize);
}

char *ptsname(int fildes) {
    static char g_ptsname_buf[32];
    int err = ptsname_r(fildes, g_ptsname_buf, sizeof(g_ptsname_buf));
    if (err != 0) {
        errno = err;
        return NULL;
    }
    return g_ptsname_buf;
}

int ioctl(int fildes, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *argp = va_arg(ap, void *);
    va_end(ap);

    switch (request) {
    case TIOCGWINSZ:
        return tcgetwinsize(fildes, (struct winsize *)argp);
    case TIOCSWINSZ:
        return tcsetwinsize(fildes, (const struct winsize *)argp);
    case TIOCSCTTY: {
        uint32_t h = 0u;
        if (check_tty_fd_handle(fildes, &h) != 0) {
            return -1;
        }
        tty_req_t req = {0};
        tty_rep_t rep = {0};
        req.op = TTY_OP_SCTTY;
        req.handle = h;
        pid_t sid = getsid(0);
        pid_t pgrp = getpgrp();
        req.arg = sid > 0 ? (uint32_t)sid : 0u;
        req.flags = pgrp > 0 ? (uint32_t)pgrp : 0u;
        long r = tty_rpc(&req, &rep);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    case TIOCNOTTY: {
        uint32_t h = 0u;
        return check_tty_fd_handle(fildes, &h);
    }
    case TIOCGPGRP: {
        if (!argp) {
            errno = EFAULT;
            return -1;
        }
        pid_t pg = tcgetpgrp(fildes);
        if (pg < 0) {
            return -1;
        }
        *(pid_t *)argp = pg;
        return 0;
    }
    case TIOCSPGRP:
        if (!argp) {
            errno = EFAULT;
            return -1;
        }
        return tcsetpgrp(fildes, *(const pid_t *)argp);
    case FIONBIO: {
        if (!argp) {
            errno = EFAULT;
            return -1;
        }
        int fl = fcntl(fildes, F_GETFL);
        if (fl < 0) {
            return -1;
        }
        if (*(const int *)argp) {
            fl |= O_NONBLOCK;
        } else {
            fl &= ~O_NONBLOCK;
        }
        return fcntl(fildes, F_SETFL, fl);
    }
    case FIONREAD: {
        if (!argp) {
            errno = EFAULT;
            return -1;
        }
        uint32_t h = 0u;
        int err = check_tty_fd_handle_err(fildes, &h);
        if (err == 0) {
            tty_req_t req = {0};
            tty_rep_t rep = {0};
            req.op = TTY_OP_FIONREAD;
            req.handle = h;
            long r = tty_rpc(&req, &rep);
            if (r < 0) {
                errno = (int)(-r);
                return -1;
            }
            *(int *)argp = (int)rep.len;
            return 0;
        }
        if (err == EBADF) {
            errno = EBADF;
            return -1;
        }
        *(int *)argp = 0;
        return 0;
    }
    default:
        errno = ENOTTY;
        return -1;
    }
}
