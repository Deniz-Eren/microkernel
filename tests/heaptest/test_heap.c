/**
 * @file     test_heap.c
 * @brief    QEMU test for mmap-backed thread-safe malloc, free, and realloc
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: VER_MEM_0003 */

#define WORKER_COUNT 4
#define WORKER_ITERS 48

static int check_pattern(const uint8_t *buf, size_t len, uint8_t pat) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != pat) {
            return 1;
        }
    }
    return 0;
}

static int test_alignment_and_slab_reclamation(void) {
    static const size_t sizes[] = {0u, 1u, 7u, 16u, 127u, 1024u, 16384u};
    void *ptrs[7];
    for (size_t i = 0; i < 7u; i++) {
        ptrs[i] = malloc(sizes[i]);
        if (!ptrs[i] || (((uintptr_t)ptrs[i] & 15u) != 0u)) {
            printf("[FAIL] malloc(%zu) alignment or OOM\n", sizes[i]);
            return 1;
        }
        if (sizes[i] > 0u) {
            memset(ptrs[i], (int)(0xA0u + i), sizes[i]);
        }
    }
    for (size_t i = 0; i < 7u; i++) {
        free(ptrs[i]);
    }

    /* calloc must zero reused slab memory that previously held 0xA0+ bytes. */
    uint8_t *z = (uint8_t *)calloc(256u, sizeof(uint8_t));
    if (!z || check_pattern(z, 256u, 0u) != 0) {
        printf("[FAIL] calloc reused block not zeroed\n");
        return 1;
    }
    free(z);

    /* Churn 10 rounds x 128 x 16 KiB = 20 MiB to prove slab reclamation
     * exceeds the old 1 MiB bump arena cleanly. */
    void *batch[128];
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 128; i++) {
            batch[i] = malloc(16384u);
            if (!batch[i] || (((uintptr_t)batch[i] & 15u) != 0u)) {
                printf("[FAIL] slab churn OOM round=%d i=%d\n", round, i);
                return 1;
            }
            uint8_t pat = (uint8_t)((round * 13 + i) & 0xFF);
            memset(batch[i], pat, 16384u);
        }
        for (int i = 0; i < 128; i++) {
            uint8_t pat = (uint8_t)((round * 13 + i) & 0xFF);
            if (check_pattern((const uint8_t *)batch[i], 16384u, pat) != 0) {
                printf("[FAIL] slab churn corruption round=%d i=%d\n", round,
                       i);
                return 1;
            }
            free(batch[i]);
        }
    }
    printf("[OK] mmap slab coalescing, alignment, and 20 MiB churn verified\n");
    return 0;
}

static int test_large_munmap_and_realloc(void) {
    /* 64 x 256 KiB = 16 MiB cumulative large mmap/munmap reclamation. */
    for (int i = 0; i < 64; i++) {
        uint8_t *big = (uint8_t *)malloc(256u * 1024u);
        if (!big || (((uintptr_t)big & 15u) != 0u)) {
            printf("[FAIL] large malloc(256 KiB) failed at iter=%d\n", i);
            return 1;
        }
        big[0] = (uint8_t)i;
        big[256u * 1024u - 1u] = (uint8_t)(i ^ 0x5Au);
        if (big[0] != (uint8_t)i ||
            big[256u * 1024u - 1u] != (uint8_t)(i ^ 0x5Au)) {
            printf("[FAIL] large block readback mismatch\n");
            return 1;
        }
        /* POSIX.1-2024: free() must preserve errno. */
        errno = EAGAIN;
        free(big);
        if (errno != EAGAIN) {
            printf("[FAIL] free() modified errno=%d\n", errno);
            return 1;
        }
    }

    /* Cross slab <-> large boundaries with realloc and reallocarray. */
    uint8_t *p = (uint8_t *)malloc(32u);
    if (!p) {
        printf("[FAIL] realloc seed malloc failed\n");
        return 1;
    }
    for (size_t i = 0; i < 32u; i++) {
        p[i] = (uint8_t)(0x30u + i);
    }

    p = (uint8_t *)realloc(p, 4096u);
    if (!p || p[0] != 0x30u || p[31] != 0x4Fu) {
        printf("[FAIL] realloc 32 -> 4096 lost data\n");
        return 1;
    }
    memset(p + 32, 0x55, 4064u);

    p = (uint8_t *)realloc(p, 65536u);
    if (!p || p[0] != 0x30u || p[31] != 0x4Fu || p[4095] != 0x55u) {
        printf("[FAIL] realloc 4096 -> 65536 (slab->large) lost data\n");
        return 1;
    }

    errno = 0;
    void *ovf = reallocarray(p, (size_t)-1 / 8u + 2u, 16u);
    if (ovf != NULL || errno != ENOMEM || p[0] != 0x30u || p[4095] != 0x55u) {
        printf("[FAIL] reallocarray overflow clobbered state\n");
        return 1;
    }

    p = (uint8_t *)realloc(p, 128u);
    if (!p || p[0] != 0x30u || p[31] != 0x4Fu || p[127] != 0x55u) {
        printf("[FAIL] realloc 65536 -> 128 (large->slab) lost data\n");
        return 1;
    }

    if (realloc(p, 0u) != NULL) {
        printf("[FAIL] realloc(p, 0) expected NULL\n");
        return 1;
    }

    printf("[OK] large munmap reclamation, free errno, and realloc boundaries "
           "verified\n");
    return 0;
}

static void *heap_worker(void *arg) {
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < WORKER_ITERS; i++) {
        size_t sz = (size_t)(64 + ((id * 37 + i * 193) % 4096));
        uint8_t pat = (uint8_t)((id * 53 + i) & 0xFF);
        uint8_t *s = (uint8_t *)malloc(sz);
        if (!s || (((uintptr_t)s & 15u) != 0u)) {
            return (void *)(intptr_t)1;
        }
        memset(s, pat, sz);

        uint8_t *g = (uint8_t *)realloc(s, sz + 512u);
        if (!g || check_pattern(g, sz, pat) != 0) {
            free(g ? g : s);
            return (void *)(intptr_t)2;
        }

        uint8_t *big = (uint8_t *)calloc(1u, 48u * 1024u);
        if (!big || big[0] != 0u || big[48u * 1024u - 1u] != 0u) {
            free(g);
            free(big);
            return (void *)(intptr_t)3;
        }
        big[0] = pat;
        big[48u * 1024u - 1u] = pat;
        if (big[0] != pat || big[48u * 1024u - 1u] != pat) {
            free(g);
            free(big);
            return (void *)(intptr_t)4;
        }
        free(big);
        free(g);
    }
    return NULL;
}

static int test_multithreaded_heap(void) {
    pthread_t th[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) {
        if (pthread_create(&th[i], NULL, heap_worker,
                           (void *)(intptr_t)(i + 1)) != 0) {
            printf("[FAIL] pthread_create heap_worker %d failed\n", i);
            return 1;
        }
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        void *res = NULL;
        if (pthread_join(th[i], &res) != 0 || res != NULL) {
            printf("[FAIL] heap_worker %d failed rc=%ld\n", i,
                   (long)(intptr_t)res);
            return 1;
        }
    }
    printf("[OK] 4-thread concurrent mmap malloc/calloc/realloc/free stress "
           "verified\n");
    return 0;
}

int main(void) {
    if (test_alignment_and_slab_reclamation() != 0) {
        return 1;
    }
    if (test_large_munmap_and_realloc() != 0) {
        return 1;
    }
    if (test_multithreaded_heap() != 0) {
        return 1;
    }
    return 0;
}
