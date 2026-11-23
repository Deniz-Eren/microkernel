/**
 * @file     tgmath.h
 * @brief    ISO C17 and POSIX.1-2024 type-generic math and complex macros
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _TGMATH_H
#define _TGMATH_H

/* need:: ICD_LIBC_0010 */

#include <complex.h>
#include <math.h>

/* Promote integer arguments to double per ISO C17 7.25p3 (unevaluated). */
#define __TG_PROMOTE(x)                                                        \
    _Generic((x), float _Complex                                               \
             : (x), double _Complex                                            \
             : (x), long double _Complex                                       \
             : (x), float                                                      \
             : (x), double                                                     \
             : (x), long double                                                \
             : (x), default                                                    \
             : (double)(x))

#define __TG_R1(x, df, ff, lf)                                                 \
    _Generic(__TG_PROMOTE(x), long double : lf, float : ff, default : df)(x)

#define __TG_R1_EX(x, y, df, ff, lf)                                           \
    _Generic(__TG_PROMOTE(x), long double                                      \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)((x), (y))

#define __TG_R2(x, y, df, ff, lf)                                              \
    _Generic(__TG_PROMOTE(x) + __TG_PROMOTE(y), long double                    \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)((x), (y))

#define __TG_R2_EX(x, y, z, df, ff, lf)                                        \
    _Generic(__TG_PROMOTE(x) + __TG_PROMOTE(y), long double                    \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)((x), (y), (z))

#define __TG_R3(x, y, z, df, ff, lf)                                           \
    _Generic(__TG_PROMOTE(x) + __TG_PROMOTE(y) + __TG_PROMOTE(z), long double  \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)((x), (y), (z))

#define __TG_RC1(x, df, ff, lf, dc, fc, lc)                                    \
    _Generic(__TG_PROMOTE(x), long double _Complex                             \
             : lc, double _Complex                                             \
             : dc, float _Complex                                              \
             : fc, long double                                                 \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)(x)

#define __TG_RC2(x, y, df, ff, lf, dc, fc, lc)                                 \
    _Generic(__TG_PROMOTE(x) + __TG_PROMOTE(y), long double _Complex           \
             : lc, double _Complex                                             \
             : dc, float _Complex                                              \
             : fc, long double                                                 \
             : lf, float                                                       \
             : ff, default                                                     \
             : df)((x), (y))

#define __TG_C1(x, dc, fc, lc)                                                 \
    _Generic(__TG_PROMOTE(x), long double _Complex                             \
             : lc, long double                                                 \
             : lc, float _Complex                                              \
             : fc, float                                                       \
             : fc, default                                                     \
             : dc)(x)

/* Real and complex type-generic functions. */
#define acos(x) __TG_RC1((x), acos, acosf, acosl, cacos, cacosf, cacosl)
#define asin(x) __TG_RC1((x), asin, asinf, asinl, casin, casinf, casinl)
#define atan(x) __TG_RC1((x), atan, atanf, atanl, catan, catanf, catanl)
#define acosh(x) __TG_RC1((x), acosh, acoshf, acoshl, cacosh, cacoshf, cacoshl)
#define asinh(x) __TG_RC1((x), asinh, asinhf, asinhl, casinh, casinhf, casinhl)
#define atanh(x) __TG_RC1((x), atanh, atanhf, atanhl, catanh, catanhf, catanhl)
#define cos(x) __TG_RC1((x), cos, cosf, cosl, ccos, ccosf, ccosl)
#define sin(x) __TG_RC1((x), sin, sinf, sinl, csin, csinf, csinl)
#define tan(x) __TG_RC1((x), tan, tanf, tanl, ctan, ctanf, ctanl)
#define cosh(x) __TG_RC1((x), cosh, coshf, coshl, ccosh, ccoshf, ccoshl)
#define sinh(x) __TG_RC1((x), sinh, sinhf, sinhl, csinh, csinhf, csinhl)
#define tanh(x) __TG_RC1((x), tanh, tanhf, tanhl, ctanh, ctanhf, ctanhl)
#define exp(x) __TG_RC1((x), exp, expf, expl, cexp, cexpf, cexpl)
#define log(x) __TG_RC1((x), log, logf, logl, clog, clogf, clogl)
#define pow(x, y) __TG_RC2((x), (y), pow, powf, powl, cpow, cpowf, cpowl)
#define sqrt(x) __TG_RC1((x), sqrt, sqrtf, sqrtl, csqrt, csqrtf, csqrtl)
#define fabs(x) __TG_RC1((x), fabs, fabsf, fabsl, cabs, cabsf, cabsl)

