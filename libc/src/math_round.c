/**
 * @file     math_round.c
 * @brief    POSIX.1-2024 <math.h> rounding, scaling, nextafter, and remainder
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <limits.h>
#include "math_internal.h"

/* need:: COMP_LIBC_0007 */

double floor(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    double r = 0.0;
    __asm__("vroundsd $1, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    if (!isfinite(x) || x == 0.0) {
        return x;
    }
    double t = trunc(x);
    return (t > x) ? (t - 1.0) : t;
#endif
}

float floorf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vroundss $1, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)floor((double)x);
#endif
}

long double floorl(long double x) {
    return (long double)floor((double)x);
}

double ceil(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    double r = 0.0;
    __asm__("vroundsd $2, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    if (!isfinite(x) || x == 0.0) {
        return x;
    }
    double t = trunc(x);
    return (t < x) ? (t + 1.0) : t;
#endif
}

float ceilf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vroundss $2, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)ceil((double)x);
#endif
}

long double ceill(long double x) {
    return (long double)ceil((double)x);
}

double trunc(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    double r = 0.0;
    __asm__("vroundsd $3, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    if (!isfinite(x) || fabs(x) >= 4503599627370496.0) {
        return x;
    }
    return copysign((double)(int64_t)x, x);
#endif
}

float truncf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vroundss $3, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)trunc((double)x);
#endif
}

long double truncl(long double x) {
    return (long double)trunc((double)x);
}

double rint(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    double r = 0.0;
    __asm__("vroundsd $4, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return nearbyint(x);
#endif
}

float rintf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vroundss $4, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)rint((double)x);
#endif
}

long double rintl(long double x) {
    return (long double)rint((double)x);
}

double nearbyint(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    /* Imm8 0x0C = current MXCSR rounding (0x04) | suppress FE_INEXACT (0x08).
     */
    double r = 0.0;
    __asm__("vroundsd $12, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    fenv_t env;
    (void)feholdexcept(&env);
    double r = rint(x);
    (void)fesetenv(&env);
    return r;
#endif
}

float nearbyintf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    float r = 0.0f;
    __asm__("vroundss $12, %1, %1, %0" : "=x"(r) : "x"(x));
    return r;
#else
    return (float)nearbyint((double)x);
#endif
}

long double nearbyintl(long double x) {
    return (long double)nearbyint((double)x);
}

double round(double x) {
    if (!isfinite(x) || x == 0.0) {
        return x;
    }
    double ax = fabs(x);
    if (ax >= 4503599627370496.0) {
        return x;
    }
    double fl = floor(ax);
    double r = (ax - fl >= 0.5) ? (fl + 1.0) : fl;
    return copysign(r, x);
}

float roundf(float x) {
    if (!isfinite(x) || x == 0.0f) {
        return x;
    }
    float ax = fabsf(x);
    if (ax >= 8388608.0f) {
        return x;
    }
    float fl = floorf(ax);
    float r = (ax - fl >= 0.5f) ? (fl + 1.0f) : fl;
    return copysignf(r, x);
}

long double roundl(long double x) {
    return (long double)round((double)x);
}

long lrint(double x) {
#if defined(__x86_64__) && defined(__AVX__)
    long r = 0;
    __asm__("vcvtsd2si %1, %0" : "=r"(r) : "x"(x));
    return r;
#else
    return (long)rint(x);
#endif
}

long lrintf(float x) {
#if defined(__x86_64__) && defined(__AVX__)
    long r = 0;
    __asm__("vcvtss2si %1, %0" : "=r"(r) : "x"(x));
    return r;
#else
    return (long)rintf(x);
#endif
}

long lrintl(long double x) {
    return lrint((double)x);
}

long long llrint(double x) {
    return (long long)lrint(x);
}

long long llrintf(float x) {
    return (long long)lrintf(x);
}

long long llrintl(long double x) {
    return (long long)lrint((double)x);
}

long lround(double x) {
    return (long)round(x);
}

long lroundf(float x) {
    return (long)roundf(x);
}

long lroundl(long double x) {
    return (long)round((double)x);
}

long long llround(double x) {
    return (long long)round(x);
}

long long llroundf(float x) {
    return (long long)roundf(x);
}

long long llroundl(long double x) {
    return (long long)round((double)x);
}

double modf(double x, double *iptr) {
    if (isnan(x)) {
        *iptr = x;
        return x;
    }
    if (isinf(x)) {
        *iptr = x;
        return copysign(0.0, x);
    }
    double t = trunc(x);
    *iptr = t;
    return copysign(x - t, x);
}

float modff(float x, float *iptr) {
    if (isnan(x)) {
        *iptr = x;
        return x;
    }
    if (isinf(x)) {
        *iptr = x;
        return copysignf(0.0f, x);
    }
    float t = truncf(x);
    *iptr = t;
    return copysignf(x - t, x);
}

