/**
 * @file     main.c
 * @brief    Ring 3 Modern VirtIO 1.0 PCI block device driver (/dev/blk)
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
#include <sys/blk.h>
#include <sys/intr.h>
#include <sys/ipc.h>

/* need:: COMP_IPC_0015 */

#define VIRTIO_PCI_VENDOR 0x1AF4u
#define VIRTIO_PCI_DEV_BLK_LEGACY 0x1001u
#define VIRTIO_PCI_DEV_BLK_MODERN 0x1042u

#define VIRTIO_PCI_CAP_COMMON_CFG 1u
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2u
#define VIRTIO_PCI_CAP_ISR_CFG 3u
#define VIRTIO_PCI_CAP_DEVICE_CFG 4u

#define VIRTIO_STATUS_ACK 1u
#define VIRTIO_STATUS_DRIVER 2u
#define VIRTIO_STATUS_DRIVER_OK 4u
#define VIRTIO_STATUS_FEATURES_OK 8u

#define VRING_DESC_F_NEXT 1u
#define VRING_DESC_F_WRITE 2u

#define VIRTIO_BLK_T_IN 0u
#define VIRTIO_BLK_T_OUT 1u
#define VIRTIO_BLK_S_OK 0u

#define BLK_QSIZE 16u
#define DMA_TOTAL_BYTES 16384u
#define DMA_AVAIL_OFF 0x0100u
#define DMA_USED_OFF 0x0400u
#define DMA_HDR_OFF 0x1000u
#define DMA_STAT_OFF 0x1100u
#define DMA_DATA_OFF 0x2000u

typedef struct __attribute__((packed)) {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint32_t queue_desc_lo;
    uint32_t queue_desc_hi;
    uint32_t queue_driver_lo;
    uint32_t queue_driver_hi;
    uint32_t queue_device_lo;
    uint32_t queue_device_hi;
} virtio_common_cfg_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[BLK_QSIZE];
    uint16_t used_event;
} vring_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[BLK_QSIZE];
    uint16_t avail_event;
} vring_used_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_blk_hdr_t;

typedef struct {
    volatile virtio_common_cfg_t *common;
    volatile uint8_t *notify_base;
    volatile uint8_t *isr_cfg;
    volatile uint32_t *dev_cfg;
    uint32_t notify_mult;
    uint16_t notify_off;
    uint16_t qsize;
    uint16_t last_used;
    uint64_t capacity;
    uint64_t irq_count;
    uint8_t *dma_va;
    uint64_t dma_phys;
    int irq_chan;
    uint32_t irq_line;
    uint32_t irq_gsi;
} virtio_blk_t;

static virtio_blk_t g_blk;

static int map_pci_cap_window(uint32_t bdf, uint8_t bar, uint32_t off,
                              uint32_t len, uintptr_t *va_out) {
    if (bar > 5u) {
        return -1;
    }
    uint32_t bar_lo = 0u;
    if (PCIConfigRead32(bdf, 0x10u + (uint32_t)bar * 4u, &bar_lo) != 0) {
        return -1;
    }
    if ((bar_lo & 1u) != 0u) {
        return -1; /* I/O space BAR not supported for MMIO */
    }
    uint64_t phys = (uint64_t)(bar_lo & ~0xFu);
    if ((bar_lo & 0x6u) == 0x4u && bar < 5u) {
        uint32_t bar_hi = 0u;
        if (PCIConfigRead32(bdf, 0x10u + ((uint32_t)bar + 1u) * 4u, &bar_hi) !=
            0) {
            return -1;
        }
        phys |= ((uint64_t)bar_hi << 32);
    }
    if (phys == 0ULL) {
        return -1;
    }
    uint64_t target = phys + off;
    uint64_t aligned = target & ~0xFFFULL;
    uint64_t page_off = target & 0xFFFULL;
    size_t map_len =
        (size_t)((page_off + (len ? len : 64u) + 0xFFFULL) & ~0xFFFULL);
    long va = VMMapMMIO(aligned, map_len, 0u);
    if (va < 0) {
        return -1;
    }
    *va_out = (uintptr_t)va + (uintptr_t)page_off;
    return 0;
}

