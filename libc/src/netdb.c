/**
 * @file     netdb.c
 * @brief    POSIX.1-2024 <netdb.h> database and resolver implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

/* need:: COMP_LIBC_0019 */

typedef struct {
    const char *name;
    int family;
    uint8_t addr[16];
} static_host_t;

static const static_host_t g_hosts[] = {
    {"localhost", AF_INET, {127u, 0u, 0u, 1u}},
    {"localhost",
     AF_INET6,
     {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u}},
    {"ip6-localhost",
     AF_INET6,
     {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u}},
    {"gateway", AF_INET, {10u, 0u, 2u, 2u}},
    {"microkernel", AF_INET, {10u, 0u, 2u, 15u}},
};

typedef struct {
    const char *name;
    uint32_t net;
} static_net_t;

static const static_net_t g_nets[] = {
    {"loopback", 0x7F000000u},
    {"localnet", 0x0A000200u},
};

typedef struct {
    const char *name;
    int proto;
} static_proto_t;

static const static_proto_t g_protos[] = {
    {"ip", IPPROTO_IP},   {"icmp", IPPROTO_ICMP}, {"tcp", IPPROTO_TCP},
    {"udp", IPPROTO_UDP}, {"ipv6", IPPROTO_IPV6}, {"ipv6-icmp", IPPROTO_ICMPV6},
};

typedef struct {
    const char *name;
    uint16_t port;
    const char *proto;
} static_serv_t;

static const static_serv_t g_servs[] = {
    {"echo", 7u, "tcp"},    {"echo", 7u, "udp"},     {"discard", 9u, "tcp"},
    {"discard", 9u, "udp"}, {"daytime", 13u, "tcp"}, {"ftp", 21u, "tcp"},
    {"ssh", 22u, "tcp"},    {"telnet", 23u, "tcp"},  {"smtp", 25u, "tcp"},
    {"domain", 53u, "tcp"}, {"domain", 53u, "udp"},  {"http", 80u, "tcp"},
    {"ntp", 123u, "udp"},   {"https", 443u, "tcp"},
};

static size_t g_host_idx;
static size_t g_net_idx;
static size_t g_proto_idx;
static size_t g_serv_idx;

static char *g_empty_aliases[1] = {NULL};

/* --- Host database --- */

void sethostent(int stayopen) {
    (void)stayopen;
    g_host_idx = 0u;
}

void endhostent(void) {
    g_host_idx = 0u;
}

struct hostent *gethostent(void) {
    static struct hostent he;
    static char name_buf[64];
    static uint8_t addr_buf[16];
    static char *addr_list[2];

    size_t count = sizeof(g_hosts) / sizeof(g_hosts[0]);
    if (g_host_idx >= count) {
        return NULL;
    }
    const static_host_t *h = &g_hosts[g_host_idx++];
    strncpy(name_buf, h->name, sizeof(name_buf) - 1u);
    name_buf[sizeof(name_buf) - 1u] = '\0';
    int len = (h->family == AF_INET6) ? 16 : 4;
    memcpy(addr_buf, h->addr, (size_t)len);
    addr_list[0] = (char *)addr_buf;
    addr_list[1] = NULL;

    he.h_name = name_buf;
    he.h_aliases = g_empty_aliases;
    he.h_addrtype = h->family;
    he.h_length = len;
    he.h_addr_list = addr_list;
    return &he;
}

/* --- Network database --- */

void setnetent(int stayopen) {
    (void)stayopen;
    g_net_idx = 0u;
}

void endnetent(void) {
    g_net_idx = 0u;
}

static struct netent *fill_netent(const static_net_t *n) {
    static struct netent ne;
    static char name_buf[64];
    strncpy(name_buf, n->name, sizeof(name_buf) - 1u);
    name_buf[sizeof(name_buf) - 1u] = '\0';
    ne.n_name = name_buf;
    ne.n_aliases = g_empty_aliases;
    ne.n_addrtype = AF_INET;
    ne.n_net = n->net;
    return &ne;
}

struct netent *getnetent(void) {
    size_t count = sizeof(g_nets) / sizeof(g_nets[0]);
    if (g_net_idx >= count) {
        return NULL;
    }
    return fill_netent(&g_nets[g_net_idx++]);
}

struct netent *getnetbyname(const char *name) {
    if (!name) {
        return NULL;
    }
    size_t count = sizeof(g_nets) / sizeof(g_nets[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(g_nets[i].name, name) == 0) {
            return fill_netent(&g_nets[i]);
        }
    }
    return NULL;
}

