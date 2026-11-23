/**
 * @file     dlplugin.c
 * @brief    Position-independent ET_DYN test shared library for <dlfcn.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stddef.h>
#include <string.h>

int dl_ctor_ran = 0;
static int *g_dtor_flag = NULL;

static int helper_square(int x) {
    return x * x;
}

typedef int (*op_fn_t)(int);
static op_fn_t g_ops[1] = {helper_square};

__attribute__((constructor)) static void dlplugin_init(void) {
    dl_ctor_ran = 42;
}

__attribute__((destructor)) static void dlplugin_fini(void) {
    if (g_dtor_flag) {
        *g_dtor_flag = 99;
    }
}

int dl_add(int a, int b) {
    return a + b;
}

int dl_call_reloc(int x) {
    return g_ops[0](x);
}

int dl_use_host_strlen(const char *s) {
    return (int)strlen(s);
}

const char *dl_greeting(void) {
    return "hello-from-et-dyn";
}

void dl_set_dtor_flag(int *flag_ptr) {
    g_dtor_flag = flag_ptr;
}
