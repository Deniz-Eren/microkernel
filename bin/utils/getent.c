/**
 * @file     getent.c
 * @brief    Ring 3 getent utility over libc NSS databases
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <grp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

/* need:: COMP_LIBC_0037 */

static int parse_u32(const char *s, uint32_t *out) {
    if (s == NULL || *s == '\0') {
        return 0;
    }
    uint64_t val = 0u;
    for (const char *p = s; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        val = val * 10u + (uint64_t)(*p - '0');
        if (val > 0xFFFFFFFFu) {
            return 0;
        }
    }
    *out = (uint32_t)val;
    return 1;
}

static int print_aliases(char *const *aliases) {
    if (aliases == NULL) {
        return 0;
    }
    for (size_t i = 0u; aliases[i] != NULL; i++) {
        if (printf(" %s", aliases[i]) < 0) {
            return 1;
        }
    }
    return 0;
}

static int print_passwd(const struct passwd *pw) {
    return (printf("%s:%s:%u:%u:%s:%s:%s\n", pw->pw_name, pw->pw_passwd,
                   (unsigned)pw->pw_uid, (unsigned)pw->pw_gid, pw->pw_gecos,
                   pw->pw_dir, pw->pw_shell) < 0)
               ? 1
               : 0;
}

static int do_passwd(int nkeys, char *const *keys) {
    if (nkeys == 0) {
        setpwent();
        int rc = 0;
        struct passwd *pw = NULL;
        while ((pw = getpwent()) != NULL) {
            if (print_passwd(pw) != 0) {
                rc = 1;
                break;
            }
        }
        endpwent();
        return rc;
    }
    int rc = 0;
    for (int i = 0; i < nkeys; i++) {
        struct passwd *pw = getpwnam(keys[i]);
        uint32_t uid = 0u;
        if (pw == NULL && parse_u32(keys[i], &uid)) {
            pw = getpwuid((uid_t)uid);
        }
        if (pw == NULL) {
            rc = 2;
            continue;
        }
        if (print_passwd(pw) != 0) {
            return 1;
        }
    }
    return rc;
}

static int print_group(const struct group *gr) {
    if (printf("%s:x:%u:", gr->gr_name, (unsigned)gr->gr_gid) < 0) {
        return 1;
    }
    if (gr->gr_mem != NULL) {
        for (size_t i = 0u; gr->gr_mem[i] != NULL; i++) {
            if (printf("%s%s", i == 0u ? "" : ",", gr->gr_mem[i]) < 0) {
                return 1;
            }
        }
    }
    return (printf("\n") < 0) ? 1 : 0;
}

static int do_group(int nkeys, char *const *keys) {
    if (nkeys == 0) {
        setgrent();
        int rc = 0;
        struct group *gr = NULL;
        while ((gr = getgrent()) != NULL) {
            if (print_group(gr) != 0) {
                rc = 1;
                break;
            }
        }
        endgrent();
        return rc;
    }
    int rc = 0;
    for (int i = 0; i < nkeys; i++) {
        struct group *gr = getgrnam(keys[i]);
        uint32_t gid = 0u;
        if (gr == NULL && parse_u32(keys[i], &gid)) {
            gr = getgrgid((gid_t)gid);
        }
        if (gr == NULL) {
            rc = 2;
            continue;
        }
        if (print_group(gr) != 0) {
            return 1;
        }
    }
    return rc;
}

static int print_host_addr(int family, const void *addr, const char *name,
                           char *const *aliases) {
    char buf[INET6_ADDRSTRLEN];
    if (inet_ntop(family, addr, buf, (socklen_t)sizeof(buf)) == NULL) {
        return 1;
    }
    if (printf("%s %s", buf, name) < 0 || print_aliases(aliases) != 0 ||
        printf("\n") < 0) {
        return 1;
    }
    return 0;
}

