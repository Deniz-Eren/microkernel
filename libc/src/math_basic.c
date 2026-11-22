/**
 * @file     math_basic.c
 * @brief    POSIX.1-2024 <math.h> classification, fabs, fma, sqrt, cbrt, hypot
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "math_internal.h"

/* need:: COMP_LIBC_0007 */

int __fpclassifyf(float x) {
    uint32_t u = flt_to_u32(x);
    uint32_t exp = (u >> 23) & 0xFFu;
    uint32_t frac = u & 0x7FFFFFu;
    if (exp == 0u) {
        return frac == 0u ? FP_ZERO : FP_SUBNORMAL;
    }
    if (exp == 0xFFu) {
        return frac == 0u ? FP_INFINITE : FP_NAN;
    }
    return FP_NORMAL;
}

int __fpclassifyd(double x) {
    uint64_t u = dbl_to_u64(x);
    uint64_t exp = (u >> 52) & 0x7FFULL;
    uint64_t frac = u & 0x000FFFFFFFFFFFFFULL;
    if (exp == 0ULL) {
        return frac == 0ULL ? FP_ZERO : FP_SUBNORMAL;
    }
    if (exp == 0x7FFULL) {
        return frac == 0ULL ? FP_INFINITE : FP_NAN;
    }
    return FP_NORMAL;
}

int __fpclassifyl(long double x) {
#if defined(__x86_64__)
    union {
        long double ld;
        struct {
            uint64_t mant;
            uint16_t se;
        } parts;
    } v = {.ld = x};
    uint16_t exp = (uint16_t)(v.parts.se & 0x7FFFu);
    uint64_t frac = v.parts.mant & 0x7FFFFFFFFFFFFFFFULL;
    if (exp == 0u) {
        return v.parts.mant == 0ULL ? FP_ZERO : FP_SUBNORMAL;
    }
    if (exp == 0x7FFFu) {
        return frac == 0ULL ? FP_INFINITE : FP_NAN;
    }
    return FP_NORMAL;
#else
    return __fpclassifyd((double)x);
#endif
}

int __signbitf(float x) {
    return (int)(flt_to_u32(x) >> 31);
}

int __signbitd(double x) {
    return (int)(dbl_to_u64(x) >> 63);
}

int __signbitl(long double x) {
#if defined(__x86_64__)
    union {
        long double ld;
        struct {
            uint64_t mant;
            uint16_t se;
        } parts;
    } v = {.ld = x};
    return (int)(v.parts.se >> 15);
#else
    return __signbitd((double)x);
#endif
}

double fabs(double x) {
    return u64_to_dbl(dbl_to_u64(x) & 0x7FFFFFFFFFFFFFFFULL);
}

float fabsf(float x) {
    return u32_to_flt(flt_to_u32(x) & 0x7FFFFFFFu);
}

long double fabsl(long double x) {
#if defined(__x86_64__)
    union {
        long double ld;
        struct {
            uint64_t mant;
            uint16_t se;
        } parts;
    } v = {.ld = x};
    v.parts.se &= 0x7FFFu;
    return v.ld;
#else
    return (long double)fabs((double)x);
#endif
}

double copysign(double x, double y) {
    uint64_t ux = dbl_to_u64(x) & 0x7FFFFFFFFFFFFFFFULL;
    uint64_t uy = dbl_to_u64(y) & 0x8000000000000000ULL;
    return u64_to_dbl(ux | uy);
}

float copysignf(float x, float y) {
    uint32_t ux = flt_to_u32(x) & 0x7FFFFFFFu;
    uint32_t uy = flt_to_u32(y) & 0x80000000u;
    return u32_to_flt(ux | uy);
}

long double copysignl(long double x, long double y) {
#if defined(__x86_64__)
    union {
        long double ld;
        struct {
            uint64_t mant;
            uint16_t se;
        } parts;
    } vx = {.ld = x}, vy = {.ld = y};
    vx.parts.se = (uint16_t)((vx.parts.se & 0x7FFFu) | (vy.parts.se & 0x8000u));
    return vx.ld;
#else
    return (long double)copysign((double)x, (double)y);
#endif
}

double nan(const char *tagp) {
    (void)tagp;
    return __builtin_nan("");
}

float nanf(const char *tagp) {
    (void)tagp;
    return __builtin_nanf("");
}

long double nanl(const char *tagp) {
    (void)tagp;
    return __builtin_nanl("");
}

double fmax(double x, double y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0 && y == 0.0) {
        return signbit(x) ? y : x;
    }
    return x > y ? x : y;
}

float fmaxf(float x, float y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0f && y == 0.0f) {
        return signbit(x) ? y : x;
    }
    return x > y ? x : y;
}