static int probe_virtio_blk(void) {
    uint32_t found_bdf = 0xFFFFFFFFu;
    uint8_t found_slot = 0u;

    for (uint8_t slot = 0; slot < 32u; slot++) {
        uint32_t bdf = pci_bdf(0u, slot, 0u);
        uint32_t id = 0u;
        if (PCIConfigRead32(bdf, 0x00u, &id) != 0) {
            continue;
        }
        uint16_t vendor = (uint16_t)(id & 0xFFFFu);
        uint16_t device = (uint16_t)(id >> 16);
        if (vendor == VIRTIO_PCI_VENDOR &&
            (device == VIRTIO_PCI_DEV_BLK_MODERN ||
             device == VIRTIO_PCI_DEV_BLK_LEGACY)) {
            found_bdf = bdf;
            found_slot = slot;
            break;
        }
    }
    if (found_bdf == 0xFFFFFFFFu) {
        return -1;
    }

    /* Enable PCI Memory Space (bit 1) and Bus Master (bit 2); enable INTx. */
    uint32_t cmd_sts = 0u;
    if (PCIConfigRead32(found_bdf, 0x04u, &cmd_sts) != 0) {
        return -1;
    }
    uint32_t new_cmd = (cmd_sts & 0xFFFFu);
    new_cmd |= (1u << 1) | (1u << 2);
    new_cmd &= ~(1u << 10);
    (void)PCIConfigWrite32(found_bdf, 0x04u, new_cmd);

    uint32_t irq_reg = 0u;
    (void)PCIConfigRead32(found_bdf, 0x3Cu, &irq_reg);
    uint8_t irq_line = (uint8_t)(irq_reg & 0xFFu);
    uint8_t irq_pin = (uint8_t)((irq_reg >> 8) & 0xFFu);
    g_blk.irq_line = (irq_line >= 1u && irq_line < 24u) ? irq_line : 0u;
    g_blk.irq_gsi =
        (irq_pin >= 1u && irq_pin <= 4u)
            ? (16u + (((uint32_t)found_slot + (uint32_t)irq_pin - 1u) & 3u))
            : 0u;

    /* Walk PCI capability list to map VirtIO 1.0 MMIO regions. */
    uint32_t cap_reg = 0u;
    if (PCIConfigRead32(found_bdf, 0x34u, &cap_reg) != 0) {
        return -1;
    }
    uint8_t cap_off = (uint8_t)(cap_reg & 0xFCu);
    for (int guard = 0; guard < 32 && cap_off >= 0x40u; guard++) {
        uint32_t d0 = 0u;
        if (PCIConfigRead32(found_bdf, cap_off, &d0) != 0) {
            break;
        }
        uint8_t cap_id = (uint8_t)(d0 & 0xFFu);
        uint8_t next = (uint8_t)((d0 >> 8) & 0xFCu);
        uint8_t cfg_type = (uint8_t)((d0 >> 24) & 0xFFu);
        if (cap_id == 0x09u) {
            uint32_t d1 = 0u;
            uint32_t d2 = 0u;
            uint32_t d3 = 0u;
            (void)PCIConfigRead32(found_bdf, (uint32_t)cap_off + 4u, &d1);
            (void)PCIConfigRead32(found_bdf, (uint32_t)cap_off + 8u, &d2);
            (void)PCIConfigRead32(found_bdf, (uint32_t)cap_off + 12u, &d3);
            uint8_t bar = (uint8_t)(d1 & 0xFFu);
            uintptr_t va = 0u;
            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG && !g_blk.common &&
                map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_blk.common = (volatile virtio_common_cfg_t *)va;
            } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG &&
                       !g_blk.notify_base &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_blk.notify_base = (volatile uint8_t *)va;
                (void)PCIConfigRead32(found_bdf, (uint32_t)cap_off + 16u,
                                      &g_blk.notify_mult);
            } else if (cfg_type == VIRTIO_PCI_CAP_ISR_CFG && !g_blk.isr_cfg &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_blk.isr_cfg = (volatile uint8_t *)va;
            } else if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG &&
                       !g_blk.dev_cfg &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_blk.dev_cfg = (volatile uint32_t *)va;
            }
        }
        cap_off = next;
    }

    if (!g_blk.common || !g_blk.notify_base || !g_blk.isr_cfg ||
        !g_blk.dev_cfg) {
        return -1;
    }

    /* VirtIO 1.0 reset and feature negotiation (VIRTIO_F_VERSION_1 only). */
    g_blk.common->device_status = 0u;
    while (g_blk.common->device_status != 0u) {
        arch_cpu_relax();
    }
    g_blk.common->device_status = VIRTIO_STATUS_ACK;
    g_blk.common->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER;

    g_blk.common->device_feature_select = 1u;
    uint32_t feat_hi = g_blk.common->device_feature;
    if ((feat_hi & 1u) == 0u) {
        return -1; /* VIRTIO_F_VERSION_1 required */
    }
    g_blk.common->driver_feature_select = 0u;
    g_blk.common->driver_feature = 0u;
    g_blk.common->driver_feature_select = 1u;
    g_blk.common->driver_feature = 1u;

    g_blk.common->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    if ((g_blk.common->device_status & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        return -1;
    }

    /* Allocate physically contiguous DMA memory for virtqueue + I/O bounce. */
    uint64_t dma_phys = 0ULL;
    long dma_va = DMAAlloc(DMA_TOTAL_BYTES, &dma_phys);
    if (dma_va < 0 || dma_phys == 0ULL) {
        return -1;
    }
    g_blk.dma_va = (uint8_t *)(uintptr_t)dma_va;
    g_blk.dma_phys = dma_phys;
    memset(g_blk.dma_va, 0, DMA_TOTAL_BYTES);

    /* Configure virtqueue 0. */
    g_blk.common->queue_select = 0u;
    uint16_t max_q = g_blk.common->queue_size;
    if (max_q < 4u) {
        return -1;
    }
    g_blk.qsize = (max_q < BLK_QSIZE) ? max_q : BLK_QSIZE;
    g_blk.common->queue_size = g_blk.qsize;
    g_blk.notify_off = g_blk.common->queue_notify_off;

    uint64_t desc_pa = dma_phys;
    uint64_t avail_pa = dma_phys + DMA_AVAIL_OFF;
    uint64_t used_pa = dma_phys + DMA_USED_OFF;
    g_blk.common->queue_desc_lo = (uint32_t)(desc_pa & 0xFFFFFFFFULL);
    g_blk.common->queue_desc_hi = (uint32_t)(desc_pa >> 32);
    g_blk.common->queue_driver_lo = (uint32_t)(avail_pa & 0xFFFFFFFFULL);
    g_blk.common->queue_driver_hi = (uint32_t)(avail_pa >> 32);
    g_blk.common->queue_device_lo = (uint32_t)(used_pa & 0xFFFFFFFFULL);
    g_blk.common->queue_device_hi = (uint32_t)(used_pa >> 32);
    g_blk.common->queue_enable = 1u;

    /* Create dedicated IRQ pulse channel and attach IOAPIC line(s). */
    g_blk.irq_chan = (int)CreateChannel();
    if (g_blk.irq_chan < 0) {
        return -1;
    }
    int attached = 0;
    if (g_blk.irq_line != 0u &&
        AttachIntrEvent(g_blk.irq_line, g_blk.irq_chan, PULSE_CODE_IRQ,
                        g_blk.irq_line) == 0) {
        attached++;
    }
    if (g_blk.irq_gsi != 0u && g_blk.irq_gsi != g_blk.irq_line &&
        AttachIntrEvent(g_blk.irq_gsi, g_blk.irq_chan, PULSE_CODE_IRQ,
                        g_blk.irq_gsi) == 0) {
        attached++;
    }
    if (attached == 0) {
        return -1;
    }

    /* Set DRIVER_OK and read capacity (sectors). */
    g_blk.common->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                  VIRTIO_STATUS_FEATURES_OK |
                                  VIRTIO_STATUS_DRIVER_OK;

    uint32_t cap_lo = g_blk.dev_cfg[0];
    uint32_t cap_hi = g_blk.dev_cfg[1];
    g_blk.capacity = ((uint64_t)cap_hi << 32) | (uint64_t)cap_lo;

    printf("[OK] blkdrv virtio-blk ready: slot=%u irq=%u gsi=%u sectors=%llu"
           " dma=0x%llx\n",
           (unsigned)found_slot, g_blk.irq_line, g_blk.irq_gsi,
           (unsigned long long)g_blk.capacity,
           (unsigned long long)g_blk.dma_phys);
    return 0;
}

