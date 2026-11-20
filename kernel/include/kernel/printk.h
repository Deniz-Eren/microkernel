// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stdarg.h>
#include <stddef.h>

void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void printk_v(const char *fmt, va_list ap);
void serial_set_quiet(int quiet);
