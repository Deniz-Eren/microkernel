/**
 * @file     main.c
 * @brief    Ring 3 MC146818 CMOS RTC driver using IOPLSet, IRQ 8, and MsgV
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/intr.h>
#include <sys/ipc.h>

/* need:: COMP_IPC_0004 */

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71
#define RTC_IRQ 8u
#define RTC_REG_SEC 0x00
#define RTC_REG_MIN 0x02
#define RTC_REG_HOUR 0x04
#define RTC_REG_A 0x0A
#define RTC_REG_B 0x0B
#define RTC_REG_C 0x0C
#define RTC_MAGIC 0x52544331u /* "RTC1" */

typedef struct {
    uint32_t irq_count;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t pad;
} rtc_reply_hdr_t;

static uint8_t cmos_read(uint8_t reg) {
    port_outb(CMOS_ADDR, (uint8_t)(0x80u | reg));
    return port_inb(CMOS_DATA);
}

static void cmos_write(uint8_t reg, uint8_t val) {
    port_outb(CMOS_ADDR, (uint8_t)(0x80u | reg));
    port_outb(CMOS_DATA, val);
}

static int register_rtc_service(int chanid) {
    int nconn = (int)OpenConnection(1, 1);
    if (nconn < 0) {
        return -1;
    }
    name_msg_t nm = {0};
    nm.type = NAME_MSG_ATTACH;
    nm.chanid = (uint32_t)chanid;
    memcpy(nm.path, "/dev/rtc", sizeof("/dev/rtc"));
    name_reply_t nr = {0};
    long s = SendMsg(nconn, &nm, sizeof(nm), &nr, sizeof(nr));
    CloseConnection(nconn);
    return (s < 0 || nr.err != 0) ? -1 : 0;
}

int main(void) {
    if (IOPLSet(3) != 0) {
        printf("[FAIL] rtcdrv IOPLSet(3) failed\n");
        return 1;
    }
    printf("[OK] rtcdrv granted Ring 3 IOPL=3\n");

    int chan = (int)CreateChannel();
    if (chan < 0) {
        printf("[FAIL] rtcdrv CreateChannel failed\n");
        return 1;
    }
    if (AttachIntrEvent(RTC_IRQ, chan, PULSE_CODE_IRQ, RTC_IRQ) != 0) {
        printf("[FAIL] rtcdrv AttachIntrEvent(8) failed\n");
        return 1;
    }

    /* Program MC146818 periodic interrupt at 64 Hz (rate code 0x0A = 15.6 ms)
     * and clear any stale status in Register C. */
    uint8_t reg_a = cmos_read(RTC_REG_A);
    cmos_write(RTC_REG_A, (uint8_t)((reg_a & 0xF0u) | 0x0Au));
    uint8_t reg_b = cmos_read(RTC_REG_B);
    cmos_write(RTC_REG_B, (uint8_t)(reg_b | 0x40u)); /* PIE = 1 */
    (void)cmos_read(RTC_REG_C);

    /* Wait for 3 hardware IRQ 8 pulses delivered via IPC. */
    uint32_t irq_count = 0;
    while (irq_count < 3u) {
        ipc_pulse_t pulse = {0};
        ipc_recv_info_t info = {0};
        long msgid = ReceiveMsg(chan, &pulse, sizeof(pulse), &info);
        if (msgid == 0 && pulse.code == PULSE_CODE_IRQ &&
            pulse.value == RTC_IRQ) {
            (void)cmos_read(RTC_REG_C); /* acknowledge hardware interrupt */
            irq_count++;
            if (irq_count < 3u) {
                if (IntrUnmask(RTC_IRQ) != 0) {
                    printf("[FAIL] rtcdrv IntrUnmask(8) failed\n");
                    return 1;
                }
            }
        }
    }

    /* Stop periodic interrupts once the 3-IRQ verification burst completes. */
    reg_b = cmos_read(RTC_REG_B);
    cmos_write(RTC_REG_B, (uint8_t)(reg_b & ~0x40u));
    (void)cmos_read(RTC_REG_C);
    printf("[OK] rtcdrv handled %u hardware IRQ 8 events via IPC\n", irq_count);

    if (register_rtc_service(chan) != 0) {
        printf("[FAIL] rtcdrv failed to register /dev/rtc\n");
        return 1;
    }
    printf("[OK] rtcdrv registered /dev/rtc (chanid=%d)\n", chan);

    /* Serve client requests using ReadMsgV and WriteMsgV scatter-gather IPC. */
    for (;;) {
        ipc_recv_info_t info = {0};
        long msgid = ReceiveMsg(chan, (void *)0, 0, &info);
        if (msgid <= 0) {
            continue;
        }
        uint32_t magic = 0;
        char tag[16];
        memset(tag, 0, sizeof(tag));
        ipc_iov_t riov[2] = {
            {.base = &magic, .len = sizeof(magic)},
            {.base = tag, .len = sizeof(tag)},
        };
        long rlen = ReadMsgV((int)msgid, 0, riov, 2);
        if (rlen != (long)(sizeof(magic) + sizeof(tag)) || magic != RTC_MAGIC) {
            ReplyMsg((int)msgid, (void *)0, 0);
            continue;
        }
        tag[sizeof(tag) - 1] = '\0';

        rtc_reply_hdr_t hdr = {
            .irq_count = irq_count,
            .hour = cmos_read(RTC_REG_HOUR),
            .min = cmos_read(RTC_REG_MIN),
            .sec = cmos_read(RTC_REG_SEC),
            .pad = 0,
        };
        ipc_iov_t wiov[2] = {
            {.base = &hdr, .len = sizeof(hdr)},
            {.base = tag, .len = sizeof(tag)},
        };
        (void)WriteMsgV((int)msgid, 0, wiov, 2);
        ReplyMsg((int)msgid, (void *)0, 0);
    }
    return 0;
}
