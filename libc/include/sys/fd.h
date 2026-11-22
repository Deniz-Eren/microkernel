/**
 * @file     fd.h
 * @brief    Kernel FD table ABI and Ring 3 /dev/pipe IPC protocol
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0003 */

#include <stdint.h>

#define PROC_MAX_FDS 32

#define FD_KIND_NONE 0u
#define FD_KIND_CONSOLE 1u
#define FD_KIND_SERVER 2u
#define FD_KIND_VFS 3u
#define FD_KIND_SHM 4u
#define FD_KIND_TTY 5u
#define FD_KIND_SOCK 6u

#define FD_FLAG_CLOEXEC 0x01u

/* SYS_FD_OP sub-operations. */
#define FD_OP_ALLOC 1u
#define FD_OP_GET 2u
#define FD_OP_CLOSE 3u
#define FD_OP_DUP 4u
#define FD_OP_DUP3 5u
#define FD_OP_SETFD 6u
#define FD_OP_SETFL 7u
#define FD_OP_LIST 8u

/* Kernel-to-server pulse sent on execve O_CLOEXEC close or process exit. */
#define PULSE_CODE_FD_CLOSE 0x4644u

/* Internal timer tick pulses for finite ppoll deadlines. */
#define PULSE_CODE_PIPE_TICK 0x5054u
#define PULSE_CODE_TTY_TICK 0x5454u
#define PULSE_CODE_SOCK_TICK 0x5354u

struct __k_fd_entry {
    uint8_t kind;
    uint8_t fd_flags;
    uint16_t status_flags;
    uint32_t srv_pid;
    uint32_t srv_chanid;
    uint32_t srv_handle;
};

/* Ring 3 /dev/pipe IPC protocol (fits within IPC_MSG_MAX = 512 bytes). */
#define PIPE_OP_CREATE 1u
#define PIPE_OP_DUP 2u
#define PIPE_OP_CLOSE 3u
#define PIPE_OP_READ 4u
#define PIPE_OP_WRITE 5u
#define PIPE_OP_POLL 6u
#define PIPE_OP_STATS 7u

#define PIPE_CHUNK_MAX 384u
#define PIPE_POLL_MAX 32u

typedef struct {
    uint32_t op;
    uint32_t handle;
    uint32_t len;
    uint32_t flags;
    uint8_t data[PIPE_CHUNK_MAX];
} pipe_req_t;

typedef struct {
    int32_t err;
    uint32_t handle_r;
    uint32_t handle_w;
    uint32_t len;
    uint8_t data[PIPE_CHUNK_MAX];
} pipe_rep_t;

typedef struct {
    uint32_t handle;
    int16_t events;
    int16_t revents;
} pipe_poll_item_t;

typedef struct {
    uint32_t op; /* PIPE_OP_POLL */
    uint32_t nfds;
    int64_t timeout_ns; /* < 0: infinite, 0: non-blocking, > 0: relative ns */
    pipe_poll_item_t items[PIPE_POLL_MAX];
} pipe_poll_req_t;

typedef struct {
    int32_t err;
    uint32_t ready;
    pipe_poll_item_t items[PIPE_POLL_MAX];
} pipe_poll_rep_t;
