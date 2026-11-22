/**
 * @file     uart_16550.c
 * @brief    x86-64 16550A COM UART hardware backend for Ring 3 ttyserv
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <sys/fd.h>
#include <sys/intr.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include "uart_backend.h"

/* need:: COMP_IPC_0010 */

static uint16_t g_uart_port = 0x3F8u;

void tty_uart_write(const uint8_t *buf, size_t len) {
    if (!buf || len == 0u) {
        return;
    }
    /* Serialize COM1 TX against Ring 0 printk() under the kernel console lock
     * when running on the default COM1 port, preserving Ring 3 OPOST/ONLCR. */
    if (g_uart_port == 0x3F8u) {
        size_t off = 0u;
        while (off < len) {
            size_t chunk = len - off;
            if (chunk > 1024u) {
                chunk = 1024u;
            }
            long r = __sys5(SYS_FD_OP, FD_OP_CONSOLE_TX,
                            (long)(uintptr_t)(buf + off), (long)chunk, 0, 0);
            if (r <= 0) {
                break;
            }
            off += (size_t)r;
        }
        return;
    }
    for (size_t i = 0; i < len; i++) {
        for (uint32_t spin = 0; spin < 100000u; spin++) {
            if (port_inb((uint16_t)(g_uart_port + 5u)) & 0x20u) {
                break;
            }
        }
        port_outb(g_uart_port, buf[i]);
    }
}

void tty_uart_putc(uint8_t c) {
    tty_uart_write(&c, 1u);
}

int tty_uart_getc(uint8_t *out) {
    if ((port_inb((uint16_t)(g_uart_port + 5u)) & 0x01u) == 0u) {
        return 0;
    }
    *out = port_inb(g_uart_port);
    return 1;
}

int tty_uart_init(uint32_t base, uint32_t irq, int chan) {
    if (base > 0u && base <= 0xFFF8u) {
        g_uart_port = (uint16_t)base;
    }
    if (IOPLSet(3) != 0) {
        printf("[FAIL] ttyserv IOPLSet(3) failed\n");
        return -1;
    }
    if (AttachIntrEvent(irq, chan, PULSE_CODE_IRQ, irq) != 0) {
        printf("[FAIL] ttyserv AttachIntrEvent(%u) failed\n", irq);
        return -1;
    }

    /* 1-byte FIFO trigger, clear FIFOs, enable Received Data Available IRQ. */
    port_outb((uint16_t)(g_uart_port + 2u), 0x07u);
    uint8_t discard = 0;
    while (tty_uart_getc(&discard)) {
    }
    port_outb((uint16_t)(g_uart_port + 1u), 0x01u);

    /* Brief 16550A loopback (MCR bit 4) to verify IOAPIC IRQ -> IPC pulse. */
    port_outb((uint16_t)(g_uart_port + 4u), 0x1Bu);
    port_outb(g_uart_port, 0x5Au);

    int ok = 0;
    for (int tries = 0; tries < 8; tries++) {
        ipc_pulse_t pulse = {0};
        ipc_recv_info_t info = {0};
        long msgid = ReceiveMsg(chan, &pulse, sizeof(pulse), &info);
        if (msgid == 0 && pulse.code == PULSE_CODE_IRQ && pulse.value == irq) {
            uint8_t b = 0;
            while (tty_uart_getc(&b)) {
                if (b == 0x5Au) {
                    ok = 1;
                }
            }
            break;
        }
    }

    /* Restore normal UART operation (DTR | RTS | OUT2, no loopback). */
    port_outb((uint16_t)(g_uart_port + 4u), 0x0Bu);
    (void)IntrUnmask(irq);
    if (!ok) {
        printf("[FAIL] ttyserv UART IRQ %u loopback self-test failed\n", irq);
        return -1;
    }
    printf("[OK] ttyserv handled COM1 IRQ %u pulse via IOAPIC\n", irq);
    printf("[OK] ttyserv configured UART port=0x%x gsi=%u (OPOST/ECHO "
           "active)\n",
           (unsigned)g_uart_port, irq);
    return 0;
}
