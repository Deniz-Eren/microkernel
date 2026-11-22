/**
 * @file     printk.h
 * @brief    Kernel formatted serial logging interface
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdarg.h>
#include <stddef.h>

void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void printk_v(const char *fmt, va_list ap);
void console_write_locked(const char *s, size_t n);
void console_write_raw_locked(const char *s, size_t n);
void serial_set_quiet(int quiet);
