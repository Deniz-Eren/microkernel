/**
 * @file     tty.h
 * @brief    Ring 3 /dev/tty IPC protocol structures and operation codes
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0010 */

#include <stdint.h>
#include <termios.h>

#define TTY_DATA_MAX 384u

#define TTY_OP_READ 1u
#define TTY_OP_WRITE 2u
#define TTY_OP_GETATTR 3u
#define TTY_OP_SETATTR 4u
#define TTY_OP_FLUSH 5u
#define TTY_OP_POLL 6u
#define TTY_OP_INJECT 7u
#define TTY_OP_DRAIN 8u
#define TTY_OP_FLOW 9u
#define TTY_OP_SENDBREAK 10u

typedef struct {
    uint32_t op;
    uint32_t flags;
    uint32_t arg;
    uint32_t len;
    int64_t timeout_ns;
    union {
        uint8_t data[TTY_DATA_MAX];
        struct termios tio;
    } u;
} tty_req_t;

typedef struct {
    int32_t err;
    uint32_t len;
    int16_t revents;
    uint16_t reserved;
    union {
        uint8_t data[TTY_DATA_MAX];
        struct termios tio;
    } u;
} tty_rep_t;

_Static_assert(sizeof(tty_req_t) <= 512u, "tty_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(tty_rep_t) <= 512u, "tty_rep_t exceeds IPC_MSG_MAX");
