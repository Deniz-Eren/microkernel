/**
 * @file     if.h
 * @brief    POSIX.1-2024 network interface name/index APIs (<net/if.h>)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _NET_IF_H
#define _NET_IF_H

/* need:: ICD_LIBC_0017 */

#define IF_NAMESIZE 16

struct if_nameindex {
    unsigned if_index;
    char *if_name;
};

void if_freenameindex(struct if_nameindex *ptr);
char *if_indextoname(unsigned ifindex, char *ifname);
struct if_nameindex *if_nameindex(void);
unsigned if_nametoindex(const char *ifname);

#endif /* _NET_IF_H */
