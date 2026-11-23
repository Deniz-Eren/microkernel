/**
 * @file     inet.c
 * @brief    POSIX.1-2024 <arpa/inet.h>, <netinet/in.h>, and <net/if.h> APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/net.h>

/* need:: COMP_LIBC_0019 */

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

uint32_t htonl(uint32_t hostlong) {
    return __builtin_bswap32(hostlong);
}

uint16_t htons(uint16_t hostshort) {
    return __builtin_bswap16(hostshort);
}

uint32_t ntohl(uint32_t netlong) {
    return __builtin_bswap32(netlong);
}

uint16_t ntohs(uint16_t netshort) {
    return __builtin_bswap16(netshort);
}

static int parse_ipv4_strict(const char *src, uint8_t out[4]) {
    if (!src || *src == '\0') {
        return 0;
    }
    const char *p = src;
    for (int oct = 0; oct < 4; oct++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        if (*p == '0' && p[1] >= '0' && p[1] <= '9') {
            return 0; /* RFC/POSIX: reject leading zeros in inet_pton */
        }
        unsigned val = 0u;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10u + (unsigned)(*p - '0');
            if (val > 255u) {
                return 0;
            }
            p++;
            digits++;
        }
        if (digits == 0) {
            return 0;
        }
        out[oct] = (uint8_t)val;
        if (oct < 3) {
            if (*p != '.') {
                return 0;
            }
            p++;
        }
    }
    return (*p == '\0') ? 1 : 0;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static int parse_ipv6(const char *src, uint8_t out[16]) {
    if (!src || *src == '\0') {
        return 0;
    }
    uint16_t words[8] = {0};
    int nwords = 0;
    int compress_pos = -1;
    const char *p = src;

    if (p[0] == ':') {
        if (p[1] != ':') {
            return 0;
        }
        compress_pos = 0;
        p += 2;
        if (*p == '\0') {
            memset(out, 0, 16u);
            return 1;
        }
    }

    while (*p != '\0') {
        if (nwords >= 8) {
            return 0;
        }
        /* Check for embedded IPv4 suffix (e.g. ::ffff:127.0.0.1). */
        const char *scan = p;
        int has_dot = 0;
        while (*scan != '\0' && *scan != ':') {
            if (*scan == '.') {
                has_dot = 1;
                break;
            }
            scan++;
        }
        if (has_dot) {
            if (nwords > 6) {
                return 0;
            }
            uint8_t v4[4];
            if (!parse_ipv4_strict(p, v4)) {
                return 0;
            }
            words[nwords++] = (uint16_t)(((uint16_t)v4[0] << 8) | v4[1]);
            words[nwords++] = (uint16_t)(((uint16_t)v4[2] << 8) | v4[3]);
            p += strlen(p);
            break;
        }

        unsigned val = 0u;
        int digits = 0;
        while (digits < 4) {
            int hv = hex_val(*p);
            if (hv < 0) {
                break;
            }
            val = (val << 4) | (unsigned)hv;
            p++;
            digits++;
        }
        if (digits == 0) {
            return 0;
        }
        words[nwords++] = (uint16_t)val;

        if (*p == '\0') {
            break;
        }
        if (*p != ':') {
            return 0;
        }
        p++;
        if (*p == '\0') {
            return 0; /* trailing single ':' */
        }
        if (*p == ':') {
            if (compress_pos >= 0) {
                return 0; /* only one '::' allowed */
            }
            compress_pos = nwords;
            p++;
            if (*p == '\0') {
                break;
            }
        }
    }

    uint16_t final_w[8] = {0};
    if (compress_pos >= 0) {
        if (nwords >= 8) {
            return 0;
        }
        int head = compress_pos;
        int tail = nwords - compress_pos;
        for (int i = 0; i < head; i++) {
            final_w[i] = words[i];
        }
        for (int i = 0; i < tail; i++) {
            final_w[8 - tail + i] = words[compress_pos + i];
        }
    } else {
        if (nwords != 8) {
            return 0;
        }
        for (int i = 0; i < 8; i++) {
            final_w[i] = words[i];
        }
    }

    for (int i = 0; i < 8; i++) {
        out[2 * i] = (uint8_t)(final_w[i] >> 8);
        out[2 * i + 1] = (uint8_t)(final_w[i] & 0xFFu);
    }
    return 1;
}

int inet_pton(int af, const char *restrict src, void *restrict dst) {
    if (!src || !dst) {
        errno = EFAULT;
        return -1;
    }
    if (af == AF_INET) {
        return parse_ipv4_strict(src, (uint8_t *)dst);
    }
    if (af == AF_INET6) {
        return parse_ipv6(src, (uint8_t *)dst);
    }
    errno = EAFNOSUPPORT;
    return -1;
}

