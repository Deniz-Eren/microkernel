/**
 * @file     test_libc_issue8.c
 * @brief    QEMU test for POSIX.1-2024 strlcpy, reallocarray, getentropy, uchar
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <uchar.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/* need:: VER_LIBC_0005 VER_LIBC_0013 */

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

static int test_locale_and_l_helpers(void) {
    /* Startup locale must be "C" with MB_CUR_MAX == 1. */
    const char *init_loc = setlocale(LC_ALL, NULL);
    if (!init_loc || strcmp(init_loc, "C") != 0 || MB_CUR_MAX != 1u) {
        printf("[FAIL] initial locale='%s' MB_CUR_MAX=%zu\n",
               init_loc ? init_loc : "?", MB_CUR_MAX);
        return 1;
    }
    struct lconv *lc = localeconv();
    if (!lc || !lc->decimal_point || strcmp(lc->decimal_point, ".") != 0 ||
        lc->frac_digits != CHAR_MAX) {
        printf("[FAIL] localeconv() C defaults mismatch\n");
        return 1;
    }

    /* Per-thread locale via newlocale/duplocale/uselocale/getlocalename_l. */
    locale_t utf8 = newlocale(LC_ALL_MASK, "C.UTF-8", (locale_t)0);
    if (!utf8) {
        printf("[FAIL] newlocale(C.UTF-8) failed errno=%d\n", errno);
        return 1;
    }
    locale_t dup = duplocale(utf8);
    if (!dup) {
        printf("[FAIL] duplocale failed errno=%d\n", errno);
        freelocale(utf8);
        return 1;
    }
    const char *n_ctype = getlocalename_l(LC_CTYPE, dup);
    const char *n_glob = getlocalename_l(LC_CTYPE, LC_GLOBAL_LOCALE);
    if (!n_ctype || strcmp(n_ctype, "C.UTF-8") != 0 || !n_glob ||
        strcmp(n_glob, "C") != 0) {
        printf("[FAIL] getlocalename_l mismatch\n");
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }
    errno = 0;
    if (getlocalename_l(LC_ALL, dup) != NULL || errno != EINVAL) {
        printf("[FAIL] getlocalename_l(LC_ALL) expected EINVAL\n");
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }
    errno = 0;
    if (newlocale(LC_CTYPE_MASK, "invalid_LOCALE", (locale_t)0) !=
            (locale_t)0 ||
        errno != ENOENT) {
        printf("[FAIL] newlocale(invalid) expected ENOENT\n");
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }

    locale_t prev = uselocale(dup);
    if (prev != LC_GLOBAL_LOCALE || uselocale((locale_t)0) != dup ||
        MB_CUR_MAX != 4u) {
        printf("[FAIL] uselocale(C.UTF-8) failed MB_CUR_MAX=%zu\n", MB_CUR_MAX);
        (void)uselocale(LC_GLOBAL_LOCALE);
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }

    /* Locale-aware *_l helpers across ctype/strings/string/time. */
    char xbuf[16] = {0};
    char tbuf[32] = {0};
    struct tm tm0 = {0};
    tm0.tm_year = 124;
    tm0.tm_mon = 4;
    tm0.tm_mday = 15;
    if (!isalpha_l('z', dup) || toupper_l('a', dup) != 'A' ||
        tolower_l('Z', dup) != 'z' ||
        strcasecmp_l("Hello", "hELLo", dup) != 0 ||
        strncasecmp_l("abcX", "ABCy", 3u, dup) != 0 ||
        strcoll("alpha", "beta") >= 0 || strcoll_l("a", "a", dup) != 0 ||
        strxfrm(xbuf, "posix", sizeof(xbuf)) != 5u ||
        strcmp(xbuf, "posix") != 0 ||
        strxfrm_l(xbuf, "utf8", sizeof(xbuf), dup) != 4u ||
        strcmp(xbuf, "utf8") != 0 ||
        strcmp(strerror_l(EINVAL, dup), "Invalid argument") != 0 ||
        strftime_l(tbuf, sizeof(tbuf), "%Y-%m-%d", &tm0, dup) != 10u ||
        strcmp(tbuf, "2024-05-15") != 0) {
        printf("[FAIL] *_l helper verification failed\n");
        (void)uselocale(LC_GLOBAL_LOCALE);
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }

    (void)uselocale(LC_GLOBAL_LOCALE);
    if (MB_CUR_MAX != 1u) {
        printf("[FAIL] uselocale(LC_GLOBAL_LOCALE) did not restore C\n");
        freelocale(dup);
        freelocale(utf8);
        return 1;
    }
    freelocale(dup);
    freelocale(utf8);
    printf("[OK] <locale.h> C/C.UTF-8, uselocale, getlocalename_l, and *_l "
           "helpers verified\n");
    return 0;
}