long double modfl(long double x, long double *iptr) {
    double di = 0.0;
    double df = modf((double)x, &di);
    *iptr = (long double)di;
    return (long double)df;
}

double frexp(double x, int *exp_out) {
    uint64_t u = dbl_to_u64(x);
    uint64_t exp = (u >> 52) & 0x7FFULL;
    if (exp == 0x7FFULL || (u & 0x7FFFFFFFFFFFFFFFULL) == 0ULL) {
        *exp_out = 0;
        return x;
    }
    if (exp == 0ULL) {
        /* Subnormal: multiply by 2^64 to normalize. */
        x *= 18446744073709551616.0;
        u = dbl_to_u64(x);
        exp = (u >> 52) & 0x7FFULL;
        *exp_out = (int)exp - 1022 - 64;
    } else {
        *exp_out = (int)exp - 1022;
    }
    u = (u & 0x800FFFFFFFFFFFFFULL) | (0x3FEULL << 52);
    return u64_to_dbl(u);
}

float frexpf(float x, int *exp_out) {
    return (float)frexp((double)x, exp_out);
}

long double frexpl(long double x, int *exp_out) {
    return (long double)frexp((double)x, exp_out);
}

double scalbln(double x, long n) {
    if (!isfinite(x) || x == 0.0 || n == 0L) {
        return x;
    }
    if (n > 2098L) {
        return math_overflow_err(signbit(x));
    }
    if (n < -2098L) {
        return math_underflow_err(signbit(x));
    }
    while (n > 1023L) {
        x *= 0x1.0p1023;
        n -= 1023L;
        if (!isfinite(x)) {
            return math_overflow_err(signbit(x));
        }
    }
    while (n < -1022L) {
        x *= 0x1.0p-1022;
        n += 1022L;
        if (x == 0.0) {
            return math_underflow_err(signbit(x));
        }
    }
    double scale = u64_to_dbl((uint64_t)(n + 1023L) << 52);
    double r = x * scale;
    if (isinf(r)) {
        return math_overflow_err(signbit(x));
    }
    if (r == 0.0) {
        return math_underflow_err(signbit(x));
    }
    return r;
}

float scalblnf(float x, long n) {
    double r = scalbln((double)x, n);
    if (!isfinite((float)r) && isfinite(x)) {
        return (float)math_overflow_err(signbit(x));
    }
    return (float)r;
}

long double scalblnl(long double x, long n) {
    return (long double)scalbln((double)x, n);
}

double scalbn(double x, int n) {
    return scalbln(x, (long)n);
}

float scalbnf(float x, int n) {
    return scalblnf(x, (long)n);
}

long double scalbnl(long double x, int n) {
    return scalblnl(x, (long)n);
}

double ldexp(double x, int exp_val) {
    return scalbn(x, exp_val);
}

float ldexpf(float x, int exp_val) {
    return scalbnf(x, exp_val);
}

long double ldexpl(long double x, int exp_val) {
    return scalbnl(x, exp_val);
}

int ilogb(double x) {
    int cls = fpclassify(x);
    if (cls == FP_ZERO) {
        (void)feraiseexcept(FE_INVALID);
        errno = EDOM;
        return FP_ILOGB0;
    }
    if (cls == FP_NAN) {
        (void)feraiseexcept(FE_INVALID);
        errno = EDOM;
        return FP_ILOGBNAN;
    }
    if (cls == FP_INFINITE) {
        (void)feraiseexcept(FE_INVALID);
        errno = EDOM;
        return INT_MAX;
    }
    int e = 0;
    (void)frexp(x, &e);
    return e - 1;
}

int ilogbf(float x) {
    return ilogb((double)x);
}

int ilogbl(long double x) {
    return ilogb((double)x);
}

double logb(double x) {
    int cls = fpclassify(x);
    if (cls == FP_NAN) {
        return x;
    }
    if (cls == FP_INFINITE) {
        return (double)INFINITY;
    }
    if (cls == FP_ZERO) {
        return math_pole_err(1);
    }
    int e = 0;
    (void)frexp(x, &e);
    return (double)(e - 1);
}

float logbf(float x) {
    return (float)logb((double)x);
}

long double logbl(long double x) {
    return (long double)logb((double)x);
}

double nextafter(double x, double y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (x == y) {
        return y;
    }
    if (x == 0.0) {
        uint64_t tiny = (y < 0.0) ? (0x8000000000000001ULL) : 1ULL;
        (void)feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
        return u64_to_dbl(tiny);
    }
    uint64_t ux = dbl_to_u64(x);
    if ((x > 0.0 && y > x) || (x < 0.0 && y < x)) {
        ux++;
    } else {
        ux--;
    }
    double r = u64_to_dbl(ux);
    if (isinf(r)) {
        return math_overflow_err(signbit(r));
    }
    if (fpclassify(r) == FP_SUBNORMAL || r == 0.0) {
        (void)feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
    }
    return r;
}

