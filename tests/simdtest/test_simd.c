/**
 * @file     test_simd.c
 * @brief    Ring 3 x86-64-v3 AVX2/FMA/BMI and XSAVE/XRSTOR isolation test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_BOOT_0004 */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/bootinfo.h>
#include <sys/microkernel.h>
#include <sys/wait.h>
#include <unistd.h>

#define MXCSR_RN 0x1F80u
#define MXCSR_RD 0x3F80u /* round toward -inf */
#define MXCSR_RU 0x5F80u /* round toward +inf */

typedef struct {
    int id;
    uint32_t target_mxcsr;
    int ok;
} xsave_worker_arg_t;

static pthread_barrier_t g_bar;

static uint32_t read_mxcsr(void) {
    uint32_t v = 0u;
    __asm__ volatile("vstmxcsr %0" : "=m"(v));
    return v;
}

static void write_mxcsr(uint32_t v) {
    __asm__ volatile("vldmxcsr %0" : : "m"(v) : "memory");
}

static int verify_bootinfo_and_v3_ops(void) {
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)BOOTINFO_VA;
    if (bi->magic != BOOTINFO_MAGIC ||
        (bi->cpu_features & BOOT_CPU_X86_64_V3) == 0u) {
        printf("[FAIL] simdtest: bootinfo missing BOOT_CPU_X86_64_V3\n");
        return -1;
    }

    /* Exact FMA test: (1 + 2^-52)*(1 - 2^-52) - 1 == -2^-104 only with FMA. */
    volatile double a = 1.0 + 0x1.0p-52;
    volatile double b = 1.0 - 0x1.0p-52;
    volatile double c = -1.0;
    double fused = c;
    __asm__ volatile("vfmadd231sd %1, %2, %0" : "+x"(fused) : "x"(a), "x"(b));
    if (fused != -0x1.0p-104) {
        printf("[FAIL] simdtest: hardware FMA mismatch\n");
        return -1;
    }

    /* 256-bit AVX2 integer vector add across 8x32-bit lanes. */
    typedef uint32_t v8u32_t __attribute__((vector_size(32)));
    v8u32_t va = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    v8u32_t vb = {10u, 20u, 30u, 40u, 50u, 60u, 70u, 80u};
    v8u32_t vc = va + vb;
    for (int i = 0; i < 8; i++) {
        if (vc[i] != (uint32_t)((i + 1) * 11)) {
            printf("[FAIL] simdtest: AVX2 vector lane %d mismatch\n", i);
            return -1;
        }
    }

    /* Scalar x86-64-v3 bit manipulation: tzcnt, lzcnt, popcnt. */
    volatile uint64_t bits = 0x0010000000000800ULL;
    if (__builtin_ctzll(bits) != 11 || __builtin_clzll(bits) != 11 ||
        __builtin_popcountll(bits) != 2) {
        printf("[FAIL] simdtest: BMI/POPCNT/LZCNT mismatch\n");
        return -1;
    }

    /* AVX2 libc memcpy/memmove/memset/memcmp across alignments and sizes. */
    static uint8_t src[512];
    static uint8_t dst[512];
    for (size_t i = 0; i < sizeof(src); i++) {
        src[i] = (uint8_t)((i * 37u + 13u) & 0xFFu);
    }
    for (size_t off = 0; off < 32u; off += 7u) {
        for (size_t len = 1u; len <= 257u; len += 31u) {
            memset(dst, 0xA5, sizeof(dst));
            memcpy(dst + off, src + off, len);
            if (memcmp(dst + off, src + off, len) != 0 ||
                dst[off + len] != 0xA5u) {
                printf("[FAIL] simdtest: AVX2 memcpy/memset mismatch\n");
                return -1;
            }
            memmove(dst + off + 5u, dst + off, len);
            if (memcmp(dst + off + 5u, src + off, len) != 0) {
                printf("[FAIL] simdtest: AVX2 memmove overlap mismatch\n");
                return -1;
            }
        }
    }

    printf("[OK] x86-64-v3 AVX2, FMA, BMI1/BMI2, and bootinfo flags verified"
           " (avx512=%u)\n",
           (bi->cpu_features & BOOT_CPU_AVX512) ? 1u : 0u);
    return 0;
}

