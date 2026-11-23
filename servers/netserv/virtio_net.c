/**
 * @file     virtio_net.c
 * @brief    Ring 3 Modern VirtIO 1.0 PCI network driver (MMIO, DMA, IOAPIC IRQ)
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
#include "net_internal.h"

/* need:: COMP_IPC_0018 */

#define VIRTIO_PCI_VENDOR 0x1AF4u
#define VIRTIO_PCI_DEV_NET_LEGACY 0x1000u
#define VIRTIO_PCI_DEV_NET_MODERN 0x1041u

#define VIRTIO_PCI_CAP_COMMON_CFG 1u
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2u
#define VIRTIO_PCI_CAP_ISR_CFG 3u
#define VIRTIO_PCI_CAP_DEVICE_CFG 4u

#define VIRTIO_STATUS_ACK 1u
#define VIRTIO_STATUS_DRIVER 2u
#define VIRTIO_STATUS_DRIVER_OK 4u
#define VIRTIO_STATUS_FEATURES_OK 8u

#define VIRTIO_NET_F_MAC (1u << 5)

#define VRING_DESC_F_WRITE 2u

#define NET_QSIZE 16u
#define NET_BUF_STRIDE 2048u
#define DMA_TOTAL_BYTES 0x11000u /* 17 pages (68 KiB) */
#define DMA_RX_DESC_OFF 0x0000u
#define DMA_RX_AVAIL_OFF 0x0200u
#define DMA_RX_USED_OFF 0x0400u
#define DMA_TX_DESC_OFF 0x0800u
#define DMA_TX_AVAIL_OFF 0x0A00u
#define DMA_TX_USED_OFF 0x0C00u
#define DMA_RX_BUF_BASE 0x1000u
#define DMA_TX_BUF_BASE 0x9000u

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
    uint16_t ring[NET_QSIZE];
    uint16_t used_event;
} vring_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[NET_QSIZE];
    uint16_t avail_event;
} vring_used_t;

/* Modern VirtIO 1.0 (VIRTIO_F_VERSION_1) network header is 12 bytes. */
typedef struct __attribute__((packed)) {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} virtio_net_hdr_t;

_Static_assert(sizeof(virtio_net_hdr_t) == 12u,
               "virtio-net 1.0 header must be 12 bytes");

typedef struct {
    int ready;
    volatile virtio_common_cfg_t *common;
    volatile uint8_t *notify_base;
    volatile uint8_t *isr_cfg;
    volatile uint8_t *dev_cfg;
    uint32_t notify_mult;
    uint16_t rx_qsize;
    uint16_t tx_qsize;
    uint16_t rx_notify_off;
    uint16_t tx_notify_off;
    uint16_t rx_last_used;
    uint16_t tx_last_used;
    uint16_t tx_next_slot;
    uint8_t *dma_va;
    uint64_t dma_phys;
    uint8_t mac[6];
    uint32_t irq_line;
    uint32_t irq_gsi;
    uint64_t irq_count;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} virtio_net_t;

static virtio_net_t g_nic;

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
        return -1;
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

static void ring_doorbell(uint16_t queue_idx, uint16_t notify_off) {
    volatile uint16_t *db =
        (volatile uint16_t *)(g_nic.notify_base +
                              (size_t)notify_off * g_nic.notify_mult);
    *db = queue_idx;
}

static int setup_queue(uint16_t qidx, uint32_t desc_off, uint32_t avail_off,
                       uint32_t used_off, uint16_t *qsize_out,
                       uint16_t *notify_off_out) {
    g_nic.common->queue_select = qidx;
    uint16_t max_q = g_nic.common->queue_size;
    if (max_q < 4u) {
        return -1;
    }
    uint16_t qsize = (max_q < NET_QSIZE) ? max_q : NET_QSIZE;
    g_nic.common->queue_size = qsize;
    *qsize_out = qsize;
    *notify_off_out = g_nic.common->queue_notify_off;

    uint64_t desc_pa = g_nic.dma_phys + desc_off;
    uint64_t avail_pa = g_nic.dma_phys + avail_off;
    uint64_t used_pa = g_nic.dma_phys + used_off;
    g_nic.common->queue_desc_lo = (uint32_t)(desc_pa & 0xFFFFFFFFULL);
    g_nic.common->queue_desc_hi = (uint32_t)(desc_pa >> 32);
    g_nic.common->queue_driver_lo = (uint32_t)(avail_pa & 0xFFFFFFFFULL);
    g_nic.common->queue_driver_hi = (uint32_t)(avail_pa >> 32);
    g_nic.common->queue_device_lo = (uint32_t)(used_pa & 0xFFFFFFFFULL);
    g_nic.common->queue_device_hi = (uint32_t)(used_pa >> 32);
    g_nic.common->queue_enable = 1u;
    return 0;
}

