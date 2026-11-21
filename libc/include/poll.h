/**
 * @file     poll.h
 * @brief    POSIX.1-2024 synchronous I/O multiplexing poll and ppoll interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _POLL_H
#define _POLL_H

/* need:: ICD_IPC_0005 */

#include <signal.h>
#include <time.h>

typedef unsigned long nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN 0x0001
#define POLLPRI 0x0002
#define POLLOUT 0x0004
#define POLLERR 0x0008
#define POLLHUP 0x0010
#define POLLNVAL 0x0020
#define POLLRDNORM 0x0040
#define POLLRDBAND 0x0080
#define POLLWRNORM 0x0100
#define POLLWRBAND 0x0200

int poll(struct pollfd fds[], nfds_t nfds, int timeout);
int ppoll(struct pollfd fds[], nfds_t nfds,
          const struct timespec *restrict timeout,
          const sigset_t *restrict sigmask);

#endif /* _POLL_H */
