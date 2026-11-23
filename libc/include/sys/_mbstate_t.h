/**
 * @file     _mbstate_t.h
 * @brief    Shared mbstate_t definition for <uchar.h> and <wchar.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYS__MBSTATE_T_H
#define _SYS__MBSTATE_T_H

#include <stdint.h>

#ifndef _MBSTATE_T_DEFINED
#define _MBSTATE_T_DEFINED
typedef struct {
    uint32_t __codepoint;
    uint16_t __pending_low;
    uint8_t __expected;
    uint8_t __seen;
} mbstate_t;
#endif /* _MBSTATE_T_DEFINED */

#endif /* _SYS__MBSTATE_T_H */