long double fmaxl(long double x, long double y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0L && y == 0.0L) {
        return signbit(x) ? y : x;
    }
    return x > y ? x : y;
}

double fmin(double x, double y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0 && y == 0.0) {
        return signbit(x) ? x : y;
    }
    return x < y ? x : y;
}

float fminf(float x, float y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0f && y == 0.0f) {
        return signbit(x) ? x : y;
    }
    return x < y ? x : y;
}

long double fminl(long double x, long double y) {
    if (isnan(x)) {
        return y;
    }
    if (isnan(y)) {
        return x;
    }
    if (x == 0.0L && y == 0.0L) {
        return signbit(x) ? x : y;
    }
    return x < y ? x : y;
}

double fdim(double x, double y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (x <= y) {
        return 0.0;
    }
    double r = x - y;
    if (isinf(r) && isfinite(x) && isfinite(y)) {
        return math_overflow_err(0);
    }
    return r;
}

float fdimf(float x, float y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    return x > y ? (x - y) : 0.0f;
}

long double fdiml(long double x, long double y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    return x > y ? (x - y) : 0.0L;
}

double fma(double x, double y, double z) {
#if defined(__x86_64__) && defined(__FMA__)
    __asm__("vfmadd213sd %2, %1, %0" : "+x"(x) : "x"(y), "xm"(z));
    return x;
#else
    return (x * y) + z;
#endif
}

float fmaf(float x, float y, float z) {
#if defined(__x86_64__) && defined(__FMA__)
    __asm__("vfmadd213ss %2, %1, %0" : "+x"(x) : "x"(y), "xm"(z));
    return x;
#else
    return (x * y) + z;
#endif
}

long double fmal(long double x, long double y, long double z) {
    return (long double)fma((double)x, (double)y, (double)z);
}

double sqrt(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0) {
        return math_domain_err();
    }
#if defined(__x86_64__) && defined(__AVX__)
    double r = 0.0;
    __asm__("vsqrtsd %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    if (x == 0.0 || isinf(x)) {
        return x;
    }
    int e = 0;
    double m = frexp(x, &e);
    if (e & 1) {
        m *= 2.0;
        e -= 1;
    }
    double g = 0.5 * (1.0 + m);
    for (int i = 0; i < 6; i++) {
        g = 0.5 * (g + m / g);
    }
    return ldexp(g, e / 2);
#endif
}

float sqrtf(float x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0f) {
        return (float)math_domain_err();
    }
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vsqrtss %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)sqrt((double)x);
#endif
}

long double sqrtl(long double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0L) {
        return (long double)math_domain_err();
    }
#if defined(__x86_64__)
    __asm__("fsqrt" : "+t"(x));
    return x;
#else
    return (long double)sqrt((double)x);
#endif
}

double cbrt(double x) {
    if (isnan(x) || isinf(x) || x == 0.0) {
        return x;
    }
    double ax = fabs(x);
    int e = 0;
    double m = frexp(ax, &e);
    int q = e / 3;
    int r = e - 3 * q;
    if (r < 0) {
        r += 3;
        q -= 1;
    }
    if (r == 1) {
        m *= 2.0;
    } else if (r == 2) {
        m *= 4.0;
    }
    /* Now m in [0.5, 4.0). Quadratic initial guess + 4 Halley steps. */
    double y = fma(0.3266, m, 0.525);
    for (int i = 0; i < 4; i++) {
        double y3 = y * y * y;
        y = y * (y3 + 2.0 * m) / (2.0 * y3 + m);
    }
    /* One final Newton step in FMA form for < 1 ULP accuracy. */
    double y2 = y * y;
    y = y - (fma(y, y2, -m)) / (3.0 * y2);
    return copysign(ldexp(y, q), x);
}

float cbrtf(float x) {
    return (float)cbrt((double)x);
}

long double cbrtl(long double x) {
    return (long double)cbrt((double)x);
}

double hypot(double x, double y) {
    if (isinf(x) || isinf(y)) {
        return (double)INFINITY;
    }
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    double ax = fabs(x);
    double ay = fabs(y);
    if (ax < ay) {
        double t = ax;
        ax = ay;
        ay = t;
    }
    if (ax == 0.0) {
        return 0.0;
    }
    double r = ay / ax;
    double res = ax * sqrt(fma(r, r, 1.0));
    if (isinf(res)) {
        return math_overflow_err(0);
    }
    return res;
}

float hypotf(float x, float y) {
    if (isinf(x) || isinf(y)) {
        return INFINITY;
    }
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    return (float)hypot((double)x, (double)y);
}

long double hypotl(long double x, long double y) {
    if (isinf(x) || isinf(y)) {
        return (long double)INFINITY;
    }
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    return (long double)hypot((double)x, (double)y);
}