/* Complex-only type-generic functions. */
#define carg(x) __TG_C1((x), carg, cargf, cargl)
#define cimag(x) __TG_C1((x), cimag, cimagf, cimagl)
#define conj(x) __TG_C1((x), conj, conjf, conjl)
#define cproj(x) __TG_C1((x), cproj, cprojf, cprojl)
#define creal(x) __TG_C1((x), creal, crealf, creall)

/* Real-only type-generic functions. */
#define atan2(y, x) __TG_R2((y), (x), atan2, atan2f, atan2l)
#define cbrt(x) __TG_R1((x), cbrt, cbrtf, cbrtl)
#define ceil(x) __TG_R1((x), ceil, ceilf, ceill)
#define copysign(x, y) __TG_R2((x), (y), copysign, copysignf, copysignl)
#define erf(x) __TG_R1((x), erf, erff, erfl)
#define erfc(x) __TG_R1((x), erfc, erfcf, erfcl)
#define exp2(x) __TG_R1((x), exp2, exp2f, exp2l)
#define expm1(x) __TG_R1((x), expm1, expm1f, expm1l)
#define fdim(x, y) __TG_R2((x), (y), fdim, fdimf, fdiml)
#define floor(x) __TG_R1((x), floor, floorf, floorl)
#define fma(x, y, z) __TG_R3((x), (y), (z), fma, fmaf, fmal)
#define fmax(x, y) __TG_R2((x), (y), fmax, fmaxf, fmaxl)
#define fmin(x, y) __TG_R2((x), (y), fmin, fminf, fminl)
#define fmod(x, y) __TG_R2((x), (y), fmod, fmodf, fmodl)
#define frexp(x, e) __TG_R1_EX((x), (e), frexp, frexpf, frexpl)
#define hypot(x, y) __TG_R2((x), (y), hypot, hypotf, hypotl)
#define ilogb(x) __TG_R1((x), ilogb, ilogbf, ilogbl)
#define ldexp(x, e) __TG_R1_EX((x), (e), ldexp, ldexpf, ldexpl)
#define llrint(x) __TG_R1((x), llrint, llrintf, llrintl)
#define llround(x) __TG_R1((x), llround, llroundf, llroundl)
#define log10(x) __TG_R1((x), log10, log10f, log10l)
#define log1p(x) __TG_R1((x), log1p, log1pf, log1pl)
#define log2(x) __TG_R1((x), log2, log2f, log2l)
#define logb(x) __TG_R1((x), logb, logbf, logbl)
#define lrint(x) __TG_R1((x), lrint, lrintf, lrintl)
#define lround(x) __TG_R1((x), lround, lroundf, lroundl)
#define nearbyint(x) __TG_R1((x), nearbyint, nearbyintf, nearbyintl)
#define nextafter(x, y) __TG_R2((x), (y), nextafter, nextafterf, nextafterl)
#define nexttoward(x, y)                                                       \
    __TG_R1_EX((x), (y), nexttoward, nexttowardf, nexttowardl)
#define remainder(x, y) __TG_R2((x), (y), remainder, remainderf, remainderl)
#define remquo(x, y, q) __TG_R2_EX((x), (y), (q), remquo, remquof, remquol)
#define rint(x) __TG_R1((x), rint, rintf, rintl)
#define round(x) __TG_R1((x), round, roundf, roundl)
#define scalbn(x, n) __TG_R1_EX((x), (n), scalbn, scalbnf, scalbnl)
#define scalbln(x, n) __TG_R1_EX((x), (n), scalbln, scalblnf, scalblnl)
#define tgamma(x) __TG_R1((x), tgamma, tgammaf, tgammal)
#define trunc(x) __TG_R1((x), trunc, truncf, truncl)

#endif /* _TGMATH_H */
