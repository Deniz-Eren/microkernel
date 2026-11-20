/**
 * @file     panic.h
 * @brief    Kernel fatal panic interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

void panic(const char *fmt, ...)
    __attribute__((format(printf, 1, 2), noreturn));
