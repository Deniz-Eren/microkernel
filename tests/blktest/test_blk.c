/**
 * @file     test_blk.c
 * @brief    Ring 3 CAP_SYS_INTR EPERM and /dev/blk virtio-blk persistence test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_IPC_0008 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/blk.h>
#include <sys/intr.h>
#include <sys/ipc.h>
#include <time.h>
#include <unistd.h>

#define PERSIST_MAGIC "MKBLK_PERSIST_V1"

typedef struct {
    char magic[16];
    uint32_t gen;
    uint32_t checksum;
} persist_hdr_t;

static int resolve_blk(uint32_t *pid_out, uint32_t *chan_out) {
    int conn = (int)OpenConnection(1u, 1u);
    if (conn < 0) {
        return -1;
    }
    name_msg_t q = {0};
    name_reply_t r = {0};
    q.type = NAME_MSG_QUERY;
    strncpy(q.path, "/dev/blk", NAME_MAX_PATH - 1u);
    for (int tries = 0; tries < 80; tries++) {
        memset(&r, 0, sizeof(r));
        if (SendMsg(conn, &q, sizeof(q), &r, sizeof(r)) >= 0 && r.err == 0 &&
            r.pid > 0) {
            *pid_out = (uint32_t)r.pid;
            *chan_out = (uint32_t)r.chanid;
            (void)CloseConnection(conn);
            return 0;
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        (void)nanosleep(&ts, NULL);
    }
    (void)CloseConnection(conn);
    return -1;
}

static int verify_unprivileged_eperm(void) {
    uint64_t dma_pa = 0ULL;
    uint32_t pci_val = 0u;
    if (VMMapMMIO(0xFE000000ULL, 4096u, 0u) != -EPERM ||
        DMAAlloc(4096u, &dma_pa) != -EPERM ||
        PCIConfigRead32(pci_bdf(0u, 0u, 0u), 0x00u, &pci_val) != -EPERM ||
        PCIConfigWrite32(pci_bdf(0u, 0u, 0u), 0x00u, 0u) != -EPERM) {
        printf("[FAIL] blktest: unprivileged MMIO/DMA/PCI call did not return"
               " EPERM\n");
        return -1;
    }
    printf("[OK] unprivileged VMMapMMIO, DMAAlloc, and PCIConfig rejected with"
           " EPERM\n");
    return 0;
}

int main(void) {
    if (verify_unprivileged_eperm() != 0) {
        return 1;
    }

    uint32_t bpid = 0u;
    uint32_t bchan = 0u;
    if (resolve_blk(&bpid, &bchan) != 0) {
        printf("[FAIL] blktest: failed to resolve /dev/blk\n");
        return 1;
    }
    int conn = (int)OpenConnection(bpid, bchan);
    if (conn < 0) {
        printf("[FAIL] blktest: OpenConnection(/dev/blk) failed\n");
        return 1;
    }

    static blk_req_t req;
    static blk_rep_t rep;

    /* 1. Query block device geometry. */
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_INFO;
    if (SendMsg(conn, &req, offsetof(blk_req_t, data), &rep, sizeof(rep)) < 0 ||
        rep.err != 0 || rep.sector_size != 512u || rep.num_sectors < 64u) {
        printf("[FAIL] blktest: BLK_OP_INFO failed (err=%d sec=%llu)\n",
               rep.err, (unsigned long long)rep.num_sectors);
        (void)CloseConnection(conn);
        return 1;
    }
    uint64_t irq_before = rep.irq_count;

    /* 2. Read LBA 0 to check cross-reboot persistence sentinel. */
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_READ;
    req.lba = 0u;
    req.count = 1u;
    if (SendMsg(conn, &req, offsetof(blk_req_t, data), &rep, sizeof(rep)) < 0 ||
        rep.err != 0 || rep.bytes != 512u) {
        printf("[FAIL] blktest: read LBA 0 failed\n");
        (void)CloseConnection(conn);
        return 1;
    }

    persist_hdr_t hdr = {0};
    memcpy(&hdr, rep.data, sizeof(hdr));
    uint32_t prev_gen = 0u;
    if (memcmp(hdr.magic, PERSIST_MAGIC, 16u) == 0 &&
        hdr.checksum == (hdr.gen ^ 0xA55A1234u)) {
        prev_gen = hdr.gen;
        printf("[OK] blktest verified cross-reboot sector persistence"
               " (gen=%u)\n",
               prev_gen);
    }

    /* 3. Write incremented generation to LBA 0 and read back. */
    uint32_t next_gen = prev_gen + 1u;
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_WRITE;
    req.lba = 0u;
    req.count = 1u;
    memcpy(hdr.magic, PERSIST_MAGIC, 16u);
    hdr.gen = next_gen;
    hdr.checksum = next_gen ^ 0xA55A1234u;
    memcpy(req.data, &hdr, sizeof(hdr));
    if (SendMsg(conn, &req, offsetof(blk_req_t, data) + 512u, &rep,
                sizeof(rep)) < 0 ||
        rep.err != 0 || rep.bytes != 512u) {
        printf("[FAIL] blktest: write LBA 0 failed\n");
        (void)CloseConnection(conn);
        return 1;
    }

    /* 4. Multi-sector (4 sectors = 2048 bytes) write/readback at LBA 16..19. */
    static uint8_t pattern[BLK_MAX_BYTES];
    for (size_t i = 0; i < sizeof(pattern); i++) {
        pattern[i] = (uint8_t)((i * 73u + next_gen * 19u + 7u) & 0xFFu);
    }
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_WRITE;
    req.lba = 16u;
    req.count = BLK_MAX_SECTORS;
    memcpy(req.data, pattern, sizeof(pattern));
    if (SendMsg(conn, &req, offsetof(blk_req_t, data) + sizeof(pattern), &rep,
                sizeof(rep)) < 0 ||
        rep.err != 0 || rep.bytes != sizeof(pattern)) {
        printf("[FAIL] blktest: multi-sector write failed\n");
        (void)CloseConnection(conn);
        return 1;
    }

    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_READ;
    req.lba = 16u;
    req.count = BLK_MAX_SECTORS;
    if (SendMsg(conn, &req, offsetof(blk_req_t, data), &rep, sizeof(rep)) < 0 ||
        rep.err != 0 || rep.bytes != sizeof(pattern) ||
        memcmp(rep.data, pattern, sizeof(pattern)) != 0) {
        printf("[FAIL] blktest: multi-sector readback mismatch\n");
        (void)CloseConnection(conn);
        return 1;
    }

    if (rep.irq_count <= irq_before) {
        printf("[FAIL] blktest: expected hardware IRQ count increase\n");
        (void)CloseConnection(conn);
        return 1;
    }

    (void)CloseConnection(conn);
    printf("[OK] blktest /dev/blk sector read/write and IOAPIC IRQ pulses"
           " verified (gen=%u irqs=%llu)\n",
           next_gen, (unsigned long long)rep.irq_count);
    return 0;
}