static const char *format_ipv4(const uint8_t b[4], char *dst, socklen_t size) {
    char tmp[INET_ADDRSTRLEN];
    int n = snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    if (n < 0 || (socklen_t)(n + 1) > size) {
        errno = ENOSPC;
        return NULL;
    }
    memcpy(dst, tmp, (size_t)n + 1u);
    return dst;
}

static const char *format_ipv6(const uint8_t b[16], char *dst, socklen_t size) {
    uint16_t w[8];
    for (int i = 0; i < 8; i++) {
        w[i] = (uint16_t)(((uint16_t)b[2 * i] << 8) | b[2 * i + 1]);
    }
    /* Find longest run of >= 2 zero 16-bit groups (RFC 5952). */
    int best_start = -1;
    int best_len = 0;
    int cur_start = -1;
    int cur_len = 0;
    for (int i = 0; i < 8; i++) {
        if (w[i] == 0u) {
            if (cur_start < 0) {
                cur_start = i;
                cur_len = 1;
            } else {
                cur_len++;
            }
            if (cur_len > best_len) {
                best_start = cur_start;
                best_len = cur_len;
            }
        } else {
            cur_start = -1;
            cur_len = 0;
        }
    }
    if (best_len < 2) {
        best_start = -1;
        best_len = 0;
    }

    char tmp[INET6_ADDRSTRLEN];
    size_t pos = 0u;
    int is_v4mapped = (w[0] == 0u && w[1] == 0u && w[2] == 0u && w[3] == 0u &&
                       w[4] == 0u && w[5] == 0xFFFFu);
    for (int i = 0; i < 8;) {
        if (i == best_start) {
            tmp[pos++] = ':';
            tmp[pos++] = ':';
            i += best_len;
            continue;
        }
        if (i > 0 && !(best_start >= 0 && i == best_start + best_len)) {
            tmp[pos++] = ':';
        }
        if (is_v4mapped && i == 6) {
            int n = snprintf(tmp + pos, sizeof(tmp) - pos, "%u.%u.%u.%u", b[12],
                             b[13], b[14], b[15]);
            if (n > 0) {
                pos += (size_t)n;
            }
            break;
        }
        int n = snprintf(tmp + pos, sizeof(tmp) - pos, "%x", (unsigned)w[i]);
        if (n > 0) {
            pos += (size_t)n;
        }
        i++;
    }
    tmp[pos] = '\0';
    if ((socklen_t)(pos + 1u) > size) {
        errno = ENOSPC;
        return NULL;
    }
    memcpy(dst, tmp, pos + 1u);
    return dst;
}

const char *inet_ntop(int af, const void *restrict src, char *restrict dst,
                      socklen_t size) {
    if (!src || !dst) {
        errno = EFAULT;
        return NULL;
    }
    if (af == AF_INET) {
        return format_ipv4((const uint8_t *)src, dst, size);
    }
    if (af == AF_INET6) {
        return format_ipv6((const uint8_t *)src, dst, size);
    }
    errno = EAFNOSUPPORT;
    return NULL;
}