struct netent *getnetbyaddr(uint32_t net, int type) {
    if (type != AF_INET) {
        return NULL;
    }
    size_t count = sizeof(g_nets) / sizeof(g_nets[0]);
    for (size_t i = 0; i < count; i++) {
        if (g_nets[i].net == net) {
            return fill_netent(&g_nets[i]);
        }
    }
    return NULL;
}

/* --- Protocol database --- */

void setprotoent(int stayopen) {
    (void)stayopen;
    g_proto_idx = 0u;
}

void endprotoent(void) {
    g_proto_idx = 0u;
}

static struct protoent *fill_protoent(const static_proto_t *p) {
    static struct protoent pe;
    static char name_buf[32];
    strncpy(name_buf, p->name, sizeof(name_buf) - 1u);
    name_buf[sizeof(name_buf) - 1u] = '\0';
    pe.p_name = name_buf;
    pe.p_aliases = g_empty_aliases;
    pe.p_proto = p->proto;
    return &pe;
}

struct protoent *getprotoent(void) {
    size_t count = sizeof(g_protos) / sizeof(g_protos[0]);
    if (g_proto_idx >= count) {
        return NULL;
    }
    return fill_protoent(&g_protos[g_proto_idx++]);
}

struct protoent *getprotobyname(const char *name) {
    if (!name) {
        return NULL;
    }
    size_t count = sizeof(g_protos) / sizeof(g_protos[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(g_protos[i].name, name) == 0) {
            return fill_protoent(&g_protos[i]);
        }
    }
    return NULL;
}

struct protoent *getprotobynumber(int proto) {
    size_t count = sizeof(g_protos) / sizeof(g_protos[0]);
    for (size_t i = 0; i < count; i++) {
        if (g_protos[i].proto == proto) {
            return fill_protoent(&g_protos[i]);
        }
    }
    return NULL;
}

/* --- Service database --- */

void setservent(int stayopen) {
    (void)stayopen;
    g_serv_idx = 0u;
}

void endservent(void) {
    g_serv_idx = 0u;
}

static struct servent *fill_servent(const static_serv_t *s) {
    static struct servent se;
    static char name_buf[32];
    static char proto_buf[16];
    strncpy(name_buf, s->name, sizeof(name_buf) - 1u);
    name_buf[sizeof(name_buf) - 1u] = '\0';
    strncpy(proto_buf, s->proto, sizeof(proto_buf) - 1u);
    proto_buf[sizeof(proto_buf) - 1u] = '\0';
    se.s_name = name_buf;
    se.s_aliases = g_empty_aliases;
    se.s_port = (int)htons(s->port);
    se.s_proto = proto_buf;
    return &se;
}

struct servent *getservent(void) {
    size_t count = sizeof(g_servs) / sizeof(g_servs[0]);
    if (g_serv_idx >= count) {
        return NULL;
    }
    return fill_servent(&g_servs[g_serv_idx++]);
}

struct servent *getservbyname(const char *name, const char *proto) {
    if (!name) {
        return NULL;
    }
    size_t count = sizeof(g_servs) / sizeof(g_servs[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(g_servs[i].name, name) == 0 &&
            (!proto || strcasecmp(g_servs[i].proto, proto) == 0)) {
            return fill_servent(&g_servs[i]);
        }
    }
    return NULL;
}

struct servent *getservbyport(int port, const char *proto) {
    uint16_t host_port = ntohs((uint16_t)port);
    size_t count = sizeof(g_servs) / sizeof(g_servs[0]);
    for (size_t i = 0; i < count; i++) {
        if (g_servs[i].port == host_port &&
            (!proto || strcasecmp(g_servs[i].proto, proto) == 0)) {
            return fill_servent(&g_servs[i]);
        }
    }
    return NULL;
}

/* --- gai_strerror, getaddrinfo, freeaddrinfo, getnameinfo --- */

const char *gai_strerror(int ecode) {
    switch (ecode) {
    case 0:
        return "Success";
    case EAI_AGAIN:
        return "Temporary failure in name resolution";
    case EAI_BADFLAGS:
        return "Invalid value for ai_flags";
    case EAI_FAIL:
        return "Non-recoverable failure in name resolution";
    case EAI_FAMILY:
        return "ai_family not supported";
    case EAI_MEMORY:
        return "Memory allocation failure";
    case EAI_NONAME:
        return "Name or service not known";
    case EAI_SERVICE:
        return "Servname not supported for ai_socktype";
    case EAI_SOCKTYPE:
        return "ai_socktype not supported";
    case EAI_SYSTEM:
        return "System error";
    case EAI_OVERFLOW:
        return "Argument buffer overflow";
    default:
        return "Unknown error";
    }
}

