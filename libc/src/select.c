/**
 * @file     select.c
 * @brief    POSIX.1-2024 select, pselect, and fd_set operations over ppoll
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <poll.h>
#include <sys/fd.h>
#include <sys/select.h>
#include "tls_internal.h"

/* need:: COMP_IPC_0014 */

void(FD_CLR)(int fd, fd_set *fdset) {
    if (!fdset || fd < 0 || fd >= FD_SETSIZE) {
        return;
    }
    unsigned idx = (unsigned)fd / 64u;
    unsigned bit = (unsigned)fd % 64u;
    fdset->__bits[idx] &= ~(1UL << bit);
}

int(FD_ISSET)(int fd, fd_set *fdset) {
    if (!fdset || fd < 0 || fd >= FD_SETSIZE) {
        return 0;
    }
    unsigned idx = (unsigned)fd / 64u;
    unsigned bit = (unsigned)fd % 64u;
    return (fdset->__bits[idx] & (1UL << bit)) != 0UL ? 1 : 0;
}

void(FD_SET)(int fd, fd_set *fdset) {
    if (!fdset || fd < 0 || fd >= FD_SETSIZE) {
        return;
    }
    unsigned idx = (unsigned)fd / 64u;
    unsigned bit = (unsigned)fd % 64u;
    fdset->__bits[idx] |= (1UL << bit);
}

void(FD_ZERO)(fd_set *fdset) {
    if (!fdset) {
        return;
    }
    for (unsigned i = 0; i < (unsigned)((FD_SETSIZE + 63) / 64); i++) {
        fdset->__bits[i] = 0UL;
    }
}

int pselect(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
            fd_set *restrict errorfds, const struct timespec *restrict timeout,
            const sigset_t *restrict sigmask) {
    __libc_cancel_point();

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
            timeout->tv_nsec >= 1000000000L) {
            errno = EINVAL;
            return -1;
        }
    }

    struct pollfd pfds[PROC_MAX_FDS];
    nfds_t n = 0;
    for (int fd = 0; fd < nfds; fd++) {
        short ev = 0;
        if (readfds && (FD_ISSET)(fd, readfds)) {
            ev = (short)(ev | POLLIN);
        }
        if (writefds && (FD_ISSET)(fd, writefds)) {
            ev = (short)(ev | POLLOUT);
        }
        if (errorfds && (FD_ISSET)(fd, errorfds)) {
            ev = (short)(ev | POLLPRI);
        }
        if (ev == 0) {
            continue;
        }
        if (fd >= PROC_MAX_FDS || n >= (nfds_t)PROC_MAX_FDS) {
            errno = EBADF;
            return -1;
        }
        pfds[n].fd = fd;
        pfds[n].events = ev;
        pfds[n].revents = 0;
        n++;
    }

    int rc = ppoll(pfds, n, timeout, sigmask);
    if (rc < 0) {
        return -1;
    }

    for (nfds_t i = 0; i < n; i++) {
        if (pfds[i].revents & POLLNVAL) {
            errno = EBADF;
            return -1;
        }
    }

    if (readfds) {
        (FD_ZERO)(readfds);
    }
    if (writefds) {
        (FD_ZERO)(writefds);
    }
    if (errorfds) {
        (FD_ZERO)(errorfds);
    }

    int total_bits = 0;
    for (nfds_t i = 0; i < n; i++) {
        int fd = pfds[i].fd;
        short rev = pfds[i].revents;
        if (readfds && (pfds[i].events & POLLIN) &&
            (rev & (POLLIN | POLLHUP | POLLERR))) {
            (FD_SET)(fd, readfds);
            total_bits++;
        }
        if (writefds && (pfds[i].events & POLLOUT) &&
            (rev & (POLLOUT | POLLERR))) {
            (FD_SET)(fd, writefds);
            total_bits++;
        }
        if (errorfds && (pfds[i].events & POLLPRI) &&
            (rev & (POLLPRI | POLLERR))) {
            (FD_SET)(fd, errorfds);
            total_bits++;
        }
    }
    return total_bits;
}

int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
           fd_set *restrict errorfds, struct timeval *restrict timeout) {
    __libc_cancel_point();

    struct timespec ts = {0};
    const struct timespec *ts_ptr = NULL;
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0 ||
            timeout->tv_usec >= 1000000L) {
            errno = EINVAL;
            return -1;
        }
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000L;
        ts_ptr = &ts;
    }
    return pselect(nfds, readfds, writefds, errorfds, ts_ptr, NULL);
}
