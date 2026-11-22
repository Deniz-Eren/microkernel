/**
 * @file     select.h
 * @brief    POSIX.1-2024 synchronous I/O multiplexing select and pselect header
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

/* need:: ICD_IPC_0014 */

#include <signal.h>
#include <sys/types.h>
#include <time.h>

#ifndef _STRUCT_TIMEVAL_DEFINED
#define _STRUCT_TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif /* _STRUCT_TIMEVAL_DEFINED */

#define FD_SETSIZE 64

typedef struct {
    unsigned long __bits[(FD_SETSIZE + 63) / 64];
} fd_set;

void FD_CLR(int fd, fd_set *fdset);
int FD_ISSET(int fd, fd_set *fdset);
void FD_SET(int fd, fd_set *fdset);
void FD_ZERO(fd_set *fdset);

#define FD_CLR(fd, fdset) (FD_CLR)((fd), (fdset))
#define FD_ISSET(fd, fdset) (FD_ISSET)((fd), (fdset))
#define FD_SET(fd, fdset) (FD_SET)((fd), (fdset))
#define FD_ZERO(fdset) (FD_ZERO)((fdset))

int pselect(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
            fd_set *restrict errorfds, const struct timespec *restrict timeout,
            const sigset_t *restrict sigmask);
int select(int nfds, fd_set *restrict readfds, fd_set *restrict writefds,
           fd_set *restrict errorfds, struct timeval *restrict timeout);

#endif /* _SYS_SELECT_H */
