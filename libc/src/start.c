// Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char **environ;
void *__auxv;

extern int main(int argc, char **argv, char **envp);

/* C++ forward-compatibility hooks: iterate global constructors before main
 * and destructors at exit. */
typedef void (*init_fn_t)(void);
extern init_fn_t __init_array_start[];
extern init_fn_t __init_array_end[];
extern init_fn_t __fini_array_start[];
extern init_fn_t __fini_array_end[];

void __libc_init_array(void) {
    for (init_fn_t *p = __init_array_start; p < __init_array_end; p++) {
        (*p)();
    }
}

void __libc_fini_array(void) {
    for (init_fn_t *p = __fini_array_end; p > __fini_array_start;) {
        p--;
        (*p)();
    }
}

void __libc_start_main(long *stack) __attribute__((noreturn));
void __libc_start_main(long *stack) {
    int argc = (int)stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = argv + argc + 1;

    environ = envp;
    char **p = envp;
    while (*p) {
        p++;
    }
    __auxv = p + 1;

    __libc_init_array();
    exit(main(argc, argv, envp));
}
