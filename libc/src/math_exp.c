/**
 * @file     math_exp.c
 * @brief    POSIX.1-2024 <math.h> exp, log, pow, erf, erfc, and tgamma
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "math_internal.h"

/* need:: COMP_LIBC_0007 */

/* Evaluate exp(r) - 1 for |r| <= ln(2)/2 using FMA Horner (degree 13). */
static double expm1_core(double r) {
    static const double c[12] = {
        1.0 / 6227020800.0, /* 1/13! */
        1.0 / 479001600.0,  /* 1/12! */
        1.0 / 39916800.0,   /* 1/11! */
        1.0 / 3628800.0,    /* 1/10! */
        1.0 / 362880.0,     /* 1/9!  */
        1.0 / 40320.0,      /* 1/8!  */
        1.0 / 5040.0,       /* 1/7!  */
        1.0 / 720.0,        /* 1/6!  */
        1.0 / 120.0,        /* 1/5!  */
        1.0 / 24.0,         /* 1/4!  */
        1.0 / 6.0,          /* 1/3!  */
        1.0 / 2.0,          /* 1/2!  */
    };
    double p = c[0];
    for (int i = 1; i < 12; i++) {
        p = fma(p, r, c[i]);
    }
    return fma(p, r * r, r);
}

double exp(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x == 0.0) {
        return 1.0;
    }
    if (x > 709.782712893384) {
        if (isinf(x)) {
            return x;
        }
        return math_overflow_err(0);
    }
    if (x < -745.1332191019412) {
        if (isinf(x)) {
            return 0.0;
        }
        return math_underflow_err(0);
    }
    double kf = round(x * K_INV_LN2);
    int k = (int)kf;
    double r = fma(-kf, K_LN2_HI, x);
    r = fma(-kf, K_LN2_LO, r);
    double res = 1.0 + expm1_core(r);
    return ldexp(res, k);
}

float expf(float x) {
    if (isnan(x)) {
        return x;
    }
    if (x > 88.722839f) {
        return isinf(x) ? x : (float)math_overflow_err(0);
    }
    if (x < -103.97208f) {
        return isinf(x) ? 0.0f : (float)math_underflow_err(0);
    }
    return (float)exp((double)x);
}

long double expl(long double x) {
    return (long double)exp((double)x);
}

double exp2(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x == 0.0) {
        return 1.0;
    }
    if (x >= 1024.0) {
        return isinf(x) ? x : math_overflow_err(0);
    }
    if (x < -1075.0) {
        return isinf(x) ? 0.0 : math_underflow_err(0);
    }
    double kf = round(x);
    int k = (int)kf;
    double f = x - kf;
    double r = f * K_LN2_HI;
    r = fma(f, K_LN2_LO, r);
    return ldexp(1.0 + expm1_core(r), k);
}

float exp2f(float x) {
    if (isnan(x)) {
        return x;
    }
    if (x >= 128.0f) {
        return isinf(x) ? x : (float)math_overflow_err(0);
    }
    if (x < -150.0f) {
        return isinf(x) ? 0.0f : (float)math_underflow_err(0);
    }
    return (float)exp2((double)x);
}

long double exp2l(long double x) {
    return (long double)exp2((double)x);
}

double expm1(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    if (isinf(x)) {
        return x > 0.0 ? x : -1.0;
    }
    if (fabs(x) <= 0.34657359027997264) {
        return expm1_core(x);
    }
    if (x < -56.0) {
        return -1.0;
    }
    return exp(x) - 1.0;
}

float expm1f(float x) {
    return (float)expm1((double)x);
}

long double expm1l(long double x) {
    return (long double)expm1((double)x);
}

/* Evaluate ln((1 + s)/(1 - s)) = 2*s * (1 + s^2/3 + s^4/5 + ...). */
static double log_atanh_series(double s) {
    double s2 = s * s;
    double p = 1.0 / 21.0;
    p = fma(p, s2, 1.0 / 19.0);
    p = fma(p, s2, 1.0 / 17.0);
    p = fma(p, s2, 1.0 / 15.0);
    p = fma(p, s2, 1.0 / 13.0);
    p = fma(p, s2, 1.0 / 11.0);
    p = fma(p, s2, 1.0 / 9.0);
    p = fma(p, s2, 1.0 / 7.0);
    p = fma(p, s2, 1.0 / 5.0);
    p = fma(p, s2, 1.0 / 3.0);
    p = fma(p, s2, 1.0);
    return 2.0 * s * p;
}

