/**
 * @file     endian.c
 * @brief    POSIX.1-2024 <endian.h> byte-order conversion functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <endian.h>

/* need:: COMP_LIBC_0006 */

#if BYTE_ORDER == LITTLE_ENDIAN

uint16_t htobe16(uint16_t host_16bits) {
    return __builtin_bswap16(host_16bits);
}

uint32_t htobe32(uint32_t host_32bits) {
    return __builtin_bswap32(host_32bits);
}

uint64_t htobe64(uint64_t host_64bits) {
    return __builtin_bswap64(host_64bits);
}

uint16_t be16toh(uint16_t big_endian_16bits) {
    return __builtin_bswap16(big_endian_16bits);
}

uint32_t be32toh(uint32_t big_endian_32bits) {
    return __builtin_bswap32(big_endian_32bits);
}

uint64_t be64toh(uint64_t big_endian_64bits) {
    return __builtin_bswap64(big_endian_64bits);
}

uint16_t htole16(uint16_t host_16bits) {
    return host_16bits;
}

uint32_t htole32(uint32_t host_32bits) {
    return host_32bits;
}

uint64_t htole64(uint64_t host_64bits) {
    return host_64bits;
}

uint16_t le16toh(uint16_t little_endian_16bits) {
    return little_endian_16bits;
}

uint32_t le32toh(uint32_t little_endian_32bits) {
    return little_endian_32bits;
}

uint64_t le64toh(uint64_t little_endian_64bits) {
    return little_endian_64bits;
}

#else

uint16_t htobe16(uint16_t host_16bits) {
    return host_16bits;
}

uint32_t htobe32(uint32_t host_32bits) {
    return host_32bits;
}

uint64_t htobe64(uint64_t host_64bits) {
    return host_64bits;
}

uint16_t be16toh(uint16_t big_endian_16bits) {
    return big_endian_16bits;
}

uint32_t be32toh(uint32_t big_endian_32bits) {
    return big_endian_32bits;
}

uint64_t be64toh(uint64_t big_endian_64bits) {
    return big_endian_64bits;
}

uint16_t htole16(uint16_t host_16bits) {
    return __builtin_bswap16(host_16bits);
}

uint32_t htole32(uint32_t host_32bits) {
    return __builtin_bswap32(host_32bits);
}

uint64_t htole64(uint64_t host_64bits) {
    return __builtin_bswap64(host_64bits);
}

uint16_t le16toh(uint16_t little_endian_16bits) {
    return __builtin_bswap16(little_endian_16bits);
}

uint32_t le32toh(uint32_t little_endian_32bits) {
    return __builtin_bswap32(little_endian_32bits);
}

uint64_t le64toh(uint64_t little_endian_64bits) {
    return __builtin_bswap64(little_endian_64bits);
}

#endif
