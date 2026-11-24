/**
 * @file     test_libmtest_alt.c
 * @brief    Dynamic linker test library (LD_LIBRARY_PATH variant)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0032 */

int mtest_data = 42;
static int init_count;

__attribute__((constructor)) static void mtest_init(void) {
    init_count++;
}

int mtest_value(void) {
    return 2;
}

int mtest_add(int a, int b) {
    return a + b;
}

int mtest_init_count(void) {
    return init_count;
}
