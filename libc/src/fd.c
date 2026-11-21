/**
 * @file     fd.c
 * @brief    POSIX.1-2024 pipe2, dup3, fcntl, close, and ppoll over Ring 3 IPC
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0006 */

static int g_pipe_conn = -1;
static long g_pipe_conn_pid = -1;
static uint32_t g_pipe_srv_pid;
static uint32_t g_pipe_srv_chan;

static int ensure_pipeserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_pipe_conn >= 0 && g_pipe_conn_pid == cur_pid) {
        return g_pipe_conn;
    }
    if (g_pipe_srv_pid == 0u || g_pipe_srv_chan == 0u) {
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
                q.path[5] = 'p';
                q.path[6] = 'i';
                q.path[7] = 'p';
                q.path[8] = 'e';
                q.path[9] = '\0';
                name_reply_t rep = {0};
                long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
                CloseConnection((int)ns);
                if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                    g_pipe_srv_pid = (uint32_t)rep.pid;
                    g_pipe_srv_chan = (uint32_t)rep.chanid;
                    break;
                }
            }
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0, (long)(uintptr_t)&d,
                   0);
        }
    }
    if (g_pipe_srv_pid == 0u || g_pipe_srv_chan == 0u) {
        return -ENXIO;
    }
    long conn = OpenConnection(g_pipe_srv_pid, g_pipe_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_pipe_conn = (int)conn;
    g_pipe_conn_pid = cur_pid;
    return g_pipe_conn;
}

