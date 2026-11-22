/**
 * @file     uart_backend.h
 * @brief    Portable UART hardware backend interface for Ring 3 ttyserv
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

int tty_uart_init(uint32_t base, uint32_t irq, int chan);
void tty_uart_putc(uint8_t c);
void tty_uart_write(const uint8_t *buf, size_t len);
int tty_uart_getc(uint8_t *out);