static int do_hosts(int nkeys, char *const *keys) {
    if (nkeys == 0) {
        sethostent(0);
        int rc = 0;
        struct hostent *he = NULL;
        while ((he = gethostent()) != NULL) {
            if (he->h_addr_list == NULL || he->h_addr_list[0] == NULL) {
                continue;
            }
            if (print_host_addr(he->h_addrtype, he->h_addr_list[0], he->h_name,
                                he->h_aliases) != 0) {
                rc = 1;
                break;
            }
        }
        endhostent();
        return rc;
    }
    int rc = 0;
    for (int i = 0; i < nkeys; i++) {
        struct addrinfo hints = {0};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_CANONNAME;
        struct addrinfo *ai = NULL;
        if (getaddrinfo(keys[i], NULL, &hints, &ai) != 0 || ai == NULL) {
            rc = 2;
            continue;
        }
        int printed = 0;
        for (struct addrinfo *cur = ai; cur != NULL; cur = cur->ai_next) {
            const void *addr = NULL;
            if (cur->ai_family == AF_INET) {
                const struct sockaddr_in *sin =
                    (const struct sockaddr_in *)(const void *)cur->ai_addr;
                addr = &sin->sin_addr;
            } else if (cur->ai_family == AF_INET6) {
                const struct sockaddr_in6 *sin6 =
                    (const struct sockaddr_in6 *)(const void *)cur->ai_addr;
                addr = &sin6->sin6_addr;
            }
            if (addr == NULL) {
                continue;
            }
            const char *label =
                (cur->ai_canonname != NULL && cur->ai_canonname[0] != '\0')
                    ? cur->ai_canonname
                    : keys[i];
            if (print_host_addr(cur->ai_family, addr, label, NULL) != 0) {
                freeaddrinfo(ai);
                return 1;
            }
            printed = 1;
        }
        freeaddrinfo(ai);
        if (!printed) {
            rc = 2;
        }
    }
    return rc;
}

static int print_serv(const struct servent *se) {
    if (printf("%s %u/%s", se->s_name, (unsigned)ntohs((uint16_t)se->s_port),
               se->s_proto) < 0 ||
        print_aliases(se->s_aliases) != 0 || printf("\n") < 0) {
        return 1;
    }
    return 0;
}

static int do_services(int nkeys, char *const *keys) {
    if (nkeys == 0) {
        setservent(0);
        int rc = 0;
        struct servent *se = NULL;
        while ((se = getservent()) != NULL) {
            if (print_serv(se) != 0) {
                rc = 1;
                break;
            }
        }
        endservent();
        return rc;
    }
    int rc = 0;
    for (int i = 0; i < nkeys; i++) {
        char svc_buf[64];
        const char *svc = keys[i];
        const char *proto = NULL;
        const char *slash = strchr(keys[i], '/');
        if (slash != NULL) {
            size_t len = (size_t)(slash - keys[i]);
            if (len == 0u || len >= sizeof(svc_buf) || slash[1] == '\0') {
                rc = 2;
                continue;
            }
            memcpy(svc_buf, keys[i], len);
            svc_buf[len] = '\0';
            svc = svc_buf;
            proto = slash + 1;
        }
        struct servent *se = getservbyname(svc, proto);
        uint32_t port = 0u;
        if (se == NULL && parse_u32(svc, &port) && port <= 65535u) {
            se = getservbyport((int)htons((uint16_t)port), proto);
        }
        if (se == NULL) {
            rc = 2;
            continue;
        }
        if (print_serv(se) != 0) {
            return 1;
        }
    }
    return rc;
}

static int print_proto(const struct protoent *pe) {
    if (printf("%s %d", pe->p_name, pe->p_proto) < 0 ||
        print_aliases(pe->p_aliases) != 0 || printf("\n") < 0) {
        return 1;
    }
    return 0;
}

static int do_protocols(int nkeys, char *const *keys) {
    if (nkeys == 0) {
        setprotoent(0);
        int rc = 0;
        struct protoent *pe = NULL;
        while ((pe = getprotoent()) != NULL) {
            if (print_proto(pe) != 0) {
                rc = 1;
                break;
            }
        }
        endprotoent();
        return rc;
    }
    int rc = 0;
    for (int i = 0; i < nkeys; i++) {
        struct protoent *pe = getprotobyname(keys[i]);
        uint32_t num = 0u;
        if (pe == NULL && parse_u32(keys[i], &num)) {
            pe = getprotobynumber((int)num);
        }
        if (pe == NULL) {
            rc = 2;
            continue;
        }
        if (print_proto(pe) != 0) {
            return 1;
        }
    }
    return rc;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: getent database [key ...]\n"
                        "databases: passwd group hosts services protocols\n");
        return 1;
    }
    const char *db = argv[1];
    int nkeys = argc - 2;
    char *const *keys = (nkeys > 0) ? &argv[2] : NULL;
    if (strcmp(db, "passwd") == 0) {
        return do_passwd(nkeys, keys);
    }
    if (strcmp(db, "group") == 0) {
        return do_group(nkeys, keys);
    }
    if (strcmp(db, "hosts") == 0) {
        return do_hosts(nkeys, keys);
    }
    if (strcmp(db, "services") == 0) {
        return do_services(nkeys, keys);
    }
    if (strcmp(db, "protocols") == 0) {
        return do_protocols(nkeys, keys);
    }
    fprintf(stderr, "getent: unknown database '%s'\n", db);
    return 1;
}
