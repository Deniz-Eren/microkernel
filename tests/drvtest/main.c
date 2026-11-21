/**
 * @file     main.c
 * @brief    Ring 3 Milestone 6 test for IOPL -EPERM, /dev/rtc, and Read/WriteV
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/intr.h>
#include <sys/ipc.h>

#define RTC_MAGIC 0x52544331u /* "RTC1" */

typedef struct {
    uint32_t magic;
    char tag[16];
} rtc_req_t;

typedef struct {
    uint32_t irq_count;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t pad;
    char tag[16];
} rtc_rep_t;

int main(void) {
    /* 1. Verify unprivileged process cannot gain IOPL or attach hardware IRQ.
     */
    if (IOPLSet(3) != -EPERM) {
        printf("[FAIL] drvtest expected IOPLSet(3) -> -EPERM\n");
        return 1;
    }
    if (AttachIntrEvent(8u, 0, PULSE_CODE_IRQ, 8u) != -EPERM) {
        printf("[FAIL] drvtest expected AttachIntrEvent(8) -> -EPERM\n");
        return 1;
    }
    printf("[OK] unprivileged IOPLSet and AttachIntrEvent rejected with "
           "EPERM\n");

    /* 2. Resolve /dev/rtc once rtcdrv finishes its 3-IRQ burst and registers.
     */
    int nconn = (int)OpenConnection(1, 1);
    if (nconn < 0) {
        printf("[FAIL] drvtest OpenConnection(sysproc) -> %d\n", nconn);
        return 1;
    }

    name_msg_t nm = {0};
    nm.type = NAME_MSG_QUERY;
    memcpy(nm.path, "/dev/rtc", sizeof("/dev/rtc"));
    name_reply_t nr = {0};

    int resolved = 0;
    for (int tries = 0; tries < 2000; tries++) {
        long s = SendMsg(nconn, &nm, sizeof(nm), &nr, sizeof(nr));
        if (s >= 0 && nr.err == 0 && nr.pid > 0) {
            resolved = 1;
            break;
        }
        for (volatile int spin = 0; spin < 50000; spin++) {
        }
    }
    CloseConnection(nconn);
    if (!resolved) {
        printf("[FAIL] drvtest could not resolve /dev/rtc\n");
        return 1;
    }

    /* 3. Exercise ReadMsgV / WriteMsgV round trip with rtcdrv. */
    int rconn = (int)OpenConnection((uint32_t)nr.pid, (uint32_t)nr.chanid);
    if (rconn < 0) {
        printf("[FAIL] drvtest OpenConnection(/dev/rtc) -> %d\n", rconn);
        return 1;
    }

    rtc_req_t req = {0};
    req.magic = RTC_MAGIC;
    memcpy(req.tag, "M6_SCATTER_OK", sizeof("M6_SCATTER_OK"));
    rtc_rep_t rep = {0};

    long rlen = SendMsg(rconn, &req, sizeof(req), &rep, sizeof(rep));
    CloseConnection(rconn);
    if (rlen != (long)sizeof(rep)) {
        printf("[FAIL] drvtest SendMsg len=%ld expected=%zu\n", rlen,
               sizeof(rep));
        return 1;
    }
    rep.tag[sizeof(rep.tag) - 1] = '\0';
    if (rep.irq_count < 3u || strcmp(rep.tag, "M6_SCATTER_OK") != 0) {
        printf("[FAIL] drvtest bad reply irqs=%u tag='%s'\n", rep.irq_count,
               rep.tag);
        return 1;
    }
    printf("[OK] drvtest ReadMsgV/WriteMsgV /dev/rtc round trip succeeded "
           "(irqs=%u)\n",
           rep.irq_count);
    return 0;
}
