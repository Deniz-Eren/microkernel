/**
 * @file     tty.c
 * @brief    POSIX.1-2024 <termios.h>, isatty, and /dev/tty IPC client wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/tty.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0011 */

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

static int check_tty_fd(int fildes) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_CONSOLE && e.kind != FD_KIND_TTY) {
        errno = ENOTTY;
        return -1;
    }
    return 0;
}

ssize_t __libc_tty_read(void *buf, size_t n, uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    size_t chunk = n > TTY_DATA_MAX ? TTY_DATA_MAX : n;
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_READ;
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
        /* Not registered yet, or caller is ttyserv itself. */
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

ssize_t __libc_tty_write(const void *buf, size_t n) {
    if (n == 0u) {
        return 0;
    }
    int conn = ensure_ttyserv_fast();
    if (conn >= 0) {
        const uint8_t *src = (const uint8_t *)buf;
        size_t sent = 0;
        while (sent < n) {
            size_t chunk =
                (n - sent) > TTY_DATA_MAX ? TTY_DATA_MAX : (n - sent);
            tty_req_t req = {0};
            tty_rep_t rep = {0};
            req.op = TTY_OP_WRITE;
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

long __libc_tty_poll(int16_t events, int64_t timeout_ns, int16_t *revents_out) {
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_POLL;
    req.arg = (uint32_t)(uint16_t)events;
    req.timeout_ns = timeout_ns;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        return r;
    }
    *revents_out = rep.revents;
    return 0;
}

int isatty(int fildes) {
    return (check_tty_fd(fildes) == 0) ? 1 : 0;
}

int tcgetattr(int fildes, struct termios *termios_p) {
    if (!termios_p) {
        errno = EFAULT;
        return -1;
    }
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_GETATTR;
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
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SETATTR;
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
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_FLUSH;
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
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_DRAIN;
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
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_FLOW;
    req.arg = (uint32_t)action;
    long r = tty_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int tcsendbreak(int fildes, int duration) {
    if (check_tty_fd(fildes) != 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_SENDBREAK;
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