double log(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return math_pole_err(1);
    }
    if (isinf(x)) {
        return x;
    }
    if (x == 1.0) {
        return 0.0;
    }
    int e = 0;
    double m = frexp(x, &e);
    if (m < K_SQRT1_2) {
        m *= 2.0;
        e -= 1;
    }
    double s = (m - 1.0) / (m + 1.0);
    double lnm = log_atanh_series(s);
    double ed = (double)e;
    return fma(ed, K_LN2_HI, fma(ed, K_LN2_LO, lnm));
}

float logf(float x) {
    return (float)log((double)x);
}

long double logl(long double x) {
    return (long double)log((double)x);
}

double log2(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return math_pole_err(1);
    }
    if (isinf(x)) {
        return x;
    }
    int e = 0;
    double m = frexp(x, &e);
    if (m < K_SQRT1_2) {
        m *= 2.0;
        e -= 1;
    }
    if (m == 1.0) {
        return (double)e;
    }
    double s = (m - 1.0) / (m + 1.0);
    return (double)e + log_atanh_series(s) * K_INV_LN2;
}

float log2f(float x) {
    return (float)log2((double)x);
}

long double log2l(long double x) {
    return (long double)log2((double)x);
}

double log10(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 0.0) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return math_pole_err(1);
    }
    if (isinf(x)) {
        return x;
    }
    if (x == 1.0) {
        return 0.0;
    }
    return log(x) * K_INV_LN10;
}

float log10f(float x) {
    return (float)log10((double)x);
}

long double log10l(long double x) {
    return (long double)log10((double)x);
}

double log1p(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    if (x < -1.0) {
        return math_domain_err();
    }
    if (x == -1.0) {
        return math_pole_err(1);
    }
    if (isinf(x)) {
        return x;
    }
    if (fabs(x) <= 0.25) {
        double s = x / (2.0 + x);
        return log_atanh_series(s);
    }
    return log(1.0 + x);
}

float log1pf(float x) {
    return (float)log1p((double)x);
}

long double log1pl(long double x) {
    return (long double)log1p((double)x);
}

static int is_odd_integer(double y) {
    if (!isfinite(y) || floor(y) != y) {
        return 0;
    }
    if (fabs(y) >= 9007199254740992.0) {
        return 0; /* all doubles >= 2^53 are even */
    }
    return (((uint64_t)fabs(y)) & 1ULL) != 0ULL;
}

double pow(double x, double y) {
    if (y == 0.0 || x == 1.0) {
        return 1.0;
    }
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    int odd = is_odd_integer(y);
    if (x == 0.0) {
        if (y < 0.0) {
            return math_pole_err(odd && signbit(x));
        }
        return (odd && signbit(x)) ? -0.0 : 0.0;
    }
    if (x == -1.0 && isinf(y)) {
        return 1.0;
    }
    if (isinf(y)) {
        double ax = fabs(x);
        if ((ax > 1.0 && y > 0.0) || (ax < 1.0 && y < 0.0)) {
            return (double)INFINITY;
        }
        return 0.0;
    }
    if (isinf(x)) {
        int neg = signbit(x) && odd;
        if (y > 0.0) {
            return neg ? -(double)INFINITY : (double)INFINITY;
        }
        return neg ? -0.0 : 0.0;
    }
    if (x < 0.0 && floor(y) != y) {
        return math_domain_err();
    }

    int neg_res = (x < 0.0) && odd;
    double ax = fabs(x);

    /* Exact binary exponentiation for small integer exponents. */
    if (floor(y) == y && fabs(y) <= 1024.0) {
        int exp_i = (int)fabs(y);
        double base = ax;
        double acc = 1.0;
        while (exp_i > 0) {
            if (exp_i & 1) {
                acc *= base;
            }
            exp_i >>= 1;
            if (exp_i > 0) {
                base *= base;
            }
        }
        if (y < 0.0) {
            acc = 1.0 / acc;
        }
        if (isinf(acc)) {
            return math_overflow_err(neg_res);
        }
        if (acc == 0.0) {
            return math_underflow_err(neg_res);
        }
        return neg_res ? -acc : acc;
    }

    double p = y * log2(ax);
    double r = exp2(p);
    return neg_res ? -r : r;
}

float powf(float x, float y) {
    return (float)pow((double)x, (double)y);
}

long double powl(long double x, long double y) {
    return (long double)pow((double)x, (double)y);
}