static int blk_io(int is_write, uint64_t lba, uint32_t count, void *buf) {
    if (count == 0u || count > BLK_MAX_SECTORS || lba >= g_blk.capacity ||
        count > g_blk.capacity - lba) {
        return -EINVAL;
    }
    uint32_t nbytes = count * BLK_SECTOR_SIZE;

    vring_desc_t *desc = (vring_desc_t *)g_blk.dma_va;
    vring_avail_t *avail = (vring_avail_t *)(g_blk.dma_va + DMA_AVAIL_OFF);
    volatile vring_used_t *used =
        (volatile vring_used_t *)(g_blk.dma_va + DMA_USED_OFF);
    virtio_blk_hdr_t *hdr = (virtio_blk_hdr_t *)(g_blk.dma_va + DMA_HDR_OFF);
    volatile uint8_t *stat = (volatile uint8_t *)(g_blk.dma_va + DMA_STAT_OFF);
    uint8_t *data = g_blk.dma_va + DMA_DATA_OFF;

    hdr->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    hdr->reserved = 0u;
    hdr->sector = lba;
    *stat = 0xFFu;
    if (is_write) {
        memcpy(data, buf, nbytes);
    }

    desc[0].addr = g_blk.dma_phys + DMA_HDR_OFF;
    desc[0].len = (uint32_t)sizeof(virtio_blk_hdr_t);
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next = 1u;

    desc[1].addr = g_blk.dma_phys + DMA_DATA_OFF;
    desc[1].len = nbytes;
    desc[1].flags =
        (uint16_t)(VRING_DESC_F_NEXT | (is_write ? 0u : VRING_DESC_F_WRITE));
    desc[1].next = 2u;

    desc[2].addr = g_blk.dma_phys + DMA_STAT_OFF;
    desc[2].len = 1u;
    desc[2].flags = VRING_DESC_F_WRITE;
    desc[2].next = 0u;

    uint16_t slot = (uint16_t)(avail->idx % g_blk.qsize);
    avail->ring[slot] = 0u;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    avail->idx = (uint16_t)(avail->idx + 1u);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    /* Ring MMIO doorbell for virtqueue 0. */
    volatile uint16_t *doorbell =
        (volatile uint16_t *)(g_blk.notify_base +
                              (size_t)g_blk.notify_off * g_blk.notify_mult);
    *doorbell = 0u;

    /* Block on dedicated IRQ channel until hardware completion pulse arrives.
     */
    while (used->idx == g_blk.last_used) {
        ipc_pulse_t pulse = {0};
        ipc_recv_info_t rinfo = {0};
        long r = ReceiveMsg(g_blk.irq_chan, &pulse, sizeof(pulse), &rinfo);
        if (r == 0 && pulse.code == PULSE_CODE_IRQ) {
            (void)*g_blk.isr_cfg; /* clear level-triggered PCI INTx */
            g_blk.irq_count++;
            (void)IntrUnmask(pulse.value);
        }
    }
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    g_blk.last_used = used->idx;

    if (*stat != VIRTIO_BLK_S_OK) {
        return -EIO;
    }
    if (!is_write) {
        memcpy(buf, data, nbytes);
    }
    return 0;
}