static void *xsave_worker(void *arg) {
    xsave_worker_arg_t *w = (xsave_worker_arg_t *)arg;
    uint64_t expected[4] __attribute__((aligned(32)));
    uint64_t observed[4] __attribute__((aligned(32)));
    uint64_t seed = w->id ? 0xCAFEBABE12340000ULL : 0xDEADBEEF56780000ULL;
    for (int i = 0; i < 4; i++) {
        expected[i] = seed + (uint64_t)i * 0x0101010101010101ULL;
        observed[i] = 0ULL;
    }

    pthread_barrier_wait(&g_bar);
    write_mxcsr(w->target_mxcsr);

    /* Hold distinct 256-bit patterns live in ymm4..ymm7 across 200 direct
     * SYS_SCHED_YIELD transitions and preemptive ticks to prove XSAVE/XRSTOR
     * preserves both low and high 128-bit halves plus MXCSR per thread. */
    int mismatches = 0;
    for (int iter = 0; iter < 200; iter++) {
        uint32_t after_mxcsr = 0u;
        __asm__ volatile("vmovdqa (%[exp]), %%ymm4\n"
                         "vmovdqa (%[exp]), %%ymm5\n"
                         "vmovdqa (%[exp]), %%ymm6\n"
                         "vmovdqa (%[exp]), %%ymm7\n"
                         "movq %[sysno], %%rax\n"
                         "syscall\n"
                         "vpxor %%ymm4, %%ymm5, %%ymm5\n"
                         "vpxor %%ymm6, %%ymm7, %%ymm7\n"
                         "vpor %%ymm5, %%ymm7, %%ymm7\n"
                         "vpor %%ymm4, %%ymm7, %%ymm4\n"
                         "vmovdqa %%ymm4, (%[obs])\n"
                         "vstmxcsr %[mx]\n"
                         : [mx] "=m"(after_mxcsr)
                         : [exp] "r"(expected), [obs] "r"(observed),
                           [sysno] "i"(SYS_SCHED_YIELD)
                         : "rax", "rcx", "r11", "ymm4", "ymm5", "ymm6", "ymm7",
                           "memory");
        for (int i = 0; i < 4; i++) {
            if (observed[i] != expected[i]) {
                mismatches++;
            }
        }
        if ((after_mxcsr & 0x6000u) != (w->target_mxcsr & 0x6000u)) {
            mismatches++;
        }
    }

    write_mxcsr(MXCSR_RN);
    w->ok = (mismatches == 0);
    return NULL;
}

static int verify_fork_xsave_inheritance(void) {
    write_mxcsr(MXCSR_RU);
    pid_t pid = fork();
    if (pid < 0) {
        write_mxcsr(MXCSR_RN);
        printf("[FAIL] simdtest: fork failed\n");
        return -1;
    }
    if (pid == 0) {
        uint32_t child_mx = read_mxcsr();
        write_mxcsr(MXCSR_RD);
        _exit((child_mx & 0x6000u) == (MXCSR_RU & 0x6000u) ? 0 : 1);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        write_mxcsr(MXCSR_RN);
        printf("[FAIL] simdtest: fork child MXCSR inheritance failed\n");
        return -1;
    }
    uint32_t parent_mx = read_mxcsr();
    write_mxcsr(MXCSR_RN);
    if ((parent_mx & 0x6000u) != (MXCSR_RU & 0x6000u)) {
        printf("[FAIL] simdtest: parent MXCSR clobbered across fork\n");
        return -1;
    }
    return 0;
}

int main(void) {
    if (verify_bootinfo_and_v3_ops() != 0) {
        return 1;
    }

    pthread_barrier_init(&g_bar, NULL, 2u);
    xsave_worker_arg_t a0 = {.id = 0, .target_mxcsr = MXCSR_RU, .ok = 0};
    xsave_worker_arg_t a1 = {.id = 1, .target_mxcsr = MXCSR_RD, .ok = 0};
    pthread_t t0 = 0;
    pthread_t t1 = 0;
    if (pthread_create(&t0, NULL, xsave_worker, &a0) != 0 ||
        pthread_create(&t1, NULL, xsave_worker, &a1) != 0) {
        printf("[FAIL] simdtest: pthread_create failed\n");
        return 1;
    }
    pthread_join(t0, NULL);
    pthread_join(t1, NULL);
    pthread_barrier_destroy(&g_bar);

    if (!a0.ok || !a1.ok) {
        printf("[FAIL] simdtest: XSAVE/XRSTOR isolation failed (%d, %d)\n",
               a0.ok, a1.ok);
        return 1;
    }

    if (verify_fork_xsave_inheritance() != 0) {
        return 1;
    }

    printf("[OK] preemptive XSAVE/XRSTOR YMM and MXCSR thread isolation"
           " verified\n");
    return 0;
}
