/**
 * @file     test_dlsoctest_dyn.c
 * @brief    Dynamic linker test executable (freestanding ET_DYN + PT_INTERP)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <sys/errno.h>
#include <sys/microkernel.h>
#include <sys/vm.h>

/* need:: VER_PROC_0011 VER_LIBC_0032 */

/* Distinctive exit codes printed by the dlsoctest driver on failure. */
#define DYN_EXIT_AUXV 10
#define DYN_EXIT_VALUE 11
#define DYN_EXIT_ADD 12
#define DYN_EXIT_DATA 13
#define DYN_EXIT_INIT 14
#define DYN_EXIT_WX 15

#define AT_NULL 0UL
#define AT_PHDR 3UL
#define AT_PHENT 4UL
#define AT_PHNUM 5UL
#define AT_BASE 7UL
#define AT_ENTRY 9UL

extern int mtest_value(void);
extern int mtest_add(int a, int b);
extern int mtest_init_count(void);
extern int mtest_data;

static void dyn_exit(long code) __attribute__((noreturn));

static void dyn_exit(long code) {
    __sys1(SYS_PROC_EXIT, code);
    for (;;) {
    }
}

void dyn_main(long *stack) __attribute__((noreturn));

void dyn_main(long *stack) {
    long argc = stack[0];
    char **argv = (char **)(stack + 1);
    char **envp = argv + argc + 1;
    char **e = envp;
    while (*e) {
        e++;
    }
    const unsigned long *auxv = (const unsigned long *)(e + 1);

    unsigned long at_base = 0;
    unsigned long at_entry = 0;
    unsigned long at_phdr = 0;
    unsigned long at_phnum = 0;
    unsigned long at_phent = 0;
    for (size_t i = 0u; auxv[i] != AT_NULL; i += 2u) {
        switch (auxv[i]) {
        case AT_BASE:
            at_base = auxv[i + 1u];
            break;
        case AT_ENTRY:
            at_entry = auxv[i + 1u];
            break;
        case AT_PHDR:
            at_phdr = auxv[i + 1u];
            break;
        case AT_PHENT:
            at_phent = auxv[i + 1u];
            break;
        case AT_PHNUM:
            at_phnum = auxv[i + 1u];
            break;
        default:
            break;
        }
    }
    /* auxv must describe the interpreter (AT_BASE), the main image
     * (AT_ENTRY/AT_PHDR), and a valid 56-byte program header table. */
    if (at_base == 0u || at_entry == 0u || at_phdr == 0u || at_phent != 56u ||
        at_phnum == 0u) {
        dyn_exit(DYN_EXIT_AUXV);
    }
    const unsigned char *main_hdr =
        (const unsigned char *)(uintptr_t)(at_phdr & ~0xFFFUL);
    const unsigned char *ldso_hdr = (const unsigned char *)(uintptr_t)at_base;
    if (main_hdr[0] != 0x7Fu || main_hdr[1] != 'E' || main_hdr[2] != 'L' ||
        main_hdr[3] != 'F' || ldso_hdr[0] != 0x7Fu || ldso_hdr[1] != 'E' ||
        ldso_hdr[2] != 'L' || ldso_hdr[3] != 'F') {
        dyn_exit(DYN_EXIT_AUXV);
    }

    /* DT_NEEDED resolution must honor LD_LIBRARY_PATH ahead of the default
     * /boot directory: the driver copies the value-2 variant into
     * /tmp/libs/libmtest.so and exports LD_LIBRARY_PATH=/tmp/libs. */
    if (mtest_value() != 2) {
        dyn_exit(DYN_EXIT_VALUE);
    }
    if (mtest_add(20, 22) != 42) {
        dyn_exit(DYN_EXIT_ADD);
    }
    if (mtest_data != 42) {
        dyn_exit(DYN_EXIT_DATA);
    }
    if (mtest_init_count() < 1) {
        dyn_exit(DYN_EXIT_INIT);
    }

    /* Kernel W^X policy: making an executable page writable must fail. */
    long rc = VMProtect((uintptr_t)dyn_main & ~0xFFFUL, 4096,
                        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXEC);
    if (rc != -EACCES) {
        dyn_exit(DYN_EXIT_WX);
    }

    dyn_exit(0);
}
