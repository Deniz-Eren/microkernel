/**
 * @file     blk.h
 * @brief    Ring 3 block device IPC protocol (/dev/blk)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

/* need:: ICD_IPC_0015 */

#include <stdint.h>

#define BLK_SECTOR_SIZE 512u
#define BLK_MAX_SECTORS 4u
#define BLK_MAX_BYTES (BLK_SECTOR_SIZE * BLK_MAX_SECTORS)

#define BLK_OP_INFO 1u
#define BLK_OP_READ 2u
#define BLK_OP_WRITE 3u

typedef struct {
    uint32_t op;
    uint32_t count; /* sectors (1..BLK_MAX_SECTORS) */
    uint64_t lba;   /* starting 512-byte sector index */
    uint8_t data[BLK_MAX_BYTES];
} blk_req_t;

typedef struct {
    int32_t err;          /* 0 on success, negative errno on failure */
    uint32_t sector_size; /* bytes per sector (512) */
    uint64_t num_sectors; /* total capacity in sectors (BLK_OP_INFO) */
    uint64_t irq_count;   /* hardware IRQ pulses serviced */
    uint32_t bytes;       /* bytes transferred */
    uint32_t reserved;
    uint8_t data[BLK_MAX_BYTES];
} blk_rep_t;
