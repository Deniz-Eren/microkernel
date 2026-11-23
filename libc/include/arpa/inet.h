/**
 * @file     inet.h
 * @brief    POSIX.1-2024 Internet address manipulation APIs (<arpa/inet.h>)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ARPA_INET_H
#define _ARPA_INET_H

/* need:: ICD_LIBC_0017 */

#include <inttypes.h>
#include <netinet/in.h>

in_addr_t inet_addr(const char *cp);
char *inet_ntoa(struct in_addr in);
const char *inet_ntop(int af, const void *restrict src, char *restrict dst,
                      socklen_t size);
int inet_pton(int af, const char *restrict src, void *restrict dst);

#endif /* _ARPA_INET_H */
