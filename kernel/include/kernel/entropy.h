/**
 * @file     entropy.h
 * @brief    Kernel entropy generator and SYS_GETENTROPY syscall interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

long sys_getentropy(uint64_t buf_va, uint64_t len);
