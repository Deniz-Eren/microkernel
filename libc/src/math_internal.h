/**
 * @file     math_internal.h
 * @brief    Internal IEEE-754 bit helpers and constants for libc math modules
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _LIBC_MATH_INTERNAL_H
#define _LIBC_MATH_INTERNAL_H

#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>

/* IEEE-754 bit punning helpers (strict-aliasing safe via unions). */
static inline uint64_t dbl_to_u64(double x) {
    union {
        double d;
        uint64_t u;
    } v = {.d = x};
    return v.u;
}

static inline double u64_to_dbl(uint64_t u) {
    union {
        uint64_t u;
        double d;
    } v = {.u = u};
    return v.d;
}

static inline uint32_t flt_to_u32(float x) {
    union {
        float f;
        uint32_t u;
    } v = {.f = x};
    return v.u;
}

static inline float u32_to_flt(uint32_t u) {
    union {
        uint32_t u;
        float f;
    } v = {.u = u};
    return v.f;
}

static inline double math_domain_err(void) {
    errno = EDOM;
    (void)feraiseexcept(FE_INVALID);
    return (double)NAN;
}

static inline double math_pole_err(int neg) {
    errno = ERANGE;
    (void)feraiseexcept(FE_DIVBYZERO);
    return neg ? -HUGE_VAL : HUGE_VAL;
}

static inline double math_overflow_err(int neg) {
    errno = ERANGE;
    (void)feraiseexcept(FE_OVERFLOW);
    return neg ? -HUGE_VAL : HUGE_VAL;
}

static inline double math_underflow_err(int neg) {
    errno = ERANGE;
    (void)feraiseexcept(FE_UNDERFLOW);
    return neg ? -0.0 : 0.0;
}

/* Shared high-precision constants. */
#define K_PI 3.14159265358979323846264338327950288
#define K_PI_2 1.57079632679489661923132169163975144
#define K_PI_4 0.78539816339744830961566084581987572
#define K_LN2 0.69314718055994530941723212145817657
#define K_LN2_HI 0.69314718036912381649017333984375 /* exact 32-bit head */
#define K_LN2_LO 1.90821492927058770002e-10
#define K_INV_LN2 1.44269504088896340735992468100189214
#define K_INV_LN10 0.43429448190325182765112891891660508
#define K_SQRT2 1.41421356237309504880168872420969808
#define K_SQRT1_2 0.70710678118654752440084436210484904

#endif /* _LIBC_MATH_INTERNAL_H */