float nextafterf(float x, float y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (x == y) {
        return y;
    }
    if (x == 0.0f) {
        uint32_t tiny = (y < 0.0f) ? 0x80000001u : 1u;
        (void)feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
        return u32_to_flt(tiny);
    }
    uint32_t ux = flt_to_u32(x);
    if ((x > 0.0f && y > x) || (x < 0.0f && y < x)) {
        ux++;
    } else {
        ux--;
    }
    float r = u32_to_flt(ux);
    if (isinf(r)) {
        return (float)math_overflow_err(signbit(r));
    }
    if (fpclassify(r) == FP_SUBNORMAL || r == 0.0f) {
        (void)feraiseexcept(FE_UNDERFLOW | FE_INEXACT);
    }
    return r;
}

long double nextafterl(long double x, long double y) {
    return (long double)nextafter((double)x, (double)y);
}

double nexttoward(double x, long double y) {
    return nextafter(x, (double)y);
}

float nexttowardf(float x, long double y) {
    return nextafterf(x, (float)y);
}

long double nexttowardl(long double x, long double y) {
    return nextafterl(x, y);
}

/* Exact binary long-division fmod(x, y) over normalized 53-bit significands. */
double fmod(double x, double y) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (isinf(x) || y == 0.0) {
        return math_domain_err();
    }
    if (x == 0.0 || isinf(y)) {
        return x;
    }
    uint64_t ux = dbl_to_u64(x);
    uint64_t sign = ux & 0x8000000000000000ULL;
    double ax = fabs(x);
    double ay = fabs(y);
    if (ax < ay) {
        return x;
    }
    if (ax == ay) {
        return u64_to_dbl(sign);
    }

    int ex = 0;
    int ey = 0;
    double mx = frexp(ax, &ex);
    double my = frexp(ay, &ey);
    uint64_t ix = (uint64_t)(mx * 9007199254740992.0); /* 2^53 * [0.5, 1) */
    uint64_t iy = (uint64_t)(my * 9007199254740992.0);

    int diff = ex - ey;
    while (diff > 0) {
        if (ix >= iy) {
            ix -= iy;
        }
        ix <<= 1;
        diff--;
    }
    if (ix >= iy) {
        ix -= iy;
    }
    if (ix == 0ULL) {
        return u64_to_dbl(sign);
    }
    double rem = ldexp((double)ix, ey - 53);
    return copysign(rem, x);
}

float fmodf(float x, float y) {
    return (float)fmod((double)x, (double)y);
}

long double fmodl(long double x, long double y) {
    return (long double)fmod((double)x, (double)y);
}

double remquo(double x, double y, int *quo) {
    if (quo) {
        *quo = 0;
    }
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (isinf(x) || y == 0.0) {
        return math_domain_err();
    }
    if (x == 0.0 || isinf(y)) {
        return x;
    }
    int qsign = (signbit(x) ^ signbit(y)) ? -1 : 1;
    double ax = fabs(x);
    double ay = fabs(y);

    int ex = 0;
    int ey = 0;
    double mx = frexp(ax, &ex);
    double my = frexp(ay, &ey);
    uint64_t ix = (uint64_t)(mx * 9007199254740992.0);
    uint64_t iy = (uint64_t)(my * 9007199254740992.0);
    uint32_t qlow = 0u;

    if (ex >= ey) {
        int diff = ex - ey;
        while (diff > 0) {
            qlow <<= 1;
            if (ix >= iy) {
                ix -= iy;
                qlow |= 1u;
            }
            ix <<= 1;
            diff--;
        }
        qlow <<= 1;
        if (ix >= iy) {
            ix -= iy;
            qlow |= 1u;
        }
    }

    double rem = (ix == 0ULL) ? 0.0 : ldexp((double)ix, ey - 53);
    double half_y = 0.5 * ay;
    if (rem > half_y || (rem == half_y && (qlow & 1u) != 0u)) {
        rem -= ay;
        qlow++;
    }
    if (quo) {
        int q7 = (int)(qlow & 0x7Fu);
        *quo = qsign * q7;
    }
    if (signbit(x)) {
        rem = -rem;
    }
    return rem;
}

float remquof(float x, float y, int *quo) {
    return (float)remquo((double)x, (double)y, quo);
}

long double remquol(long double x, long double y, int *quo) {
    return (long double)remquo((double)x, (double)y, quo);
}

double remainder(double x, double y) {
    int q = 0;
    return remquo(x, y, &q);
}

float remainderf(float x, float y) {
    int q = 0;
    return remquof(x, y, &q);
}

long double remainderl(long double x, long double y) {
    int q = 0;
    return remquol(x, y, &q);
}