void freeaddrinfo(struct addrinfo *ai) {
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        free(ai->ai_addr);
        free(ai->ai_canonname);
        free(ai);
        ai = next;
    }
}

static struct addrinfo *alloc_ai_node(int family, int socktype, int proto,
                                      const uint8_t *ip, uint16_t port,
                                      const char *canon) {
    struct addrinfo *ai = (struct addrinfo *)calloc(1u, sizeof(*ai));
    if (!ai) {
        return NULL;
    }
    if (family == AF_INET) {
        struct sockaddr_in *sin =
            (struct sockaddr_in *)calloc(1u, sizeof(*sin));
        if (!sin) {
            free(ai);
            return NULL;
        }
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port);
        memcpy(&sin->sin_addr.s_addr, ip, 4u);
        ai->ai_addr = (struct sockaddr *)sin;
        ai->ai_addrlen = (socklen_t)sizeof(*sin);
    } else {
        struct sockaddr_in6 *sin6 =
            (struct sockaddr_in6 *)calloc(1u, sizeof(*sin6));
        if (!sin6) {
            free(ai);
            return NULL;
        }
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        memcpy(sin6->sin6_addr.s6_addr, ip, 16u);
        ai->ai_addr = (struct sockaddr *)sin6;
        ai->ai_addrlen = (socklen_t)sizeof(*sin6);
    }
    if (canon) {
        size_t clen = strlen(canon) + 1u;
        ai->ai_canonname = (char *)malloc(clen);
        if (!ai->ai_canonname) {
            free(ai->ai_addr);
            free(ai);
            return NULL;
        }
        memcpy(ai->ai_canonname, canon, clen);
    }
    ai->ai_family = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = proto;
    return ai;
}

static int parse_service(const char *servname, int flags, int req_socktype,
                         uint16_t *port_out, int *st_out, int *proto_out) {
    if (!servname || *servname == '\0') {
        *port_out = 0u;
        *st_out = (req_socktype != 0) ? req_socktype : SOCK_STREAM;
        *proto_out = (*st_out == SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP;
        return 0;
    }
    /* Numeric decimal port check. */
    const char *p = servname;
    int all_digits = 1;
    unsigned long val = 0ul;
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            all_digits = 0;
            break;
        }
        val = val * 10ul + (unsigned long)(*p - '0');
        if (val > 65535ul) {
            return EAI_NONAME;
        }
        p++;
    }
    if (all_digits) {
        *port_out = (uint16_t)val;
        *st_out = (req_socktype != 0) ? req_socktype : SOCK_STREAM;
        *proto_out = (*st_out == SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP;
        return 0;
    }
    if ((flags & AI_NUMERICSERV) != 0) {
        return EAI_NONAME;
    }
    const char *want_proto = NULL;
    if (req_socktype == SOCK_STREAM) {
        want_proto = "tcp";
    } else if (req_socktype == SOCK_DGRAM) {
        want_proto = "udp";
    }
    struct servent *se = getservbyname(servname, want_proto);
    if (!se) {
        return EAI_SERVICE;
    }
    *port_out = ntohs((uint16_t)se->s_port);
    if (strcmp(se->s_proto, "udp") == 0) {
        *st_out = SOCK_DGRAM;
        *proto_out = IPPROTO_UDP;
    } else {
        *st_out = SOCK_STREAM;
        *proto_out = IPPROTO_TCP;
    }
    return 0;
}

static int lookup_etc_hosts(const char *name, int want_family,
                            uint8_t out_ip[16], int *out_family) {
    FILE *fp = fopen("/etc/hosts", "r");
    if (!fp) {
        return 0;
    }
    char line[256];
    int found = 0;
    while (fgets(line, (int)sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0' || *p == '#' || *p == '\n') {
            continue;
        }
        char *ip_tok = p;
        while (*p != '\0' && !isspace((unsigned char)*p) && *p != '#') {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }
        *p++ = '\0';

        uint8_t ip[16] = {0};
        int fam = 0;
        if (inet_pton(AF_INET, ip_tok, ip) == 1) {
            fam = AF_INET;
        } else if (inet_pton(AF_INET6, ip_tok, ip) == 1) {
            fam = AF_INET6;
        } else {
            continue;
        }
        if (want_family != AF_UNSPEC && want_family != fam) {
            continue;
        }
        while (*p != '\0' && *p != '#') {
            while (*p != '\0' && isspace((unsigned char)*p)) {
                p++;
            }
            if (*p == '\0' || *p == '#') {
                break;
            }
            char *h_tok = p;
            while (*p != '\0' && !isspace((unsigned char)*p) && *p != '#') {
                p++;
            }
            char saved = *p;
            *p = '\0';
            if (strcasecmp(h_tok, name) == 0) {
                memcpy(out_ip, ip, 16u);
                *out_family = fam;
                found = 1;
                break;
            }
            *p = saved;
        }
        if (found) {
            break;
        }
    }
    fclose(fp);
    return found;
}