static int test_multibyte_and_wctype(void) {
    if (!setlocale(LC_ALL, "C.UTF-8") || MB_CUR_MAX != 4u) {
        printf("[FAIL] setlocale(LC_ALL, C.UTF-8) failed\n");
        return 1;
    }
    if (btowc('A') != (wint_t)'A' || btowc(0x80) != WEOF ||
        btowc(EOF) != WEOF || wctob((wint_t)'Z') != 'Z' ||
        wctob((wint_t)0x00E9u) != EOF || wctob(WEOF) != EOF) {
        printf("[FAIL] btowc/wctob mismatch\n");
        return 1;
    }

    /* Multibyte string: "A" + U+00E9 (é) + U+20AC (€) + U+1F600 (😀). */
    const char utf8_str[] = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
    wchar_t wout[8] = {0};
    if (mblen(NULL, 0u) != 0 || mbtowc(NULL, NULL, 0u) != 0 ||
        wctomb(NULL, 0) != 0 || mblen(utf8_str + 1, 4u) != 2 ||
        mblen(utf8_str + 3, 4u) != 3 || mblen(utf8_str + 6, 4u) != 4) {
        printf("[FAIL] mblen/mbtowc/wctomb lengths failed\n");
        return 1;
    }
    size_t nw = mbstowcs(wout, utf8_str, 8u);
    if (nw != 4u || wout[0] != L'A' || wout[1] != (wchar_t)0x00E9 ||
        wout[2] != (wchar_t)0x20AC || wout[3] != (wchar_t)0x1F600 ||
        wout[4] != L'\0') {
        printf("[FAIL] mbstowcs nw=%zu\n", nw);
        return 1;
    }
    char cout[16] = {0};
    size_t nb = wcstombs(cout, wout, sizeof(cout));
    if (nb != 10u || strcmp(cout, utf8_str) != 0) {
        printf("[FAIL] wcstombs round-trip failed nb=%zu\n", nb);
        return 1;
    }

    /* Restartable mbrtowc split across bytes + mbsnrtowcs/wcsnrtombs. */
    mbstate_t st = {0};
    wchar_t wc = 0;
    if (!mbsinit(&st) || mbrtowc(&wc, utf8_str + 6, 2u, &st) != (size_t)-2 ||
        mbsinit(&st) || mbrtowc(&wc, utf8_str + 8, 2u, &st) != 2u ||
        !mbsinit(&st) || wc != (wchar_t)0x1F600 ||
        mbrlen(utf8_str + 1, 2u, &st) != 2u) {
        printf("[FAIL] restartable mbrtowc/mbsinit/mbrlen failed\n");
        return 1;
    }
    const char *src_mb = utf8_str;
    wchar_t wpart[4] = {0};
    if (mbsnrtowcs(wpart, &src_mb, 3u, 4u, &st) != 2u || wpart[0] != L'A' ||
        wpart[1] != (wchar_t)0x00E9 || src_mb != utf8_str + 3) {
        printf("[FAIL] mbsnrtowcs partial failed\n");
        return 1;
    }
    const wchar_t *src_wc = wout;
    char cpart[8] = {0};
    if (wcsnrtombs(cpart, &src_wc, 2u, sizeof(cpart), &st) != 3u ||
        memcmp(cpart, "A\xC3\xA9", 3u) != 0 || src_wc != wout + 2) {
        printf("[FAIL] wcsnrtombs partial failed\n");
        return 1;
    }

    /* Invalid UTF-8 byte sequence must fail with EILSEQ. */
    errno = 0;
    if (mbrtowc(&wc, "\xFF", 1u, &st) != (size_t)-1 || errno != EILSEQ) {
        printf("[FAIL] mbrtowc invalid byte expected EILSEQ\n");
        return 1;
    }

    /* <wctype.h> Unicode C.UTF-8 classification and case mapping vs C. */
    locale_t c_loc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
    if (!c_loc || !iswalpha((wint_t)0x00E9u) || !iswlower((wint_t)0x00E9u) ||
        towupper((wint_t)0x00E9u) != (wint_t)0x00C9u ||
        towlower((wint_t)0x03A0u) != (wint_t)0x03C0u ||
        towupper((wint_t)0x0434u) != (wint_t)0x0414u ||
        !iswspace((wint_t)0x3000u) || !iswprint((wint_t)0x1F600u) ||
        iswalpha_l((wint_t)0x00E9u, c_loc) != 0 ||
        towupper_l((wint_t)0x00E9u, c_loc) != (wint_t)0x00E9u ||
        !iswctype((wint_t)0x00C9u, wctype("upper")) ||
        towctrans((wint_t)0x00C9u, wctrans("tolower")) != (wint_t)0x00E9u) {
        printf("[FAIL] <wctype.h> C.UTF-8/C classification failed\n");
        freelocale(c_loc);
        return 1;
    }
    freelocale(c_loc);
    printf("[OK] <stdlib.h> multibyte, mbr*/mbs*/wcs* UTF-8, and <wctype.h> "
           "verified\n");
    return 0;
}

