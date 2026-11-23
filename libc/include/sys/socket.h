/**
 * @file     socket.h
 * @brief    POSIX.1-2024 Base socket types, constants, and function APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

/* need:: ICD_IPC_0013 ICD_LIBC_0017 */

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef uint16_t sa_family_t;
#endif

typedef uint32_t socklen_t;

#ifndef _STRUCT_IOVEC_DEFINED
#define _STRUCT_IOVEC_DEFINED
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_storage {
    sa_family_t ss_family;
    char __ss_pad1[6];
    uint64_t __ss_align;
    char __ss_pad2[112];
};

struct msghdr {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

struct cmsghdr {
    socklen_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

struct linger {
    int l_onoff;
    int l_linger;
};

#define SCM_RIGHTS 1

#define __CMSG_ALIGN(len)                                                      \
    (((size_t)(len) + sizeof(size_t) - 1u) & ~(sizeof(size_t) - 1u))

#define CMSG_DATA(cmsg)                                                        \
    ((unsigned char *)((unsigned char *)(cmsg) +                               \
                       __CMSG_ALIGN(sizeof(struct cmsghdr))))

#define CMSG_FIRSTHDR(mhdr)                                                    \
    ((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr)                  \
         ? (struct cmsghdr *)(mhdr)->msg_control                               \
         : (struct cmsghdr *)0)

#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr((mhdr), (cmsg))

static inline struct cmsghdr *__cmsg_nxthdr(const struct msghdr *__mhdr,
                                            const struct cmsghdr *__cmsg) {
    if (!__cmsg || __cmsg->cmsg_len < (socklen_t)sizeof(struct cmsghdr)) {
        return (struct cmsghdr *)0;
    }
    const unsigned char *__base = (const unsigned char *)__mhdr->msg_control;
    const unsigned char *__end = __base + __mhdr->msg_controllen;
    const unsigned char *__next =
        (const unsigned char *)__cmsg + __CMSG_ALIGN(__cmsg->cmsg_len);
    if (__next + sizeof(struct cmsghdr) > __end) {
        return (struct cmsghdr *)0;
    }
    return (struct cmsghdr *)(uintptr_t)__next;
}

#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_INET 2
#define AF_INET6 10

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_SEQPACKET 5

/* POSIX.1-2024 atomic creation flags for socket, socketpair, and accept4. */
#define SOCK_NONBLOCK 0x0800
#define SOCK_CLOEXEC 0x00080000

#define SOL_SOCKET 1
#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_LINGER 13
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_ACCEPTCONN 30

#define MSG_OOB 0x0001
#define MSG_PEEK 0x0002
#define MSG_CTRUNC 0x0008
#define MSG_TRUNC 0x0020
#define MSG_DONTWAIT 0x0040
#define MSG_EOR 0x0080
#define MSG_WAITALL 0x0100
#define MSG_NOSIGNAL 0x4000

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SOMAXCONN 16

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int socket_vector[2]);
int bind(int socket, const struct sockaddr *address, socklen_t address_len);
int listen(int socket, int backlog);
int accept(int socket, struct sockaddr *restrict address,
           socklen_t *restrict address_len);
int accept4(int socket, struct sockaddr *restrict address,
            socklen_t *restrict address_len, int flag);
int connect(int socket, const struct sockaddr *address, socklen_t address_len);
ssize_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags,
               const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags,
                 struct sockaddr *restrict address,
                 socklen_t *restrict address_len);
ssize_t sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t recvmsg(int socket, struct msghdr *message, int flags);
int sockatmark(int s);
int shutdown(int socket, int how);
int getsockname(int socket, struct sockaddr *restrict address,
                socklen_t *restrict address_len);
int getpeername(int socket, struct sockaddr *restrict address,
                socklen_t *restrict address_len);
int getsockopt(int socket, int level, int option_name,
               void *restrict option_value, socklen_t *restrict option_len);
int setsockopt(int socket, int level, int option_name, const void *option_value,
               socklen_t option_len);

#endif /* _SYS_SOCKET_H */
