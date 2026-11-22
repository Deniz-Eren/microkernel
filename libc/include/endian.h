/**
 * @file     endian.h
 * @brief    POSIX.1-2024 byte-order macros and integer conversion functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ENDIAN_H
#define _ENDIAN_H

/* need:: ICD_LIBC_0006 */

#include <stdint.h>

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) &&                \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

uint16_t be16toh(uint16_t big_endian_16bits);
uint32_t be32toh(uint32_t big_endian_32bits);
uint64_t be64toh(uint64_t big_endian_64bits);
uint16_t htobe16(uint16_t host_16bits);
uint32_t htobe32(uint32_t host_32bits);
uint64_t htobe64(uint64_t host_64bits);
uint16_t htole16(uint16_t host_16bits);
uint32_t htole32(uint32_t host_32bits);
uint64_t htole64(uint64_t host_64bits);
uint16_t le16toh(uint16_t little_endian_16bits);
uint32_t le32toh(uint32_t little_endian_32bits);
uint64_t le64toh(uint64_t little_endian_64bits);

#endif /* _ENDIAN_H */
