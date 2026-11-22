/**
 * @file     cov_runtime.c
 * @brief    Freestanding Ring 3 LLVM profile runtime registration and reset
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/microkernel.h>

/* need:: COMP_DOCS_0004 */

void __libc_cov_init(const char *progname);
void __libc_cov_reset_counters(void);

#ifdef MICROKERNEL_COVERAGE

/* Uninstrumented runtime stubs required by Clang -fprofile-instr-generate. */
int __llvm_profile_runtime = 0;

void __llvm_profile_register_function(void *data);
void __llvm_profile_register_function(void *data) {
    (void)data;
}

void __llvm_profile_register_names_function(void *names, uint64_t size);
void __llvm_profile_register_names_function(void *names, uint64_t size) {
    (void)names;
    (void)size;
}

extern char __start___llvm_prf_data[];
extern char __stop___llvm_prf_data[];
extern uint64_t __start___llvm_prf_cnts[];
extern uint64_t __stop___llvm_prf_cnts[];
extern char __start___llvm_prf_names[];
extern char __stop___llvm_prf_names[];

void __libc_cov_init(const char *progname) {
    struct __k_cov_desc desc;
    desc.data_begin = (uint64_t)(uintptr_t)__start___llvm_prf_data;
    desc.data_end = (uint64_t)(uintptr_t)__stop___llvm_prf_data;
    desc.cnts_begin = (uint64_t)(uintptr_t)__start___llvm_prf_cnts;
    desc.cnts_end = (uint64_t)(uintptr_t)__stop___llvm_prf_cnts;
    desc.names_begin = (uint64_t)(uintptr_t)__start___llvm_prf_names;
    desc.names_end = (uint64_t)(uintptr_t)__stop___llvm_prf_names;
    if (desc.data_end <= desc.data_begin || desc.cnts_end <= desc.cnts_begin) {
        return;
    }

    const char *base = progname ? progname : "app";
    for (const char *p = base; *p != '\0'; p++) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    if (*base == '\0') {
        base = "app";
    }
    size_t i = 0;
    while (i + 1u < sizeof(desc.name) && base[i] != '\0') {
        desc.name[i] = base[i];
        i++;
    }
    while (i < sizeof(desc.name)) {
        desc.name[i] = '\0';
        i++;
    }
    (void)__sys2(SYS_COV_WRITE, (long)COV_OP_REGISTER, (long)(uintptr_t)&desc);
}

void __libc_cov_reset_counters(void) {
    for (uint64_t *p = __start___llvm_prf_cnts; p < __stop___llvm_prf_cnts;
         p++) {
        *p = 0;
    }
}

#else

void __libc_cov_init(const char *progname) {
    (void)progname;
}

void __libc_cov_reset_counters(void) {
}

#endif