char *inet_ntoa(struct in_addr in) {
    static char buf[INET_ADDRSTRLEN];
    uint8_t b[4];
    memcpy(b, &in.s_addr, 4u);
    (void)snprintf(buf, sizeof(buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return buf;
}

in_addr_t inet_addr(const char *cp) {
    if (!cp || *cp == '\0') {
        return INADDR_NONE;
    }
    uint32_t parts[4] = {0};
    int nparts = 0;
    const char *p = cp;
    while (*p != '\0') {
        if (nparts >= 4) {
            return INADDR_NONE;
        }
        char *end = NULL;
        errno = 0;
        unsigned long v = strtoul(p, &end, 0);
        if (end == p || errno == ERANGE) {
            return INADDR_NONE;
        }
        parts[nparts++] = (uint32_t)v;
        if (*end == '\0') {
            break;
        }
        if (*end != '.') {
            return INADDR_NONE;
        }
        p = end + 1;
        if (*p == '\0') {
            return INADDR_NONE;
        }
    }

    uint32_t host = 0u;
    if (nparts == 1) {
        host = parts[0];
    } else if (nparts == 2) {
        if (parts[0] > 0xFFu || parts[1] > 0xFFFFFFu) {
            return INADDR_NONE;
        }
        host = (parts[0] << 24) | parts[1];
    } else if (nparts == 3) {
        if (parts[0] > 0xFFu || parts[1] > 0xFFu || parts[2] > 0xFFFFu) {
            return INADDR_NONE;
        }
        host = (parts[0] << 24) | (parts[1] << 16) | parts[2];
    } else if (nparts == 4) {
        if (parts[0] > 0xFFu || parts[1] > 0xFFu || parts[2] > 0xFFu ||
            parts[3] > 0xFFu) {
            return INADDR_NONE;
        }
        host = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    } else {
        return INADDR_NONE;
    }
    return htonl(host);
}

/* --- <net/if.h> over /dev/net NET_OP_INFO --- */

static int query_net_info(net_info_t *out) {
    long ns = OpenConnection(1, 1);
    if (ns < 0) {
        return -1;
    }
    name_msg_t q = {0};
    name_reply_t nr = {0};
    q.type = NAME_MSG_QUERY;
    strncpy(q.path, "/dev/net", NAME_MAX_PATH - 1u);
    long r = SendMsg((int)ns, &q, sizeof(q), &nr, sizeof(nr));
    CloseConnection((int)ns);
    if (r < 0 || nr.err != 0 || nr.pid <= 0 || nr.chanid <= 0) {
        return -1;
    }
    long conn = OpenConnection((uint32_t)nr.pid, (uint32_t)nr.chanid);
    if (conn < 0) {
        return -1;
    }
    net_req_t req;
    net_rep_t rep;
    memset(&req, 0, offsetof(net_req_t, data));
    memset(&rep, 0, offsetof(net_rep_t, data));
    req.op = NET_OP_INFO;
    long sr = SendMsg((int)conn, &req, offsetof(net_req_t, data), &rep,
                      offsetof(net_rep_t, data));
    CloseConnection((int)conn);
    if (sr < 0 || rep.err != 0) {
        return -1;
    }
    *out = rep.u.info;
    return 0;
}

unsigned if_nametoindex(const char *ifname) {
    if (!ifname || *ifname == '\0') {
        return 0u;
    }
    net_info_t info;
    if (query_net_info(&info) != 0) {
        return (strcmp(ifname, "lo0") == 0) ? 1u : 0u;
    }
    for (uint32_t i = 0; i < info.if_count && i < NET_MAX_IFS; i++) {
        if (strcmp(info.ifs[i].name, ifname) == 0) {
            return info.ifs[i].index;
        }
    }
    return 0u;
}

char *if_indextoname(unsigned ifindex, char *ifname) {
    if (!ifname) {
        errno = EFAULT;
        return NULL;
    }
    if (ifindex == 0u) {
        errno = ENXIO;
        return NULL;
    }
    net_info_t info;
    if (query_net_info(&info) != 0) {
        if (ifindex == 1u) {
            memcpy(ifname, "lo0", 4u);
            return ifname;
        }
        errno = ENXIO;
        return NULL;
    }
    for (uint32_t i = 0; i < info.if_count && i < NET_MAX_IFS; i++) {
        if (info.ifs[i].index == ifindex) {
            strncpy(ifname, info.ifs[i].name, IF_NAMESIZE - 1u);
            ifname[IF_NAMESIZE - 1u] = '\0';
            return ifname;
        }
    }
    errno = ENXIO;
    return NULL;
}

struct if_nameindex *if_nameindex(void) {
    net_info_t info;
    uint32_t count = 0u;
    if (query_net_info(&info) == 0) {
        count = info.if_count > NET_MAX_IFS ? NET_MAX_IFS : info.if_count;
    } else {
        count = 1u;
        memset(&info, 0, sizeof(info));
        info.if_count = 1u;
        info.ifs[0].index = 1u;
        memcpy(info.ifs[0].name, "lo0", 4u);
    }

    size_t arr_sz = ((size_t)count + 1u) * sizeof(struct if_nameindex);
    size_t names_sz = (size_t)count * IF_NAMESIZE;
    uint8_t *block = (uint8_t *)calloc(1u, arr_sz + names_sz);
    if (!block) {
        errno = ENOMEM;
        return NULL;
    }
    struct if_nameindex *list = (struct if_nameindex *)block;
    char *name_pool = (char *)(block + arr_sz);
    for (uint32_t i = 0; i < count; i++) {
        char *dst = name_pool + (size_t)i * IF_NAMESIZE;
        strncpy(dst, info.ifs[i].name, IF_NAMESIZE - 1u);
        dst[IF_NAMESIZE - 1u] = '\0';
        list[i].if_index = info.ifs[i].index;
        list[i].if_name = dst;
    }
    list[count].if_index = 0u;
    list[count].if_name = NULL;
    return list;
}

void if_freenameindex(struct if_nameindex *ptr) {
    free(ptr);
}