/* Lentz continued fraction for erfc(x) when x >= 2.0 (converges < 1e-16). */
static double erfc_cf(double x) {
    const double inv_sqrt_pi = 0.56418958354775628694807945156077259;
    double x2 = x * x;
    double b = x2 + 0.5;
    double c = 1.0e30;
    double d = 1.0 / b;
    double h = d;
    for (int i = 1; i <= 64; i++) {
        double a = -(double)i * ((double)i - 0.5);
        b += 2.0;
        d = fma(a, d, b);
        if (fabs(d) < 1.0e-30) {
            d = 1.0e-30;
        }
        c = b + a / c;
        if (fabs(c) < 1.0e-30) {
            c = 1.0e-30;
        }
        d = 1.0 / d;
        double delta = d * c;
        h *= delta;
        if (fabs(delta - 1.0) < 1.0e-16) {
            break;
        }
    }
    return exp(-x2) * inv_sqrt_pi * x * h;
}

/* Power series for erf(x) when |x| < 2.0: 2/sqrt(pi) * sum (-1)^k x^(2k+1) /
 * (k! * (2k+1)). */
static double erf_series(double x) {
    const double two_over_sqrt_pi = 1.12837916709551257389615890312154517;
    double x2 = x * x;
    double term = x;
    double sum = x;
    for (int k = 1; k <= 40; k++) {
        term *= -x2 / (double)k;
        double add = term / (double)(2 * k + 1);
        sum += add;
        if (fabs(add) < fabs(sum) * 1.0e-16) {
            break;
        }
    }
    return two_over_sqrt_pi * sum;
}

double erf(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    if (isinf(x)) {
        return x > 0.0 ? 1.0 : -1.0;
    }
    double ax = fabs(x);
    double r;
    if (ax < 2.0) {
        r = erf_series(ax);
    } else if (ax >= 6.0) {
        r = 1.0;
    } else {
        r = 1.0 - erfc_cf(ax);
    }
    return copysign(r, x);
}

float erff(float x) {
    return (float)erf((double)x);
}

long double erfl(long double x) {
    return (long double)erf((double)x);
}

double erfc(double x) {
    if (isnan(x)) {
        return x;
    }
    if (isinf(x)) {
        return x > 0.0 ? 0.0 : 2.0;
    }
    if (x == 0.0) {
        return 1.0;
    }
    if (x < 0.0) {
        return 2.0 - erfc(-x);
    }
    if (x < 2.0) {
        return 1.0 - erf_series(x);
    }
    if (x > 27.3) {
        return math_underflow_err(0);
    }
    return erfc_cf(x);
}

float erfcf(float x) {
    return (float)erfc((double)x);
}

long double erfcl(long double x) {
    return (long double)erfc((double)x);
}

/* Lanczos approximation (g=7, 9-term Godfrey coefficients) for Gamma(z). */
double tgamma(double x) {
    static const double p[9] = {
        0.99999999999980993227684700473478,  676.520368121885098567009190444019,
        -1259.13921672240287047156078755283, 771.3234287776530788486528258894,
        -176.61502916214059906584551354,     12.507343278686904814458936853,
        -0.13857109526572011689554707,       9.984369578019570859563e-6,
        1.50563273514931155834e-7,
    };
    const double sqrt_2pi = 2.50662827463100050241576528481104525;

    if (isnan(x)) {
        return x;
    }
    if (x == 0.0) {
        return math_pole_err(signbit(x));
    }
    if (isinf(x)) {
        return x > 0.0 ? x : math_domain_err();
    }
    if (x < 0.0 && floor(x) == x) {
        return math_domain_err();
    }
    if (x > 171.62437695630272) {
        return math_overflow_err(0);
    }
    if (x >= 1.0 && x <= 22.0 && floor(x) == x) {
        double fact = 1.0;
        int n = (int)x;
        for (int i = 2; i < n; i++) {
            fact *= (double)i;
        }
        return fact;
    }
    if (x < 0.5) {
        double s = sin(K_PI * x);
        if (s == 0.0) {
            return math_domain_err();
        }
        double denom = s * tgamma(1.0 - x);
        double res = K_PI / denom;
        if (isinf(res)) {
            return math_overflow_err(signbit(res));
        }
        return res;
    }
    double z = x - 1.0;
    double acc = p[0];
    for (int i = 1; i < 9; i++) {
        acc += p[i] / (z + (double)i);
    }
    double t = z + 7.5;
    double log_g = log(sqrt_2pi * acc) + (z + 0.5) * log(t) - t;
    return exp(log_g);
}

float tgammaf(float x) {
    return (float)tgamma((double)x);
}

long double tgammal(long double x) {
    return (long double)tgamma((double)x);
}
