/**
 * @file     sock.h
 * @brief    Ring 3 /dev/sock IPC protocol structures and operation codes
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0012 */

#include <stdint.h>

#define SOCK_PATH_MAX 108u
#define SOCK_DATA_MAX 360u
#define SOCK_DGRAM_DATA_MAX 256u
#define SOCK_POLL_MAX 32u

#define SOCK_OP_CREATE 1u
#define SOCK_OP_PAIR 2u
#define SOCK_OP_DUP 3u
#define SOCK_OP_CLOSE 4u
#define SOCK_OP_BIND 5u
#define SOCK_OP_LISTEN 6u
#define SOCK_OP_ACCEPT 7u
#define SOCK_OP_CONNECT 8u
#define SOCK_OP_SEND 9u
#define SOCK_OP_RECV 10u
#define SOCK_OP_SHUTDOWN 11u
#define SOCK_OP_GETNAME 12u
#define SOCK_OP_GETOPT 13u
#define SOCK_OP_SETOPT 14u
#define SOCK_OP_POLL 15u
#define SOCK_OP_STATS 16u

typedef struct {
    uint32_t op;
    uint32_t handle;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t flags;
    uint32_t len;
    union {
        uint8_t data[SOCK_DATA_MAX];
        char path[SOCK_PATH_MAX];
        struct {
            char path[SOCK_PATH_MAX];
            uint8_t data[SOCK_DGRAM_DATA_MAX];
        } dgram;
    } u;
} sock_req_t;

typedef struct {
    int32_t err;
    uint32_t handle;
    uint32_t handle2;
    uint32_t len;
    int32_t val;
    uint32_t reserved;
    union {
        uint8_t data[SOCK_DATA_MAX];
        char path[SOCK_PATH_MAX];
        struct {
            char path[SOCK_PATH_MAX];
            uint8_t data[SOCK_DGRAM_DATA_MAX];
        } dgram;
    } u;
} sock_rep_t;

typedef struct {
    uint32_t handle;
    int16_t events;
    int16_t revents;
} sock_poll_item_t;

typedef struct {
    uint32_t op; /* SOCK_OP_POLL */
    uint32_t nfds;
    int64_t timeout_ns; /* < 0: infinite, 0: non-blocking, > 0: relative ns */
    sock_poll_item_t items[SOCK_POLL_MAX];
} sock_poll_req_t;

typedef struct {
    int32_t err;
    uint32_t ready;
    sock_poll_item_t items[SOCK_POLL_MAX];
} sock_poll_rep_t;

_Static_assert(sizeof(sock_req_t) <= 512u, "sock_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(sock_rep_t) <= 512u, "sock_rep_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(sock_poll_req_t) <= 512u,
               "sock_poll_req_t exceeds IPC_MSG_MAX");
_Static_assert(sizeof(sock_poll_rep_t) <= 512u,
               "sock_poll_rep_t exceeds IPC_MSG_MAX");
