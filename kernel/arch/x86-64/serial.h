/**
 * @file     serial.h
 * @brief    COM1 serial port driver declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s, size_t n);
void serial_write_raw(const char *s, size_t n);
