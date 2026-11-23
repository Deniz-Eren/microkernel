/**
 * @file     tty.h
 * @brief    Ring 3 /dev/tty and PTY IPC protocol structures and opcodes
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

#define TTY_DATA_MAX 368u
#define TTY_POLL_MAX 32u

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
#define TTY_OP_GETPGRP 11u
#define TTY_OP_SETPGRP 12u
#define TTY_OP_GETSID 13u
#define TTY_OP_GETWINSIZE 14u
#define TTY_OP_SETWINSIZE 15u
#define TTY_OP_PTY_OPEN_MASTER 16u
#define TTY_OP_PTY_UNLOCK 17u
#define TTY_OP_PTY_OPEN_SLAVE 18u
#define TTY_OP_DUP 19u
#define TTY_OP_CLOSE 20u
#define TTY_OP_FIONREAD 21u
#define TTY_OP_POLL_MULTI 22u
#define TTY_OP_SCTTY 23u

typedef struct {
    uint32_t op;
    uint32_t handle;
    uint32_t flags;
    uint32_t arg;
    uint32_t len;
    uint32_t reserved;
    int64_t timeout_ns;
    union {
        uint8_t data[TTY_DATA_MAX];
        struct termios tio;
        struct winsize ws;
    } u;
} tty_req_t;

typedef struct {
    int32_t err;
    uint32_t handle;
    uint32_t len;
    int16_t revents;
    uint16_t reserved;
    union {
        uint8_t data[TTY_DATA_MAX];
        struct termios tio;
        struct winsize ws;
    } u;
} tty_rep_t;

typedef struct {
    uint32_t handle;
    int16_t events;
    int16_t revents;
} tty_poll_item_t;

typedef struct {
    uint32_t op; /* TTY_OP_POLL_MULTI */
    uint32_t nfds;
    int64_t timeout_ns;
    tty_poll_item_t items[TTY_POLL_MAX];
} tty_poll_req_t;

typedef struct {
    int32_t err;
    uint32_t ready;
    tty_poll_item_t items[TTY_POLL_MAX];
} tty_poll_rep_t;

_Static_assert(sizeof(tty_req_t) <= 512u, "tty_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(tty_rep_t) <= 512u, "tty_rep_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(tty_poll_req_t) <= 512u,
               "tty_poll_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(tty_poll_rep_t) <= 512u,
               "tty_poll_rep_t exceeds IPC_MSG_MAX");
