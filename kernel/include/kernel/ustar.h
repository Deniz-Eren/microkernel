/**
 * @file     ustar.h
 * @brief    POSIX ustar boot archive reader interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t phys;
    uint64_t end;
    uint64_t cursor;
    const char *name;
    uint64_t data_phys;
    uint64_t size;
} ustar_iter_t;

int ustar_open(ustar_iter_t *it, uint64_t phys, uint64_t size);
int ustar_next(ustar_iter_t *it); /* 1 = entry available, 0 = done */
const void *ustar_data(const ustar_iter_t *it);