static long pipe_rpc(const pipe_req_t *req, pipe_rep_t *rep) {
    int conn = ensure_pipeserv();
    if (conn < 0) {
        return conn;
    }
    long r = SendMsg(conn, req, sizeof(*req), rep, sizeof(*rep));
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static void pipe_close_handle(uint32_t handle) {
    pipe_req_t req = {0};
    pipe_rep_t rep = {0};
    req.op = PIPE_OP_CLOSE;
    req.handle = handle;
    (void)pipe_rpc(&req, &rep);
}

static long pipe_dup_handle(uint32_t handle) {
    pipe_req_t req = {0};
    pipe_rep_t rep = {0};
    req.op = PIPE_OP_DUP;
    req.handle = handle;
    return pipe_rpc(&req, &rep);
}

int __libc_fd_prefork(void) {
    struct __k_fd_entry fds[PROC_MAX_FDS];
    memset(fds, 0, sizeof(fds));
    long n =
        __sys5(SYS_FD_OP, FD_OP_LIST, (long)(uintptr_t)fds, PROC_MAX_FDS, 0, 0);
    if (n < 0) {
        return (int)n;
    }
    for (long i = 0; i < n; i++) {
        if (fds[i].kind == FD_KIND_SERVER || fds[i].kind == FD_KIND_VFS) {
            long r = (fds[i].kind == FD_KIND_VFS)
                         ? __libc_vfs_dup_handle(fds[i].srv_handle)
                         : pipe_dup_handle(fds[i].srv_handle);
            if (r < 0) {
                for (long j = 0; j < i; j++) {
                    if (fds[j].kind == FD_KIND_SERVER) {
                        pipe_close_handle(fds[j].srv_handle);
                    } else if (fds[j].kind == FD_KIND_VFS) {
                        __libc_vfs_close_handle(fds[j].srv_handle);
                    }
                }
                return (int)r;
            }
        }
    }
    return 0;
}

void __libc_fd_postfork_rollback(void) {
    struct __k_fd_entry fds[PROC_MAX_FDS];
    memset(fds, 0, sizeof(fds));
    long n =
        __sys5(SYS_FD_OP, FD_OP_LIST, (long)(uintptr_t)fds, PROC_MAX_FDS, 0, 0);
    if (n < 0) {
        return;
    }
    for (long i = 0; i < n; i++) {
        if (fds[i].kind == FD_KIND_SERVER) {
            pipe_close_handle(fds[i].srv_handle);
        } else if (fds[i].kind == FD_KIND_VFS) {
            __libc_vfs_close_handle(fds[i].srv_handle);
        }
    }
}

ssize_t __libc_fd_read(int fd, void *buf, size_t n) {
    __libc_cancel_point();
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_CONSOLE) {
        errno = ENOSYS;
        return -1;
    }
    if (e.kind == FD_KIND_VFS) {
        return __libc_vfs_read(e.srv_handle, buf, n);
    }
    if (e.kind != FD_KIND_SERVER) {
        errno = EBADF;
        return -1;
    }
    if (n == 0u) {
        return 0;
    }

    size_t chunk = n > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : n;
    pipe_req_t req = {0};
    pipe_rep_t rep = {0};
    req.op = PIPE_OP_READ;
    req.handle = e.srv_handle;
    req.len = (uint32_t)chunk;
    req.flags = (uint32_t)e.status_flags;

    long r = pipe_rpc(&req, &rep);
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

ssize_t __libc_fd_write(int fd, const void *buf, size_t n) {
    __libc_cancel_point();
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_CONSOLE) {
        long r = __sys2(SYS_DEBUG_LOG, (long)(uintptr_t)buf, (long)n);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return (ssize_t)r;
    }
    if (e.kind == FD_KIND_VFS) {
        return __libc_vfs_write(e.srv_handle, buf, n, e.status_flags);
    }
    if (e.kind != FD_KIND_SERVER) {
        errno = EBADF;
        return -1;
    }
    if (n == 0u) {
        return 0;
    }

    size_t chunk = n > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : n;
    pipe_req_t req = {0};
    pipe_rep_t rep = {0};
    req.op = PIPE_OP_WRITE;
    req.handle = e.srv_handle;
    req.len = (uint32_t)chunk;
    req.flags = (uint32_t)e.status_flags;
    memcpy(req.data, buf, chunk);

    long r = pipe_rpc(&req, &rep);
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

int close(int fd) {
    __libc_cancel_point();
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry old = {0};
    long r = __sys5(SYS_FD_OP, FD_OP_CLOSE, fd, (long)(uintptr_t)&old, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (old.kind == FD_KIND_SERVER) {
        pipe_close_handle(old.srv_handle);
    } else if (old.kind == FD_KIND_VFS) {
        __libc_vfs_close_handle(old.srv_handle);
    }
    return 0;
}

int pipe2(int fildes[2], int flags) {
    if (!fildes) {
        errno = EFAULT;
        return -1;
    }
    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        errno = EINVAL;
        return -1;
    }

    pipe_req_t req = {0};
    pipe_rep_t rep = {0};
    req.op = PIPE_OP_CREATE;
    long r = pipe_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }

    uint8_t fd_flags = (flags & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u;
    uint16_t nb = (uint16_t)(flags & O_NONBLOCK);

    struct __k_fd_entry er = {
        .kind = FD_KIND_SERVER,
        .fd_flags = fd_flags,
        .status_flags = (uint16_t)(O_RDONLY | nb),
        .srv_pid = g_pipe_srv_pid,
        .srv_chanid = g_pipe_srv_chan,
        .srv_handle = rep.handle_r,
    };
    struct __k_fd_entry ew = {
        .kind = FD_KIND_SERVER,
        .fd_flags = fd_flags,
        .status_flags = (uint16_t)(O_WRONLY | nb),
        .srv_pid = g_pipe_srv_pid,
        .srv_chanid = g_pipe_srv_chan,
        .srv_handle = rep.handle_w,
    };

    long rfd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&er, 0, 0);
    if (rfd < 0) {
        pipe_close_handle(rep.handle_r);
        pipe_close_handle(rep.handle_w);
        errno = (int)(-rfd);
        return -1;
    }
    long wfd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&ew, 0, 0);
    if (wfd < 0) {
        (void)__sys5(SYS_FD_OP, FD_OP_CLOSE, rfd, 0, 0, 0);
        pipe_close_handle(rep.handle_r);
        pipe_close_handle(rep.handle_w);
        errno = (int)(-wfd);
        return -1;
    }
    fildes[0] = (int)rfd;
    fildes[1] = (int)wfd;
    return 0;
}

int pipe(int fildes[2]) {
    return pipe2(fildes, 0);
}

static int dup_min(int oldfd, int min_fd, int cloexec) {
    if (oldfd < 0 || min_fd < 0) {
        errno = (oldfd < 0) ? EBADF : EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, oldfd, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_SERVER || e.kind == FD_KIND_VFS) {
        long dr = (e.kind == FD_KIND_VFS) ? __libc_vfs_dup_handle(e.srv_handle)
                                          : pipe_dup_handle(e.srv_handle);
        if (dr < 0) {
            errno = (int)(-dr);
            return -1;
        }
    }
    long nf = __sys5(SYS_FD_OP, FD_OP_DUP, oldfd, min_fd,
                     cloexec ? FD_FLAG_CLOEXEC : 0, 0);
    if (nf < 0) {
        if (e.kind == FD_KIND_SERVER) {
            pipe_close_handle(e.srv_handle);
        } else if (e.kind == FD_KIND_VFS) {
            __libc_vfs_close_handle(e.srv_handle);
        }
        errno = (int)(-nf);
        return -1;
    }
    return (int)nf;
}