static void post_rx_slot(uint16_t slot) {
    vring_desc_t *desc = (vring_desc_t *)(g_nic.dma_va + DMA_RX_DESC_OFF);
    vring_avail_t *avail = (vring_avail_t *)(g_nic.dma_va + DMA_RX_AVAIL_OFF);
    desc[slot].addr =
        g_nic.dma_phys + DMA_RX_BUF_BASE + (uint64_t)slot * NET_BUF_STRIDE;
    desc[slot].len = NET_BUF_STRIDE;
    desc[slot].flags = VRING_DESC_F_WRITE;
    desc[slot].next = 0u;

    uint16_t aidx = (uint16_t)(avail->idx % g_nic.rx_qsize);
    avail->ring[aidx] = slot;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    avail->idx = (uint16_t)(avail->idx + 1u);
}

int virtio_net_init(int irq_chan, uint8_t mac_out[6]) {
    memset(&g_nic, 0, sizeof(g_nic));
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
            (device == VIRTIO_PCI_DEV_NET_MODERN ||
             device == VIRTIO_PCI_DEV_NET_LEGACY)) {
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

    uint32_t host_id = 0u;
    (void)PCIConfigRead32(pci_bdf(0u, 0u, 0u), 0x00u, &host_id);
    uint32_t gsi_base = (host_id == 0x29C08086u) ? 20u : 16u;

    uint32_t irq_reg = 0u;
    (void)PCIConfigRead32(found_bdf, 0x3Cu, &irq_reg);
    uint8_t irq_line = (uint8_t)(irq_reg & 0xFFu);
    uint8_t irq_pin = (uint8_t)((irq_reg >> 8) & 0xFFu);
    g_nic.irq_line = (irq_line >= 1u && irq_line < 24u) ? irq_line : 0u;
    g_nic.irq_gsi =
        (irq_pin >= 1u && irq_pin <= 4u)
            ? (gsi_base +
               (((uint32_t)found_slot + (uint32_t)irq_pin - 1u) & 3u))
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
            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG && !g_nic.common &&
                map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_nic.common = (volatile virtio_common_cfg_t *)va;
            } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG &&
                       !g_nic.notify_base &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_nic.notify_base = (volatile uint8_t *)va;
                (void)PCIConfigRead32(found_bdf, (uint32_t)cap_off + 16u,
                                      &g_nic.notify_mult);
            } else if (cfg_type == VIRTIO_PCI_CAP_ISR_CFG && !g_nic.isr_cfg &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_nic.isr_cfg = (volatile uint8_t *)va;
            } else if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG &&
                       !g_nic.dev_cfg &&
                       map_pci_cap_window(found_bdf, bar, d2, d3, &va) == 0) {
                g_nic.dev_cfg = (volatile uint8_t *)va;
            }
        }
        cap_off = next;
    }

    if (!g_nic.common || !g_nic.notify_base || !g_nic.isr_cfg ||
        !g_nic.dev_cfg) {
        return -1;
    }

    /* Reset and negotiate VIRTIO_F_VERSION_1 (+ VIRTIO_NET_F_MAC if offered).
     */
    g_nic.common->device_status = 0u;
    while (g_nic.common->device_status != 0u) {
        arch_cpu_relax();
    }
    g_nic.common->device_status = VIRTIO_STATUS_ACK;
    g_nic.common->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER;

    g_nic.common->device_feature_select = 0u;
    uint32_t feat_lo = g_nic.common->device_feature;
    g_nic.common->device_feature_select = 1u;
    uint32_t feat_hi = g_nic.common->device_feature;
    if ((feat_hi & 1u) == 0u) {
        return -1;
    }

    uint32_t drv_lo = feat_lo & VIRTIO_NET_F_MAC;
    g_nic.common->driver_feature_select = 0u;
    g_nic.common->driver_feature = drv_lo;
    g_nic.common->driver_feature_select = 1u;
    g_nic.common->driver_feature = 1u;

    g_nic.common->device_status =
        VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    if ((g_nic.common->device_status & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        return -1;
    }

    uint64_t dma_phys = 0ULL;
    long dma_va = DMAAlloc(DMA_TOTAL_BYTES, &dma_phys);
    if (dma_va < 0 || dma_phys == 0ULL) {
        return -1;
    }
    g_nic.dma_va = (uint8_t *)(uintptr_t)dma_va;
    g_nic.dma_phys = dma_phys;
    memset(g_nic.dma_va, 0, DMA_TOTAL_BYTES);

    /* Configure RX queue 0 and TX queue 1. */
    if (setup_queue(0u, DMA_RX_DESC_OFF, DMA_RX_AVAIL_OFF, DMA_RX_USED_OFF,
                    &g_nic.rx_qsize, &g_nic.rx_notify_off) != 0 ||
        setup_queue(1u, DMA_TX_DESC_OFF, DMA_TX_AVAIL_OFF, DMA_TX_USED_OFF,
                    &g_nic.tx_qsize, &g_nic.tx_notify_off) != 0) {
        return -1;
    }

    /* Attach dedicated PCI IOAPIC GSI; fall back to legacy IRQ line only if no
     * GSI is available (avoids sharing legacy PIC line 11 across PCI slots). */
    int attached = 0;
    if (g_nic.irq_gsi != 0u &&
        AttachIntrEvent(g_nic.irq_gsi, irq_chan, PULSE_CODE_IRQ,
                        g_nic.irq_gsi) == 0) {
        attached++;
    } else if (g_nic.irq_line != 0u &&
               AttachIntrEvent(g_nic.irq_line, irq_chan, PULSE_CODE_IRQ,
                               g_nic.irq_line) == 0) {
        attached++;
    }
    if (attached == 0) {
        return -1;
    }

    if ((drv_lo & VIRTIO_NET_F_MAC) != 0u) {
        for (int i = 0; i < 6; i++) {
            g_nic.mac[i] = g_nic.dev_cfg[i];
        }
    } else {
        g_nic.mac[0] = 0x52u;
        g_nic.mac[1] = 0x54u;
        g_nic.mac[2] = 0x00u;
        g_nic.mac[3] = 0x12u;
        g_nic.mac[4] = 0x34u;
        g_nic.mac[5] = 0x56u;
    }
    memcpy(mac_out, g_nic.mac, 6u);

    /* Pre-populate all RX descriptors before marking DRIVER_OK. */
    for (uint16_t i = 0; i < g_nic.rx_qsize; i++) {
        post_rx_slot(i);
    }

    g_nic.common->device_status = VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER |
                                  VIRTIO_STATUS_FEATURES_OK |
                                  VIRTIO_STATUS_DRIVER_OK;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    ring_doorbell(0u, g_nic.rx_notify_off);
    g_nic.ready = 1;

    printf("[OK] netserv virtio-net ready: slot=%u irq=%u gsi=%u"
           " mac=%02x:%02x:%02x:%02x:%02x:%02x dma=0x%llx\n",
           (unsigned)found_slot, g_nic.irq_line, g_nic.irq_gsi, g_nic.mac[0],
           g_nic.mac[1], g_nic.mac[2], g_nic.mac[3], g_nic.mac[4], g_nic.mac[5],
           (unsigned long long)g_nic.dma_phys);
    return 0;
}