int getaddrinfo(const char *restrict nodename, const char *restrict servname,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res) {
    if (!res) {
        return EAI_FAIL;
    }
    *res = NULL;
    if (!nodename && !servname) {
        return EAI_NONAME;
    }

    int flags = 0;
    int family = AF_UNSPEC;
    int socktype = 0;
    int protocol = 0;
    if (hints) {
        flags = hints->ai_flags;
        family = hints->ai_family;
        socktype = hints->ai_socktype;
        protocol = hints->ai_protocol;
    }
    const int valid_flags = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST |
                            AI_NUMERICSERV | AI_V4MAPPED | AI_ALL |
                            AI_ADDRCONFIG;
    if ((flags & ~valid_flags) != 0) {
        return EAI_BADFLAGS;
    }
    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) {
        return EAI_FAMILY;
    }
    if (socktype != 0 && socktype != SOCK_STREAM && socktype != SOCK_DGRAM) {
        return EAI_SOCKTYPE;
    }

    uint16_t port = 0u;
    int eff_st = socktype;
    int eff_proto = protocol;
    int src =
        parse_service(servname, flags, socktype, &port, &eff_st, &eff_proto);
    if (src != 0) {
        return src;
    }
    if (protocol != 0) {
        eff_proto = protocol;
    }

    struct addrinfo *head = NULL;
    struct addrinfo **tail = &head;

    /* 1. NULL nodename: wildcard (AI_PASSIVE) or loopback. */
    if (!nodename) {
        if (family == AF_UNSPEC || family == AF_INET) {
            uint8_t ip4[4] = {127u, 0u, 0u, 1u};
            if ((flags & AI_PASSIVE) != 0) {
                memset(ip4, 0, sizeof(ip4));
            }
            struct addrinfo *n =
                alloc_ai_node(AF_INET, eff_st, eff_proto, ip4, port, NULL);
            if (!n) {
                freeaddrinfo(head);
                return EAI_MEMORY;
            }
            *tail = n;
            tail = &n->ai_next;
        }
        if (family == AF_UNSPEC || family == AF_INET6) {
            uint8_t ip6[16] = {0};
            if ((flags & AI_PASSIVE) == 0) {
                ip6[15] = 1u;
            }
            struct addrinfo *n =
                alloc_ai_node(AF_INET6, eff_st, eff_proto, ip6, port, NULL);
            if (!n) {
                freeaddrinfo(head);
                return EAI_MEMORY;
            }
            *tail = n;
            tail = &n->ai_next;
        }
        *res = head;
        return 0;
    }

    /* 2. Numeric IPv4 or IPv6 literal. */
    uint8_t lit[16] = {0};
    if (inet_pton(AF_INET, nodename, lit) == 1) {
        if (family == AF_INET6) {
            if ((flags & AI_V4MAPPED) != 0) {
                uint8_t mapped[16] = {0};
                mapped[10] = 0xFFu;
                mapped[11] = 0xFFu;
                memcpy(mapped + 12u, lit, 4u);
                head = alloc_ai_node(AF_INET6, eff_st, eff_proto, mapped, port,
                                     (flags & AI_CANONNAME) ? nodename : NULL);
                if (!head) {
                    return EAI_MEMORY;
                }
                *res = head;
                return 0;
            }
            return EAI_NONAME;
        }
        head = alloc_ai_node(AF_INET, eff_st, eff_proto, lit, port,
                             (flags & AI_CANONNAME) ? nodename : NULL);
        if (!head) {
            return EAI_MEMORY;
        }
        *res = head;
        return 0;
    }
    if (inet_pton(AF_INET6, nodename, lit) == 1) {
        if (family == AF_INET) {
            return EAI_NONAME;
        }
        head = alloc_ai_node(AF_INET6, eff_st, eff_proto, lit, port,
                             (flags & AI_CANONNAME) ? nodename : NULL);
        if (!head) {
            return EAI_MEMORY;
        }
        *res = head;
        return 0;
    }

    if ((flags & AI_NUMERICHOST) != 0) {
        return EAI_NONAME;
    }

    /* 3. /etc/hosts lookup (if present). */
    int etc_fam = 0;
    if (lookup_etc_hosts(nodename, family, lit, &etc_fam)) {
        head = alloc_ai_node(etc_fam, eff_st, eff_proto, lit, port,
                             (flags & AI_CANONNAME) ? nodename : NULL);
        if (!head) {
            return EAI_MEMORY;
        }
        *res = head;
        return 0;
    }

    /* 4. Built-in static host table. */
    size_t hcount = sizeof(g_hosts) / sizeof(g_hosts[0]);
    for (size_t i = 0; i < hcount; i++) {
        if (strcasecmp(g_hosts[i].name, nodename) != 0) {
            continue;
        }
        if (family != AF_UNSPEC && g_hosts[i].family != family) {
            continue;
        }
        const char *canon =
            (!head && (flags & AI_CANONNAME) != 0) ? g_hosts[i].name : NULL;
        struct addrinfo *n = alloc_ai_node(g_hosts[i].family, eff_st, eff_proto,
                                           g_hosts[i].addr, port, canon);
        if (!n) {
            freeaddrinfo(head);
            return EAI_MEMORY;
        }
        *tail = n;
        tail = &n->ai_next;
    }

    if (!head) {
        return EAI_NONAME;
    }
    *res = head;
    return 0;
}

