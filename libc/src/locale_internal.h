/**
 * @file     locale_internal.h
 * @brief    Internal locale state and UTF-8 helper prototypes for libc
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <locale.h>
#include <stddef.h>
#include <stdint.h>

#define LOC_ID_C 0u
#define LOC_ID_UTF8 1u

#define LOC_CAT_COUNT 6u
#define LOC_NAME_MAX 32u

struct __microkernel_locale {
    uint8_t cat[LOC_CAT_COUNT];
    char name[LOC_CAT_COUNT][LOC_NAME_MAX];
};

int __libc_locale_is_utf8(locale_t loc);