static int test_wchar_strings_and_conv(void) {
    /* wmem* + wcslen/wcsnlen/wcpcpy/wcpncpy/wcslcpy/wcslcat/wcscasecmp. */
    wchar_t buf[32] = {0};
    (void)wmemset(buf, L'X', 4u);
    if (wmemcmp(buf, L"XXXX", 4u) != 0 || wmemchr(buf, L'X', 4u) != buf) {
        printf("[FAIL] wmemset/wmemcmp/wmemchr failed\n");
        return 1;
    }
    wchar_t *end = wcpcpy(buf, L"Hello");
    if (end != buf + 5 || wcslen(buf) != 5u || wcsnlen(buf, 3u) != 3u) {
        printf("[FAIL] wcpcpy/wcslen/wcsnlen failed\n");
        return 1;
    }
    if (wcslcat(buf, L" World!", 10u) != 12u ||
        wcscmp(buf, L"Hello Wor") != 0 ||
        wcscasecmp(L"Caf\u00e9", L"CAF\u00c9") != 0 ||
        wcsncasecmp(L"abX", L"ABy", 2u) != 0) {
        printf("[FAIL] wcslcat/wcscasecmp failed\n");
        return 1;
    }

    /* wcschr/wcsrchr/wcsstr/wcstok/wcsdup/wcscoll/wcsxfrm. */
    wchar_t *dup = wcsdup(L"one,two,three");
    if (!dup || wcsstr(dup, L"two") != dup + 4 ||
        wcschr(dup, L',') != dup + 3 || wcsrchr(dup, L',') != dup + 7) {
        printf("[FAIL] wcsdup/wcsstr/wcschr/wcsrchr failed\n");
        free(dup);
        return 1;
    }
    wchar_t *tok_st = NULL;
    wchar_t *t1 = wcstok(dup, L",", &tok_st);
    wchar_t *t2 = wcstok(NULL, L",", &tok_st);
    wchar_t *t3 = wcstok(NULL, L",", &tok_st);
    if (!t1 || !t2 || !t3 || wcscmp(t1, L"one") != 0 ||
        wcscmp(t2, L"two") != 0 || wcscmp(t3, L"three") != 0 ||
        wcstok(NULL, L",", &tok_st) != NULL) {
        printf("[FAIL] wcstok failed\n");
        free(dup);
        return 1;
    }
    free(dup);

    /* wcstol*, wcstoimax/wcstoumax, wcstod/wcstof/wcstold, and wcsftime. */
    wchar_t *ep = NULL;
    long v1 = wcstol(L"  -0x2aZ", &ep, 0);
    unsigned long long v2 = wcstoull(L"18446744073709551615", &ep, 10);
    intmax_t v3 = wcstoimax(L"-9876543210", NULL, 10);
    uintmax_t v4 = wcstoumax(L"deadbeef", NULL, 16);
    double d1 = wcstod(L"  -3.25e2!", &ep);
    if (v1 != -42L || v2 != ULLONG_MAX || v3 != -9876543210LL ||
        v4 != 0xDEADBEEFULL || fabs(d1 - (-325.0)) > 1e-12 || *ep != L'!' ||
        fabs((double)wcstof(L"1.5", NULL) - 1.5) > 1e-6 ||
        fabs((double)wcstold(L"2.5", NULL) - 2.5) > 1e-12) {
        printf("[FAIL] wcsto* conversions failed\n");
        return 1;
    }

    struct tm tm0 = {0};
    tm0.tm_year = 124;
    tm0.tm_mon = 1;
    tm0.tm_mday = 29;
    wchar_t wdate[32] = {0};
    size_t dn = wcsftime(wdate, 32u, L"%Y\u5e74%m-%d", &tm0);
    if (dn != 10u || wcscmp(wdate, L"2024\u5e7402-29") != 0) {
        printf("[FAIL] wcsftime failed dn=%zu\n", dn);
        return 1;
    }

    (void)setlocale(LC_ALL, "C");
    printf("[OK] <wchar.h> wmem*, wcs*, wcsto*, wcstoimax/wcstoumax, and "
           "wcsftime verified\n");
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
    if (test_locale_and_l_helpers() != 0) {
        return 1;
    }
    if (test_multibyte_and_wctype() != 0) {
        return 1;
    }
    if (test_wchar_strings_and_conv() != 0) {
        return 1;
    }
    return 0;
}
