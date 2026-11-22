/**
 * @file     test_math.c
 * @brief    Ring 3 POSIX.1-2024 <fenv.h> and <math.h> verification suite
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0009 */

#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int approx(double got, double want, double tol) {
    if (isnan(got) || isnan(want)) {
        return 0;
    }
    double diff = fabs(got - want);
    if (diff <= tol) {
        return 1;
    }
    return diff <= tol * fmax(1.0, fabs(want));
}

static int test_fenv_and_rounding(void) {
    if (fesetenv(FE_DFL_ENV) != 0 || fegetround() != FE_TONEAREST) {
        printf("[FAIL] mathtest: default fenv/round failed\n");
        return -1;
    }
    if (fesetround(0x1234) == 0) {
        printf("[FAIL] mathtest: invalid fesetround accepted\n");
        return -1;
    }

    /* Verify MXCSR rounding modes via rint()/nearbyint()/lrint(). */
    (void)feclearexcept(FE_ALL_EXCEPT);
    if (fesetround(FE_UPWARD) != 0 || fegetround() != FE_UPWARD ||
        rint(2.1) != 3.0 || rint(-2.1) != -2.0 || lrint(2.1) != 3L) {
        printf("[FAIL] mathtest: FE_UPWARD rounding failed\n");
        return -1;
    }
    if (fetestexcept(FE_INEXACT) == 0) {
        printf("[FAIL] mathtest: rint did not raise FE_INEXACT\n");
        return -1;
    }
    (void)feclearexcept(FE_ALL_EXCEPT);
    if (nearbyint(2.1) != 3.0 || fetestexcept(FE_INEXACT) != 0) {
        printf("[FAIL] mathtest: nearbyint raised FE_INEXACT\n");
        return -1;
    }

    if (fesetround(FE_DOWNWARD) != 0 || rint(2.9) != 2.0 ||
        rint(-2.1) != -3.0) {
        printf("[FAIL] mathtest: FE_DOWNWARD rounding failed\n");
        return -1;
    }
    if (fesetround(FE_TOWARDZERO) != 0 || rint(2.9) != 2.0 ||
        rint(-2.9) != -2.0) {
        printf("[FAIL] mathtest: FE_TOWARDZERO rounding failed\n");
        return -1;
    }

    /* Verify feholdexcept, feraiseexcept, fegetexceptflag, feupdateenv. */
    fenv_t saved;
    (void)fesetround(FE_TONEAREST);
    (void)feclearexcept(FE_ALL_EXCEPT);
    (void)feraiseexcept(FE_OVERFLOW);
    fexcept_t flg = 0u;
    (void)fegetexceptflag(&flg, FE_ALL_EXCEPT);
    if ((flg & FE_OVERFLOW) == 0u) {
        printf("[FAIL] mathtest: fegetexceptflag missing FE_OVERFLOW\n");
        return -1;
    }
    (void)feholdexcept(&saved);
    if (fetestexcept(FE_ALL_EXCEPT) != 0) {
        printf("[FAIL] mathtest: feholdexcept did not clear flags\n");
        return -1;
    }
    (void)feraiseexcept(FE_UNDERFLOW);
    (void)feupdateenv(&saved);
    if (fetestexcept(FE_OVERFLOW | FE_UNDERFLOW) !=
        (FE_OVERFLOW | FE_UNDERFLOW)) {
        printf("[FAIL] mathtest: feupdateenv did not merge exceptions\n");
        return -1;
    }
    (void)fesetexceptflag(&flg, FE_ALL_EXCEPT);
    if (fetestexcept(FE_ALL_EXCEPT) != FE_OVERFLOW) {
        printf("[FAIL] mathtest: fesetexceptflag failed\n");
        return -1;
    }
    (void)fesetenv(FE_DFL_ENV);

    /* floor, ceil, trunc, round, lround, llround, modf, frexp, ldexp, fmod. */
    if (floor(2.7) != 2.0 || floor(-2.1) != -3.0 || floorf(-2.1f) != -3.0f ||
        ceil(2.1) != 3.0 || ceil(-2.7) != -2.0 || ceilf(2.1f) != 3.0f ||
        trunc(2.9) != 2.0 || trunc(-2.9) != -2.0 || round(2.5) != 3.0 ||
        round(-2.5) != -3.0 || lround(-2.5) != -3L || llround(2.5) != 3LL ||
        llrint(4.0) != 4LL) {
        printf("[FAIL] mathtest: nearest-integer functions failed\n");
        return -1;
    }

    double ip = 0.0;
    double fp = modf(-3.75, &ip);
    int ex = 0;
    double fr = frexp(12.0, &ex);
    int q = 0;
    double rq = remquo(10.0, 3.0, &q);
    if (ip != -3.0 || fp != -0.75 || fr != 0.75 || ex != 4 ||
        ldexp(0.75, 4) != 12.0 || scalbn(1.5, 3) != 12.0 ||
        scalbln(1.5, 3L) != 12.0 || ilogb(8.0) != 3 || logb(8.0) != 3.0 ||
        !approx(fmod(5.5, 2.0), 1.5, 1e-15) ||
        !approx(remainder(5.5, 2.0), -0.5, 1e-15) || !approx(rq, 1.0, 1e-15) ||
        q != 3 || nextafter(1.0, 2.0) <= 1.0 ||
        nextafterf(1.0f, 0.0f) >= 1.0f || nexttoward(1.0, 2.0L) <= 1.0) {
        printf("[FAIL] mathtest: modf/frexp/ldexp/fmod/nextafter failed\n");
        return -1;
    }

    printf("[OK] <fenv.h> MXCSR rounding, exceptions, and math rounding"
           " verified\n");
    return 0;
}