int getnameinfo(const struct sockaddr *restrict sa, socklen_t salen,
                char *restrict node, socklen_t nodelen, char *restrict service,
                socklen_t servicelen, int flags) {
    if (!sa) {
        return EAI_FAIL;
    }
    const void *ip_ptr = NULL;
    uint16_t port_net = 0u;
    int fam = sa->sa_family;
    if (fam == AF_INET) {
        if (salen < (socklen_t)sizeof(struct sockaddr_in)) {
            return EAI_FAMILY;
        }
        const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;
        ip_ptr = &sin->sin_addr;
        port_net = sin->sin_port;
    } else if (fam == AF_INET6) {
        if (salen < (socklen_t)sizeof(struct sockaddr_in6)) {
            return EAI_FAMILY;
        }
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)sa;
        ip_ptr = &sin6->sin6_addr;
        port_net = sin6->sin6_port;
    } else {
        return EAI_FAMILY;
    }

    if (node && nodelen > 0u) {
        const char *matched_name = NULL;
        if ((flags & NI_NUMERICHOST) == 0) {
            size_t ip_sz = (fam == AF_INET6) ? 16u : 4u;
            size_t hcount = sizeof(g_hosts) / sizeof(g_hosts[0]);
            for (size_t i = 0; i < hcount; i++) {
                if (g_hosts[i].family == fam &&
                    memcmp(g_hosts[i].addr, ip_ptr, ip_sz) == 0) {
                    matched_name = g_hosts[i].name;
                    break;
                }
            }
        }
        if (matched_name) {
            size_t nlen = strlen(matched_name);
            if ((socklen_t)(nlen + 1u) > nodelen) {
                return EAI_OVERFLOW;
            }
            memcpy(node, matched_name, nlen + 1u);
        } else {
            if ((flags & NI_NAMEREQD) != 0) {
                return EAI_NONAME;
            }
            if (!inet_ntop(fam, ip_ptr, node, nodelen)) {
                return EAI_OVERFLOW;
            }
        }
    }

    if (service && servicelen > 0u) {
        struct servent *se = NULL;
        if ((flags & NI_NUMERICSERV) == 0) {
            se = getservbyport((int)port_net,
                               (flags & NI_DGRAM) ? "udp" : "tcp");
        }
        if (se) {
            size_t slen = strlen(se->s_name);
            if ((socklen_t)(slen + 1u) > servicelen) {
                return EAI_OVERFLOW;
            }
            memcpy(service, se->s_name, slen + 1u);
        } else {
            char tmp[16];
            int n = snprintf(tmp, sizeof(tmp), "%u", (unsigned)ntohs(port_net));
            if (n < 0 || (socklen_t)(n + 1) > servicelen) {
                return EAI_OVERFLOW;
            }
            memcpy(service, tmp, (size_t)n + 1u);
        }
    }
    return 0;
}
