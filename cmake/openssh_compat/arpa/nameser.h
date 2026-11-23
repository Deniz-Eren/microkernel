/**
 * @file     nameser.h
 * @brief    Minimal arpa/nameser.h stub for OpenSSH cross-compilation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _MICROKERNEL_OPENSSH_ARPA_NAMESER_H
#define _MICROKERNEL_OPENSSH_ARPA_NAMESER_H

#define NS_MAXDNAME 1025
#define NS_MAXCDNAME 255
#define NS_PACKETSZ 512
#define NS_HFIXEDSZ 12
#define C_IN 1
#define T_SSHFP 44
#define T_RRSIG 46

#endif /* _MICROKERNEL_OPENSSH_ARPA_NAMESER_H */
