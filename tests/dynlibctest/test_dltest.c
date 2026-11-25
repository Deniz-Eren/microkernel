/**
 * @file     test_dltest.c
 * @brief    Shared libc.so test library loaded with runtime dlopen
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0033 */

int dltest_data = 41;
static int init_count;

__attribute__((constructor)) static void dltest_init(void) {
    init_count++;
}

int dltest_value(void) {
    return 42;
}

int dltest_add(int a, int b) {
    return a + b;
}

int dltest_init_count(void) {
    return init_count;
}