static int test_basic_fma_and_classification(void) {
    /* Classification & comparison macros across float, double, long double. */
    if (fpclassify(0.0) != FP_ZERO || fpclassify(1.0f) != FP_NORMAL ||
        fpclassify((double)INFINITY) != FP_INFINITE ||
        fpclassify((double)NAN) != FP_NAN ||
        fpclassify(1.0e-310) != FP_SUBNORMAL || !isfinite(1.0) ||
        !isinf(-(double)INFINITY) || !isnan((double)NAN) || !isnormal(2.0L) ||
        signbit(-0.0) == 0 || signbit(0.0) != 0 || signbit(-0.0f) == 0 ||
        signbit(-0.0L) == 0 || !isgreater(2.0, 1.0) ||
        !isgreaterequal(2.0, 2.0) || !isless(1.0, 2.0) ||
        !islessequal(1.0, 1.0) || !islessgreater(1.0, 2.0) ||
        !isunordered((double)NAN, 1.0)) {
        printf("[FAIL] mathtest: classification/comparison macros failed\n");
        return -1;
    }

    /* Exact hardware FMA (double & float), sqrt, cbrt, hypot, fmax/fmin/fdim.
     */
    double fma_exact = fma(1.0 + 0x1.0p-52, 1.0 - 0x1.0p-52, -1.0);
    float fmaf_exact = fmaf(1.0f + 0x1.0p-23f, 1.0f - 0x1.0p-23f, -1.0f);
    if (fma_exact != -0x1.0p-104 || fmaf_exact != -0x1.0p-46f ||
        !approx(sqrt(2.0), 1.4142135623730951, 1e-15) ||
        !approx((double)sqrtf(9.0f), 3.0, 1e-6) ||
        !approx((double)sqrtl(16.0L), 4.0, 1e-12) ||
        !approx(cbrt(27.0), 3.0, 1e-14) || !approx(cbrt(-8.0), -2.0, 1e-14) ||
        !approx(hypot(3.0, 4.0), 5.0, 1e-15) ||
        !approx(hypot(1e200, 1e200), 1.4142135623730951e200, 1e-14) ||
        !isinf(hypot((double)INFINITY, (double)NAN)) ||
        fmax(1.0, (double)NAN) != 1.0 || fmin(1.0, (double)NAN) != 1.0 ||
        fdim(5.0, 2.0) != 3.0 || fdim(2.0, 5.0) != 0.0 ||
        copysign(3.5, -1.0) != -3.5 || !isnan(nan(""))) {
        printf("[FAIL] mathtest: fma/sqrt/cbrt/hypot/fmax/fmin failed\n");
        return -1;
    }

    /* Domain error check (EDOM). */
    errno = 0;
    double bad = sqrt(-1.0);
    if (!isnan(bad) || errno != EDOM) {
        printf("[FAIL] mathtest: sqrt(-1.0) did not set EDOM\n");
        return -1;
    }

    printf("[OK] <math.h> classification, hardware FMA, sqrt, cbrt, and hypot"
           " verified\n");
    return 0;
}