int dup(int oldfd) {
    return dup_min(oldfd, 0, 0);
}

int dup3(int oldfd, int newfd, int flags) {
    if (flags & ~O_CLOEXEC) {
        errno = EINVAL;
        return -1;
    }
    if (oldfd == newfd) {
        errno = EINVAL;
        return -1;
    }
    if (oldfd < 0 || newfd < 0 || newfd >= PROC_MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, oldfd, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_SERVER || e.kind == FD_KIND_VFS) {
        long dr = (e.kind == FD_KIND_VFS) ? __libc_vfs_dup_handle(e.srv_handle)
                                          : pipe_dup_handle(e.srv_handle);
        if (dr < 0) {
            errno = (int)(-dr);
            return -1;
        }
    }
    struct __k_fd_entry replaced = {0};
    long nf = __sys5(SYS_FD_OP, FD_OP_DUP3, oldfd, newfd,
                     (flags & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0,
                     (long)(uintptr_t)&replaced);
    if (nf < 0) {
        if (e.kind == FD_KIND_SERVER) {
            pipe_close_handle(e.srv_handle);
        } else if (e.kind == FD_KIND_VFS) {
            __libc_vfs_close_handle(e.srv_handle);
        }
        errno = (int)(-nf);
        return -1;
    }
    if (replaced.kind == FD_KIND_SERVER) {
        pipe_close_handle(replaced.srv_handle);
    } else if (replaced.kind == FD_KIND_VFS) {
        __libc_vfs_close_handle(replaced.srv_handle);
    }
    return (int)nf;
}

int dup2(int oldfd, int newfd) {
    if (oldfd < 0 || newfd < 0 || newfd >= PROC_MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    if (oldfd == newfd) {
        struct __k_fd_entry e = {0};
        long gr =
            __sys5(SYS_FD_OP, FD_OP_GET, oldfd, (long)(uintptr_t)&e, 0, 0);
        if (gr < 0) {
            errno = (int)(-gr);
            return -1;
        }
        return newfd;
    }
    return dup3(oldfd, newfd, 0);
}

int fcntl(int fd, int cmd, ...) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    va_list ap;
    va_start(ap, cmd);
    int arg = 0;
    if (cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC || cmd == F_SETFD ||
        cmd == F_SETFL) {
        arg = va_arg(ap, int);
    }
    va_end(ap);

    struct __k_fd_entry e = {0};
    switch (cmd) {
    case F_DUPFD:
        return dup_min(fd, arg, 0);
    case F_DUPFD_CLOEXEC:
        return dup_min(fd, arg, 1);
    case F_GETFD: {
        long r = __sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)&e, 0, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return (e.fd_flags & FD_FLAG_CLOEXEC) ? FD_CLOEXEC : 0;
    }
    case F_SETFD: {
        long r = __sys5(SYS_FD_OP, FD_OP_SETFD, fd,
                        (arg & FD_CLOEXEC) ? FD_FLAG_CLOEXEC : 0, 0, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    case F_GETFL: {
        long r = __sys5(SYS_FD_OP, FD_OP_GET, fd, (long)(uintptr_t)&e, 0, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return (int)e.status_flags;
    }
    case F_SETFL: {
        long r = __sys5(SYS_FD_OP, FD_OP_SETFL, fd, arg, 0, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    default:
        errno = EINVAL;
        return -1;
    }
}

static void restore_sigmask(int swapped, const sigset_t *oldmask) {
    if (swapped) {
        int saved_err = errno;
        (void)__sys3(SYS_SIGPROCMASK, SIG_SETMASK, (long)(uintptr_t)oldmask, 0);
        errno = saved_err;
    }
}

int ppoll(struct pollfd fds[], nfds_t nfds,
          const struct timespec *restrict timeout,
          const sigset_t *restrict sigmask) {
    __libc_cancel_point();
    if (nfds > PIPE_POLL_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (nfds > 0 && !fds) {
        errno = EFAULT;
        return -1;
    }
    int64_t timeout_ns = -1;
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
            timeout->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
        timeout_ns = (int64_t)timeout->tv_sec * 1000000000LL + timeout->tv_nsec;
    }

    sigset_t oldmask = 0;
    int swapped = 0;
    if (sigmask) {
        sigset_t eff_mask = *sigmask & ~(sigset_t)(1u << LIBC_SIGCANCEL);
        sigset_t pend = 0;
        if (__sys1(SYS_SIGPENDING, (long)(uintptr_t)&pend) == 0 &&
            (pend & ~eff_mask) != 0u) {
            (void)__sys3(SYS_SIGPROCMASK, SIG_SETMASK,
                         (long)(uintptr_t)&eff_mask, (long)(uintptr_t)&oldmask);
            (void)__sys3(SYS_SIGPROCMASK, SIG_SETMASK,
                         (long)(uintptr_t)&oldmask, 0);
            __libc_cancel_point();
            errno = EINTR;
            return -1;
        }
        long mr = __sys3(SYS_SIGPROCMASK, SIG_SETMASK,
                         (long)(uintptr_t)&eff_mask, (long)(uintptr_t)&oldmask);
        if (mr < 0) {
            errno = (int)(-mr);
            return -1;
        }
        swapped = 1;
    }

    pipe_poll_req_t preq = {0};
    preq.op = PIPE_OP_POLL;
    nfds_t srv_map[PIPE_POLL_MAX];
    uint32_t srv_count = 0;
    int local_ready = 0;

    for (nfds_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) {
            continue;
        }
        struct __k_fd_entry e = {0};
        long gr =
            __sys5(SYS_FD_OP, FD_OP_GET, fds[i].fd, (long)(uintptr_t)&e, 0, 0);
        if (gr < 0) {
            fds[i].revents = POLLNVAL;
            local_ready++;
            continue;
        }
        if (e.kind == FD_KIND_CONSOLE) {
            short out = (short)(fds[i].events & (POLLOUT | POLLWRNORM));
            if (out) {
                fds[i].revents = out;
                local_ready++;
            }
            continue;
        }
        if (e.kind == FD_KIND_VFS) {
            short rev = (short)(fds[i].events &
                                (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM));
            if (rev) {
                fds[i].revents = rev;
                local_ready++;
            }
            continue;
        }
        if (e.kind == FD_KIND_SERVER) {
            preq.items[srv_count].handle = e.srv_handle;
            preq.items[srv_count].events = fds[i].events;
            preq.items[srv_count].revents = 0;
            srv_map[srv_count] = i;
            srv_count++;
        }
    }

    if (srv_count == 0u) {
        if (local_ready > 0 || timeout_ns == 0) {
            restore_sigmask(swapped, &oldmask);
            return local_ready;
        }
        if (timeout_ns > 0) {
            long sr = __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0,
                             (long)(uintptr_t)timeout, 0);
            restore_sigmask(swapped, &oldmask);
            if (sr == -EINTR) {
                __libc_cancel_point();
            }
            if (sr < 0) {
                errno = (int)(-sr);
                return -1;
            }
            return 0;
        }
        /* Infinite sleep with no active FDs until a signal interrupts. */
        struct timespec lng = {.tv_sec = 3600, .tv_nsec = 0};
        for (;;) {
            long sr = __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0,
                             (long)(uintptr_t)&lng, 0);
            if (sr < 0) {
                restore_sigmask(swapped, &oldmask);
                if (sr == -EINTR) {
                    __libc_cancel_point();
                }
                errno = (int)(-sr);
                return -1;
            }
        }
    }

    preq.nfds = srv_count;
    preq.timeout_ns = (local_ready > 0) ? 0 : timeout_ns;

    int conn = ensure_pipeserv();
    if (conn < 0) {
        restore_sigmask(swapped, &oldmask);
        errno = -conn;
        return -1;
    }

    pipe_poll_rep_t prep = {0};
    long r = SendMsg(conn, &preq, sizeof(preq), &prep, sizeof(prep));
    if (r < 0) {
        restore_sigmask(swapped, &oldmask);
        if (r == -EINTR) {
            __libc_cancel_point();
        }
        errno = (int)(-r);
        return -1;
    }
    if (prep.err < 0) {
        restore_sigmask(swapped, &oldmask);
        errno = -prep.err;
        return -1;
    }

    int total_ready = local_ready;
    for (uint32_t k = 0; k < srv_count; k++) {
        fds[srv_map[k]].revents = prep.items[k].revents;
        if (prep.items[k].revents != 0) {
            total_ready++;
        }
    }
    restore_sigmask(swapped, &oldmask);
    return total_ready;
}

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    struct timespec ts = {0};
    const struct timespec *tsp = NULL;
    if (timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (long)(timeout % 1000) * 1000000L;
        tsp = &ts;
    }
    return ppoll(fds, nfds, tsp, NULL);
}