int virtio_net_is_ready(void) {
    return g_nic.ready;
}

static void drain_tx_used(void) {
    if (!g_nic.ready) {
        return;
    }
    volatile vring_used_t *used =
        (volatile vring_used_t *)(g_nic.dma_va + DMA_TX_USED_OFF);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    g_nic.tx_last_used = used->idx;
}

void virtio_net_drain_rx(void) {
    if (!g_nic.ready) {
        return;
    }
    drain_tx_used();

    volatile vring_used_t *used =
        (volatile vring_used_t *)(g_nic.dma_va + DMA_RX_USED_OFF);
    int reposted = 0;
    for (;;) {
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (used->idx == g_nic.rx_last_used) {
            break;
        }
        uint16_t ring_i = (uint16_t)(g_nic.rx_last_used % g_nic.rx_qsize);
        uint32_t desc_id = used->ring[ring_i].id;
        uint32_t total_len = used->ring[ring_i].len;
        g_nic.rx_last_used = (uint16_t)(g_nic.rx_last_used + 1u);

        if (desc_id < g_nic.rx_qsize) {
            uint8_t *buf = g_nic.dma_va + DMA_RX_BUF_BASE +
                           (size_t)desc_id * NET_BUF_STRIDE;
            if (total_len > sizeof(virtio_net_hdr_t)) {
                uint32_t flen = total_len - (uint32_t)sizeof(virtio_net_hdr_t);
                if (flen > ETH_FRAME_MAX) {
                    flen = ETH_FRAME_MAX;
                }
                uint8_t frame[ETH_FRAME_MAX];
                memcpy(frame, buf + sizeof(virtio_net_hdr_t), flen);
                g_nic.rx_packets++;
                g_nic.rx_bytes += flen;
                post_rx_slot((uint16_t)desc_id);
                reposted = 1;
                net_rx_eth(frame, flen);
            } else {
                post_rx_slot((uint16_t)desc_id);
                reposted = 1;
            }
        }
    }
    if (reposted) {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        ring_doorbell(0u, g_nic.rx_notify_off);
    }
}

