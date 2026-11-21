/**
 * @file     un.h
 * @brief    POSIX.1-2024 UNIX domain socket address structure
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS_UN_H
#define _SYS_UN_H

/* need:: ICD_IPC_0013 */

#include <stdint.h>

#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef uint16_t sa_family_t;
#endif

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

#endif /* _SYS_UN_H */
