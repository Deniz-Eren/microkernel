/**
 * @file     panic.c
 * @brief    Kernel fatal panic handler halting the CPU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdarg.h>
#include <kernel/printk.h>
#include <kernel/panic.h>

void panic(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printk_v(fmt, ap);
    va_end(ap);
    printk("\n!!! panic — system halted\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