void virtio_net_handle_irq(uint32_t irq_val) {
    if (!g_nic.ready) {
        return;
    }
    (void)*g_nic.isr_cfg; /* clear level-triggered PCI INTx */
    g_nic.irq_count++;
    (void)IntrUnmask(irq_val);
    virtio_net_drain_rx();
}

int virtio_net_tx(const uint8_t *frame, uint32_t len) {
    if (!g_nic.ready) {
        return -ENETDOWN;
    }
    if (!frame || len < ETH_HDR_LEN || len > ETH_FRAME_MAX) {
        return -EINVAL;
    }
    drain_tx_used();

    uint16_t slot = g_nic.tx_next_slot;
    g_nic.tx_next_slot = (uint16_t)((slot + 1u) % g_nic.tx_qsize);

    uint8_t *buf =
        g_nic.dma_va + DMA_TX_BUF_BASE + (size_t)slot * NET_BUF_STRIDE;
    virtio_net_hdr_t *vh = (virtio_net_hdr_t *)buf;
    memset(vh, 0, sizeof(*vh));
    memcpy(buf + sizeof(*vh), frame, len);

    vring_desc_t *desc = (vring_desc_t *)(g_nic.dma_va + DMA_TX_DESC_OFF);
    vring_avail_t *avail = (vring_avail_t *)(g_nic.dma_va + DMA_TX_AVAIL_OFF);
    desc[slot].addr =
        g_nic.dma_phys + DMA_TX_BUF_BASE + (uint64_t)slot * NET_BUF_STRIDE;
    desc[slot].len = (uint32_t)sizeof(*vh) + len;
    desc[slot].flags = 0u;
    desc[slot].next = 0u;

    uint16_t aidx = (uint16_t)(avail->idx % g_nic.tx_qsize);
    avail->ring[aidx] = slot;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    avail->idx = (uint16_t)(avail->idx + 1u);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    ring_doorbell(1u, g_nic.tx_notify_off);
    g_nic.tx_packets++;
    g_nic.tx_bytes += len;
    return 0;
}

void virtio_net_get_counters(uint64_t *irqs, uint64_t *rx_pkts,
                             uint64_t *tx_pkts, uint64_t *rx_bytes,
                             uint64_t *tx_bytes) {
    if (irqs) {
        *irqs = g_nic.irq_count;
    }
    if (rx_pkts) {
        *rx_pkts = g_nic.rx_packets;
    }
    if (tx_pkts) {
        *tx_pkts = g_nic.tx_packets;
    }
    if (rx_bytes) {
        *rx_bytes = g_nic.rx_bytes;
    }
    if (tx_bytes) {
        *tx_bytes = g_nic.tx_bytes;
    }
}
