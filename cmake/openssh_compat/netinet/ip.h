/**
 * @file     ip.h
 * @brief    IPv4 QoS/TOS constants for OpenSSH cross-compilation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MICROKERNEL_OPENSSH_NETINET_IP_H
#define _MICROKERNEL_OPENSSH_NETINET_IP_H

#include <netinet/in_systm.h>

#define IPTOS_LOWDELAY 0x10
#define IPTOS_THROUGHPUT 0x08
#define IPTOS_RELIABILITY 0x04
#define IPTOS_MINCOST 0x02

#define IPTOS_DSCP_AF11 0x28
#define IPTOS_DSCP_AF12 0x30
#define IPTOS_DSCP_AF13 0x38
#define IPTOS_DSCP_AF21 0x48
#define IPTOS_DSCP_AF22 0x50
#define IPTOS_DSCP_AF23 0x58
#define IPTOS_DSCP_AF31 0x68
#define IPTOS_DSCP_AF32 0x70
#define IPTOS_DSCP_AF33 0x78
#define IPTOS_DSCP_AF41 0x88
#define IPTOS_DSCP_AF42 0x90
#define IPTOS_DSCP_AF43 0x98
#define IPTOS_DSCP_CS0 0x00
#define IPTOS_DSCP_CS1 0x20
#define IPTOS_DSCP_CS2 0x40
#define IPTOS_DSCP_CS3 0x60
#define IPTOS_DSCP_CS4 0x80
#define IPTOS_DSCP_CS5 0xA0
#define IPTOS_DSCP_CS6 0xC0
#define IPTOS_DSCP_CS7 0xE0
#define IPTOS_DSCP_EF 0xB8

#endif /* _MICROKERNEL_OPENSSH_NETINET_IP_H */
