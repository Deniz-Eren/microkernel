/**
 * @file     test_libc_issue8.c
 * @brief    QEMU test for POSIX.1-2024 strlcpy, reallocarray, getentropy, uchar
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <unistd.h>

/* need:: VER_LIBC_0005 */

static int test_strlcpy_strlcat(void) {
    char buf[8];
    size_t r = strlcpy(buf, "microkernel", sizeof(buf));
    if (r != 11u || strcmp(buf, "microke") != 0) {
        printf("[FAIL] strlcpy truncation r=%zu buf='%s'\n", r, buf);
        return 1;
    }
    r = strlcpy(buf, "hi", sizeof(buf));
    if (r != 2u || strcmp(buf, "hi") != 0) {
        printf("[FAIL] strlcpy exact r=%zu buf='%s'\n", r, buf);
        return 1;
    }
    r = strlcat(buf, "-posix2024", sizeof(buf));
    if (r != 12u || strcmp(buf, "hi-posi") != 0) {
        printf("[FAIL] strlcat truncation r=%zu buf='%s'\n", r, buf);
        return 1;
    }
    if (strlcat(buf, "xyz", 0u) != 3u) {
        printf("[FAIL] strlcat(dstsize=0) mismatch\n");
        return 1;
    }
    return 0;
}

static int test_reallocarray_and_getenv(void) {
    uint32_t *arr = (uint32_t *)reallocarray(NULL, 4u, sizeof(uint32_t));
    if (!arr) {
        printf("[FAIL] reallocarray initial alloc failed\n");
        return 1;
    }
    for (uint32_t i = 0; i < 4u; i++) {
        arr[i] = 100u + i;
    }

    /* Overflow must fail with ENOMEM and leave existing block intact. */
    errno = 0;
    void *bad = reallocarray(arr, (size_t)-1 / 2u + 2u, 4u);
    if (bad != NULL || errno != ENOMEM) {
        printf("[FAIL] reallocarray overflow errno=%d\n", errno);
        return 1;
    }

    uint32_t *grown = (uint32_t *)reallocarray(arr, 8u, sizeof(uint32_t));
    if (!grown || grown[0] != 100u || grown[3] != 103u) {
        printf("[FAIL] reallocarray grow lost data\n");
        return 1;
    }
    free(grown);

    /* Verify getenv and secure_getenv against an installed environ array. */
    char *fake_env[] = {"ISSUE8_KEY=ok-2024", "OTHER=1", NULL};
    char **saved_env = environ;
    environ = fake_env;
    const char *v1 = getenv("ISSUE8_KEY");
    const char *v2 = secure_getenv("ISSUE8_KEY");
    const char *v3 = secure_getenv("MISSING_KEY");
    environ = saved_env;
    if (!v1 || !v2 || strcmp(v1, "ok-2024") != 0 ||
        strcmp(v2, "ok-2024") != 0 || v3 != NULL) {
        printf("[FAIL] getenv/secure_getenv mismatch\n");
        return 1;
    }
    printf("[OK] strlcpy, strlcat, reallocarray, and secure_getenv verified\n");
    return 0;
}

static int test_getentropy(void) {
    uint8_t a[32];
    uint8_t b[32];
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    if (getentropy(a, sizeof(a)) != 0 || getentropy(b, sizeof(b)) != 0) {
        printf("[FAIL] getentropy(32) failed (errno=%d)\n", errno);
        return 1;
    }
    if (memcmp(a, b, sizeof(a)) == 0) {
        printf("[FAIL] getentropy returned two identical 32-byte blocks\n");
        return 1;
    }
    uint8_t big[260];
    errno = 0;
    if (getentropy(big, 257u) != -1 || errno != EINVAL) {
        printf("[FAIL] getentropy(257) expected EINVAL, got errno=%d\n", errno);
        return 1;
    }
    printf("[OK] getentropy(32) uniqueness and >256 EINVAL verified\n");
    return 0;
}

static int test_c17_headers(void) {
    /* <stdatomic.h> and <stdalign.h> */
    alignas(16) atomic_uint counter = 40u;
    if (((uintptr_t)&counter & 15u) != 0u || alignof(max_align_t) == 0u) {
        printf("[FAIL] stdalign alignment check failed\n");
        return 1;
    }
    atomic_fetch_add_explicit(&counter, 2u, memory_order_acq_rel);
    if (atomic_load_explicit(&counter, memory_order_acquire) != 42u) {
        printf("[FAIL] stdatomic fetch_add/load mismatch\n");
        return 1;
    }

    /* <uchar.h>: UTF-8 <-> UTF-32 and UTF-16 surrogate pair for U+1F600. */
    const char utf8_emoji[] = "\xF0\x9F\x98\x80"; /* U+1F600 */
    mbstate_t st = {0};
    char32_t c32 = 0u;
    size_t n = mbrtoc32(&c32, utf8_emoji, 4u, &st);
    if (n != 4u || c32 != 0x1F600u) {
        printf("[FAIL] mbrtoc32 n=%zu cp=0x%x\n", n, (unsigned)c32);
        return 1;
    }
    char out32[8] = {0};
    size_t w32 = c32rtomb(out32, c32, &st);
    if (w32 != 4u || memcmp(out32, utf8_emoji, 4u) != 0) {
        printf("[FAIL] c32rtomb round-trip failed\n");
        return 1;
    }

    memset(&st, 0, sizeof(st));
    char16_t hi = 0u;
    char16_t lo = 0u;
    size_t r1 = mbrtoc16(&hi, utf8_emoji, 4u, &st);
    size_t r2 = mbrtoc16(&lo, utf8_emoji + 4, 0u, &st);
    if (r1 != 4u || r2 != (size_t)-3 || hi != 0xD83Du || lo != 0xDE00u) {
        printf("[FAIL] mbrtoc16 surrogate pair failed\n");
        return 1;
    }
    memset(&st, 0, sizeof(st));
    char out16[8] = {0};
    if (c16rtomb(out16, hi, &st) != 0u || c16rtomb(out16, lo, &st) != 4u ||
        memcmp(out16, utf8_emoji, 4u) != 0) {
        printf("[FAIL] c16rtomb surrogate pair round-trip failed\n");
        return 1;
    }
    printf("[OK] <stdatomic.h>, <stdalign.h>, and <uchar.h> UTF-16/UTF-32 "
           "verified\n");
    return 0;
}

int main(void) {
    if (test_strlcpy_strlcat() != 0) {
        return 1;
    }
    if (test_reallocarray_and_getenv() != 0) {
        return 1;
    }
    if (test_getentropy() != 0) {
        return 1;
    }
    if (test_c17_headers() != 0) {
        return 1;
    }
    return 0;
}
