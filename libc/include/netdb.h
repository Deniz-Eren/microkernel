/**
 * @file     netdb.h
 * @brief    POSIX.1-2024 network database and resolver APIs (<netdb.h>)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _NETDB_H
#define _NETDB_H

/* need:: ICD_LIBC_0017 */

#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct netent {
    char *n_name;
    char **n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

struct protoent {
    char *p_name;
    char **p_aliases;
    int p_proto;
};

struct servent {
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

#define IPPORT_RESERVED 1024

#define AI_PASSIVE 0x0001
#define AI_CANONNAME 0x0002
#define AI_NUMERICHOST 0x0004
#define AI_NUMERICSERV 0x0008
#define AI_V4MAPPED 0x0010
#define AI_ALL 0x0020
#define AI_ADDRCONFIG 0x0040

#define NI_NOFQDN 0x0001
#define NI_NUMERICHOST 0x0002
#define NI_NAMEREQD 0x0004
#define NI_NUMERICSERV 0x0008
#define NI_DGRAM 0x0010
#define NI_NUMERICSCOPE 0x0020

#define NI_MAXHOST 1025
#define NI_MAXSERV 32

#define EAI_AGAIN (-3)
#define EAI_BADFLAGS (-1)
#define EAI_FAIL (-4)
#define EAI_FAMILY (-6)
#define EAI_MEMORY (-10)
#define EAI_NONAME (-2)
#define EAI_SERVICE (-8)
#define EAI_SOCKTYPE (-7)
#define EAI_SYSTEM (-11)
#define EAI_OVERFLOW (-12)

void endhostent(void);
void endnetent(void);
void endprotoent(void);
void endservent(void);
void freeaddrinfo(struct addrinfo *ai);
const char *gai_strerror(int ecode);
int getaddrinfo(const char *restrict nodename, const char *restrict servname,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res);
struct hostent *gethostent(void);
int getnameinfo(const struct sockaddr *restrict sa, socklen_t salen,
                char *restrict node, socklen_t nodelen, char *restrict service,
                socklen_t servicelen, int flags);
struct netent *getnetbyaddr(uint32_t net, int type);
struct netent *getnetbyname(const char *name);
struct netent *getnetent(void);
struct protoent *getprotobyname(const char *name);
struct protoent *getprotobynumber(int proto);
struct protoent *getprotoent(void);
struct servent *getservbyname(const char *name, const char *proto);
struct servent *getservbyport(int port, const char *proto);
struct servent *getservent(void);
void sethostent(int stayopen);
void setnetent(int stayopen);
void setprotoent(int stayopen);
void setservent(int stayopen);

#endif /* _NETDB_H */
