/**
 * @file     net_internal.h
 * @brief    Internal interfaces between netserv VirtIO driver and TCP/IP stack
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/net.h>

#define ETH_HDR_LEN 14u
#define ETH_MTU 1500u
#define ETH_FRAME_MAX 1518u
#define ETH_TYPE_IPV4 0x0800u
#define ETH_TYPE_ARP 0x0806u
#define ETH_TYPE_IPV6 0x86DDu

static inline uint16_t net_rd_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t net_rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void net_wr_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static inline void net_wr_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

/* main.c helpers */
uint64_t net_now_mono_ns(void);
void net_tick_ref_inc(void);
void net_tick_ref_dec(void);
void net_reply(uint32_t msgid, const net_rep_t *rep, uint32_t payload_len);

/* virtio_net.c driver API */
int virtio_net_init(int irq_chan, uint8_t mac_out[6]);
int virtio_net_is_ready(void);
void virtio_net_handle_irq(uint32_t irq_val);
void virtio_net_drain_rx(void);
int virtio_net_tx(const uint8_t *frame, uint32_t len);
void virtio_net_get_counters(uint64_t *irqs, uint64_t *rx_pkts,
                             uint64_t *tx_pkts, uint64_t *rx_bytes,
                             uint64_t *tx_bytes);

/* net_proto.c protocol stack API */
void net_proto_init(int nic_ready, const uint8_t mac[6]);
void net_rx_eth(const uint8_t *frame, uint32_t len);
void net_on_tick(uint64_t now_ns);
void net_handle_ipc(uint32_t msgid, const net_req_t *req, uint32_t data_len);
void net_handle_fd_close(uint32_t handle);
