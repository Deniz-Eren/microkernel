// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include "apic.h"
#include "io.h"
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/vmm.h>

/* Kernel MMIO window (uncached). */
#define MMIO_WINDOW_VA 0xFFFFFFFF90000000ULL
#define LAPIC_VA (MMIO_WINDOW_VA + 0x0000)

/* Local APIC MMIO register offsets. */
#define LAPIC_ID      0x020
#define LAPIC_EOI     0x0B0
#define LAPIC_SPUR    0x0F0
#define LAPIC_ESR     0x280
#define LAPIC_ICR_LO  0x300
#define LAPIC_LVT_TMR 0x320
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERR 0x370
#define LAPIC_TMR_ICR 0x380
#define LAPIC_TMR_CCR 0x390
#define LAPIC_TMR_DIV 0x3E0

#define TIMER_VECTOR 0x20

static volatile uint32_t *g_lapic;
static volatile uint64_t g_timer_ticks;
static uint32_t g_timer_period;

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

uint32_t lapic_read(uint32_t reg) {
    return g_lapic[reg / 4];
}

void lapic_write(uint32_t reg, uint32_t value) {
    g_lapic[reg / 4] = value;
}

void apic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

uint64_t timer_ticks(void) {
    return g_timer_ticks;
}

static void pic_mask_all(void) {
    outb(0x21, 0xFF); /* mask all lines on master 8259 */
    outb(0xA1, 0xFF); /* mask all lines on slave 8259 */
}

/* One-shot LAPIC timer calibration against the PIT (mode 0, 10 ms). */
static uint32_t calibrate_lapic_timer(void) {
    /* Program PIT channel 0: mode 0 (terminal count), lo/hi access. */
    const uint16_t pit_10ms = 11932; /* 1193182 Hz / 100 */
    outb(0x43, 0x30);
    outb(0x40, pit_10ms & 0xFF);
    outb(0x40, (pit_10ms >> 8) & 0xFF);

    /* LAPIC timer: one-shot, masked, max count. */
    lapic_write(LAPIC_LVT_TMR, TIMER_VECTOR | 0x10000 /* mask */);
    lapic_write(LAPIC_TMR_DIV, 0x3 /* divide by 16 */);
    lapic_write(LAPIC_TMR_ICR, 0xFFFFFFFF);

    /* Poll the PIT countdown until terminal count. */
    for (uint32_t spins = 0; spins < 100000000U; spins++) {
        outb(0x43, 0x00); /* latch channel 0 count */
        uint16_t c = (uint16_t)(inb(0x40) | (inb(0x40) << 8));
        if (c == 0) {
            break;
        }
    }

    uint32_t elapsed = 0xFFFFFFFFU - lapic_read(LAPIC_TMR_CCR);
    lapic_write(LAPIC_TMR_ICR, 0); /* stop */
    return elapsed / 10;           /* ticks per 1 ms */
}

void apic_init(void) {
    pic_mask_all();

    uint64_t base = rdmsr(0x1B) & 0xFFFFF000ULL;
    vmm_map_page(LAPIC_VA, base, VMM_PRESENT | VMM_WRITE | VMM_PCD | VMM_NX);
    g_lapic = (volatile uint32_t *)LAPIC_VA;

    /* Spurious interrupt vector 0xFF, APIC software-enabled. */
    lapic_write(LAPIC_SPUR, 0xFF | 0x100);
    /* Mask LINT0/LINT1/error LVTs. */
    lapic_write(LAPIC_LVT_LINT0, 0x10000);
    lapic_write(LAPIC_LVT_LINT1, 0x10000);
    lapic_write(LAPIC_LVT_ERR, (0x3F & 0xFF) | 0x10000);

    g_timer_period = calibrate_lapic_timer();

    /* Periodic mode, unmasked, 1 ms tick. */
    lapic_write(LAPIC_LVT_TMR, TIMER_VECTOR | 0x20000 /* periodic */);
    lapic_write(LAPIC_TMR_DIV, 0x3);
    lapic_write(LAPIC_TMR_ICR, g_timer_period);

    __asm__ volatile("sti");
    printk("[apic] LAPIC @ %llx, timer period %u ticks/ms\n",
           (unsigned long long)base, g_timer_period);
}

void apic_irq_dispatch(uint64_t vec) {
    if (vec == TIMER_VECTOR) {
        g_timer_ticks++;
        apic_eoi();
        extern void sched_tick(void);
        sched_tick();
        return;
    }
    if (vec == 0xFF) {
        /* Spurious: only EOI if the spurious bit is not set. */
        if (!(lapic_read(LAPIC_SPUR) & (1 << 12))) {
            apic_eoi();
        }
        return;
    }
    if (vec >= 32) {
        printk("[apic] unexpected IRQ %llu\n", (unsigned long long)vec);
        apic_eoi();
    }
}

/* Best-effort ACPI S5 shutdown for QEMU (q35 ICH9: PM1a_CNT at port 0x604,
 * legacy PIIX4 at 0x604/0xB004). QEMU exits when the guest requests S5. */
int acpi_poweroff(void) {
    static const struct {
        uint16_t port;
        uint16_t val;
    } attempts[] = {
        {0x604, 0x2000},  /* SLP_EN | SLP_TYP=0 (QEMU q35 and piix4) */
        {0x604, 0x3400},  /* SLP_EN | SLP_TYP=5 */
        {0x604, 0x3C00},  /* SLP_EN | SLP_TYP=7 */
        {0xB004, 0x2000}, /* older QEMU/Bochs */
        {0x6004, 0x2000}, /* VirtualBox */
    };
    for (unsigned i = 0; i < sizeof(attempts) / sizeof(attempts[0]); i++) {
        outw(attempts[i].port, attempts[i].val);
        for (volatile int d = 0; d < 100000; d++) {
        }
    }
    return -1;
}
