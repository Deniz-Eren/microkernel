/**
 * @file     serial.c
 * @brief    COM1 serial port initialization and character output
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "serial.h"
#include "io.h"

/* need:: COMP_BOOT_0001 */

#define COM1 0x3F8

/* Early serial runs before the x86-64-v3 CPUID gate, so keep it baseline-safe.
 */
#define SERIAL_EARLY_FN                                                        \
    __attribute__((target("arch=x86-64,no-mmx,no-sse,no-sse2,no-avx,"          \
                          "no-avx2,no-fma,no-f16c,no-bmi,no-bmi2,no-lzcnt,"    \
                          "no-popcnt,no-movbe")))

SERIAL_EARLY_FN void serial_init(void) {
    outb(COM1 + 1, 0x00); /* disable interrupts */
    outb(COM1 + 3, 0x80); /* DLAB on */
    outb(COM1 + 0, 0x01); /* divisor 1 -> 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8N1, DLAB off */
    outb(COM1 + 2, 0xC7); /* FIFO on, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* DTR | RTS | OUT2 */
}

SERIAL_EARLY_FN void serial_putc(char c) {
    if (c == '\n') {
        while (!(inb(COM1 + 5) & 0x20)) {
        }
        outb(COM1, '\r');
    }
    while (!(inb(COM1 + 5) & 0x20)) {
    }
    outb(COM1, (uint8_t)c);
}

SERIAL_EARLY_FN void serial_write(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        serial_putc(s[i]);
    }
}
