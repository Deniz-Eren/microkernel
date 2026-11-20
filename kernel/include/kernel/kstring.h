/**
 * @file     kstring.h
 * @brief    Kernel string search helper declarations
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stddef.h>

int kstrcmp(const char *a, const char *b);
char *kstrstr(const char *haystack, const char *needle);
