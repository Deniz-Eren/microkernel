// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#pragma once
#include <stddef.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s, size_t n);