static int test_exp_log_pow_and_special(void) {
    if (!approx(exp(0.0), 1.0, 1e-15) ||
        !approx(exp(1.0), 2.71828182845904523536, 1e-14) ||
        !approx(exp(-1.0), 0.36787944117144232160, 1e-14) ||
        !approx(exp2(10.0), 1024.0, 1e-13) ||
        !approx(expm1(1.0e-12), 1.0e-12, 1e-14) ||
        !approx(log(2.71828182845904523536), 1.0, 1e-14) ||
        !approx(log2(1024.0), 10.0, 1e-14) ||
        !approx(log10(1000.0), 3.0, 1e-14) ||
        !approx(log1p(1.0e-12), 1.0e-12, 1e-14) ||
        !approx(pow(2.0, 10.0), 1024.0, 1e-14) ||
        !approx(pow(-2.0, 3.0), -8.0, 1e-14) ||
        !approx(pow(9.0, 0.5), 3.0, 1e-14) ||
        !approx((double)powf(3.0f, 4.0f), 81.0, 1e-6) ||
        !approx(erf(1.0), 0.84270079294971486934, 1e-14) ||
        !approx(erfc(3.0), 0.00002209049699858544, 1e-12) ||
        !approx(tgamma(5.0), 24.0, 1e-14) ||
        !approx(tgamma(0.5), 1.77245385090551602730, 1e-13) ||
        !approx(tgamma(-0.5), -3.54490770181103205460, 1e-13)) {
        printf("[FAIL] mathtest: exp/log/pow/erf/tgamma accuracy failed\n");
        return -1;
    }

    /* Pole and domain error checks. */
    errno = 0;
    double l0 = log(0.0);
    if (!isinf(l0) || l0 > 0.0 || errno != ERANGE) {
        printf("[FAIL] mathtest: log(0.0) pole check failed\n");
        return -1;
    }
    errno = 0;
    double pbad = pow(-2.0, 0.5);
    if (!isnan(pbad) || errno != EDOM) {
        printf("[FAIL] mathtest: pow(-2.0, 0.5) domain check failed\n");
        return -1;
    }

    printf("[OK] <math.h> exp, log, pow, erf/erfc, and tgamma verified\n");
    return 0;
}

static int test_trig_and_hyperbolic(void) {
    const double pi = 3.14159265358979323846;
    if (!approx(sin(0.0), 0.0, 1e-15) || !approx(sin(pi / 6.0), 0.5, 1e-14) ||
        !approx(sin(pi / 2.0), 1.0, 1e-14) || !approx(cos(0.0), 1.0, 1e-15) ||
        !approx(cos(pi / 3.0), 0.5, 1e-14) || !approx(cos(pi), -1.0, 1e-14) ||
        !approx(tan(pi / 4.0), 1.0, 1e-14) ||
        !approx((double)sinf(0.5f), sin(0.5), 1e-6) ||
        !approx((double)cosf(0.5f), cos(0.5), 1e-6) ||
        !approx((double)tanf(0.5f), tan(0.5), 1e-6) ||
        !approx(asin(0.5), pi / 6.0, 1e-14) ||
        !approx(asin(0.99), 1.4292568534704692, 1e-13) ||
        !approx(acos(0.5), pi / 3.0, 1e-14) ||
        !approx(acos(-0.5), 2.0 * pi / 3.0, 1e-14) ||
        !approx((double)acosf(0.5f), (double)(pi / 3.0), 1e-6) ||
        !approx(atan(1.0), pi / 4.0, 1e-14) ||
        !approx(atan(10.0), 1.4711276743037347, 1e-14) ||
        !approx(atan2(1.0, 1.0), pi / 4.0, 1e-14) ||
        !approx(atan2(1.0, -1.0), 3.0 * pi / 4.0, 1e-14) ||
        !approx(atan2(-1.0, -1.0), -3.0 * pi / 4.0, 1e-14)) {
        printf("[FAIL] mathtest: trig/inverse-trig accuracy failed\n");
        return -1;
    }

    if (!approx(sinh(1.0), 1.17520119364380145688, 1e-14) ||
        !approx(cosh(1.0), 1.54308063481524377848, 1e-14) ||
        !approx(tanh(1.0), 0.76159415595576488812, 1e-14) ||
        !approx(asinh(sinh(1.2)), 1.2, 1e-13) ||
        !approx(acosh(cosh(1.5)), 1.5, 1e-13) ||
        !approx(atanh(tanh(0.7)), 0.7, 1e-13)) {
        printf("[FAIL] mathtest: hyperbolic functions accuracy failed\n");
        return -1;
    }

    errno = 0;
    if (!isnan(acos(2.0)) || errno != EDOM) {
        printf("[FAIL] mathtest: acos(2.0) EDOM failed\n");
        return -1;
    }

    printf("[OK] <math.h> trig, inverse trig (acos/asin/atan2), and hyperbolic"
           " verified\n");
    return 0;
}

int main(void) {
    if (test_fenv_and_rounding() != 0) {
        return 1;
    }
    if (test_basic_fma_and_classification() != 0) {
        return 1;
    }
    if (test_exp_log_pow_and_special() != 0) {
        return 1;
    }
    if (test_trig_and_hyperbolic() != 0) {
        return 1;
    }
    return 0;
}