static int register_name(const char *path, int chanid) {
    int conn = (int)OpenConnection(1u, 1u);
    if (conn < 0) {
        return -1;
    }
    name_msg_t msg = {0};
    name_reply_t rep = {0};
    msg.type = NAME_MSG_ATTACH;
    msg.chanid = (uint32_t)chanid;
    strncpy(msg.path, path, NAME_MAX_PATH - 1u);
    long r = SendMsg(conn, &msg, sizeof(msg), &rep, sizeof(rep));
    (void)CloseConnection(conn);
    return (r < 0 || rep.err != 0) ? -1 : 0;
}

int main(void) {
    if (probe_virtio_blk() != 0) {
        printf("[blkdrv] no virtio-blk PCI device found (idle)\n");
        return 0;
    }

    int svc_chan = (int)CreateChannel();
    if (svc_chan < 0 || register_name("/dev/blk", svc_chan) != 0) {
        printf("[FAIL] blkdrv failed to register /dev/blk\n");
        return 1;
    }
    printf("[OK] blkdrv registered /dev/blk\n");

    static blk_req_t req;
    static blk_rep_t rep;
    ipc_recv_info_t info = {0};
    for (;;) {
        memset(&req, 0, sizeof(req));
        memset(&rep, 0, sizeof(rep));
        long msgid =
            ReceiveMsg(svc_chan, &req, offsetof(blk_req_t, data), &info);
        if (msgid <= 0) {
            continue;
        }
        rep.sector_size = BLK_SECTOR_SIZE;
        rep.num_sectors = g_blk.capacity;
        if (req.op == BLK_OP_INFO) {
            rep.err = 0;
            rep.irq_count = g_blk.irq_count;
            (void)ReplyMsg((int)msgid, &rep, offsetof(blk_rep_t, data));
            continue;
        }
        if (req.op == BLK_OP_READ) {
            int rc = blk_io(0, req.lba, req.count, rep.data);
            rep.err = rc;
            rep.irq_count = g_blk.irq_count;
            rep.bytes = (rc == 0) ? (req.count * BLK_SECTOR_SIZE) : 0u;
            ipc_iov_t iov = {
                .base = &rep,
                .len = offsetof(blk_rep_t, data) + rep.bytes,
            };
            (void)WriteMsgV((int)msgid, 0u, &iov, 1u);
            (void)ReplyMsg((int)msgid, NULL, 0u);
            continue;
        }
        if (req.op == BLK_OP_WRITE) {
            size_t want = (size_t)req.count * BLK_SECTOR_SIZE;
            int rc = -EINVAL;
            if (req.count > 0u && req.count <= BLK_MAX_SECTORS) {
                ipc_iov_t iov = {.base = req.data, .len = want};
                long got =
                    ReadMsgV((int)msgid, offsetof(blk_req_t, data), &iov, 1u);
                if (got == (long)want) {
                    rc = blk_io(1, req.lba, req.count, req.data);
                } else {
                    rc = -EFAULT;
                }
            }
            rep.err = rc;
            rep.irq_count = g_blk.irq_count;
            rep.bytes = (rc == 0) ? (uint32_t)want : 0u;
            (void)ReplyMsg((int)msgid, &rep, offsetof(blk_rep_t, data));
            continue;
        }
        rep.err = -EINVAL;
        (void)ReplyMsg((int)msgid, &rep, offsetof(blk_rep_t, data));
    }
}
