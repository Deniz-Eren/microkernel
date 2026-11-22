/**
 * @file     math_trig.c
 * @brief    POSIX.1-2024 <math.h> trig, inverse trig, and hyperbolic functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "math_internal.h"

/* need:: COMP_LIBC_0007 */

/* Three-part Cody-Waite split of pi/2 for accurate quadrant reduction. */
#define PIO2_1 1.57079632673412561417e+00 /* 33-bit exact head */
#define PIO2_2 6.07710050630396597660e-11 /* next 33 bits */
#define PIO2_3 2.02226624879595063154e-21
#define INV_PIO2 0.63661977236758134307553505349005745

static double sin_poly(double r) {
    double r2 = r * r;
    double p = 1.0 / 355687428096000.0;     /* +1/17! */
    p = fma(p, r2, -1.0 / 1307674368000.0); /* -1/15! */
    p = fma(p, r2, 1.0 / 6227020800.0);     /* +1/13! */
    p = fma(p, r2, -1.0 / 39916800.0);      /* -1/11! */
    p = fma(p, r2, 1.0 / 362880.0);         /* +1/9!  */
    p = fma(p, r2, -1.0 / 5040.0);          /* -1/7!  */
    p = fma(p, r2, 1.0 / 120.0);            /* +1/5!  */
    p = fma(p, r2, -1.0 / 6.0);             /* -1/3!  */
    return fma(p * r2, r, r);
}

static double cos_poly(double r) {
    double r2 = r * r;
    double p = 1.0 / 20922789888000.0;    /* +1/16! */
    p = fma(p, r2, -1.0 / 87178291200.0); /* -1/14! */
    p = fma(p, r2, 1.0 / 479001600.0);    /* +1/12! */
    p = fma(p, r2, -1.0 / 3628800.0);     /* -1/10! */
    p = fma(p, r2, 1.0 / 40320.0);        /* +1/8!  */
    p = fma(p, r2, -1.0 / 720.0);         /* -1/6!  */
    p = fma(p, r2, 1.0 / 24.0);           /* +1/4!  */
    p = fma(p, r2, -0.5);                 /* -1/2!  */
    return fma(p, r2, 1.0);
}

static int reduce_pio2(double x, double *rem) {
    double k = round(x * INV_PIO2);
    double r = fma(-k, PIO2_1, x);
    r = fma(-k, PIO2_2, r);
    r = fma(-k, PIO2_3, r);
    *rem = r;
    return (int)((int64_t)k & 3);
}

double sin(double x) {
    if (isnan(x)) {
        return x;
    }
    if (isinf(x)) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return x;
    }
    double r = 0.0;
    int q = reduce_pio2(x, &r);
    switch (q) {
    case 0:
        return sin_poly(r);
    case 1:
        return cos_poly(r);
    case 2:
        return -sin_poly(r);
    default:
        return -cos_poly(r);
    }
}

float sinf(float x) {
    return (float)sin((double)x);
}

long double sinl(long double x) {
    return (long double)sin((double)x);
}

double cos(double x) {
    if (isnan(x)) {
        return x;
    }
    if (isinf(x)) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return 1.0;
    }
    double r = 0.0;
    int q = reduce_pio2(x, &r);
    switch (q) {
    case 0:
        return cos_poly(r);
    case 1:
        return -sin_poly(r);
    case 2:
        return -cos_poly(r);
    default:
        return sin_poly(r);
    }
}

float cosf(float x) {
    return (float)cos((double)x);
}

long double cosl(long double x) {
    return (long double)cos((double)x);
}

double tan(double x) {
    if (isnan(x)) {
        return x;
    }
    if (isinf(x)) {
        return math_domain_err();
    }
    if (x == 0.0) {
        return x;
    }
    double r = 0.0;
    int q = reduce_pio2(x, &r);
    double s = sin_poly(r);
    double c = cos_poly(r);
    if ((q & 1) == 0) {
        return s / c;
    }
    return -c / s;
}

float tanf(float x) {
    return (float)tan((double)x);
}

long double tanl(long double x) {
    return (long double)tan((double)x);
}

/* Direct Taylor series for asin(x) on [0, 0.5]: x + sum (2k-1)!!/(2k)!! *
 * x^(2k+1)/(2k+1), accurate to < 1 ULP for |x| <= 0.5. */
static double asin_core(double x) {
    double x2 = x * x;
    double term = x;
    double sum = x;
    for (int k = 1; k <= 24; k++) {
        double num = (double)(2 * k - 1);
        double den = (double)(2 * k);
        term *= x2 * (num / den);
        double add = term / (double)(2 * k + 1);
        sum += add;
        if (fabs(add) < fabs(sum) * 1.0e-16) {
            break;
        }
    }
    return sum;
}

double asin(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    double ax = fabs(x);
    if (ax > 1.0) {
        return math_domain_err();
    }
    if (ax == 1.0) {
        return copysign(K_PI_2, x);
    }
    double res;
    if (ax <= 0.5) {
        res = asin_core(ax);
    } else {
        /* Half-angle identity avoids cancellation near |x| = 1. */
        double s = sqrt(0.5 * (1.0 - ax));
        res = K_PI_2 - 2.0 * asin_core(s);
    }
    return copysign(res, x);
}

float asinf(float x) {
    return (float)asin((double)x);
}

long double asinl(long double x) {
    return (long double)asin((double)x);
}

double acos(double x) {
    if (isnan(x)) {
        return x;
    }
    double ax = fabs(x);
    if (ax > 1.0) {
        return math_domain_err();
    }
    if (x == 1.0) {
        return 0.0;
    }
    if (x == -1.0) {
        return K_PI;
    }
    if (ax <= 0.5) {
        return K_PI_2 - asin_core(x);
    }
    if (x > 0.5) {
        return 2.0 * asin_core(sqrt(0.5 * (1.0 - x)));
    }
    return K_PI - 2.0 * asin_core(sqrt(0.5 * (1.0 + x)));
}

float acosf(float x) {
    return (float)acos((double)x);
}

long double acosl(long double x) {
    return (long double)acos((double)x);
}

/* Alternating series for atan(z) on [0, tan(pi/8) ~= 0.41421356]. */
static double atan_core(double z) {
    double z2 = z * z;
    double p = 0.0;
    for (int k = 20; k >= 1; k--) {
        double c = (k & 1) ? (-1.0 / (double)(2 * k + 1))
                           : (1.0 / (double)(2 * k + 1));
        p = fma(p, z2, c);
    }
    return fma(p * z2, z, z);
}

double atan(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    if (isinf(x)) {
        return copysign(K_PI_2, x);
    }
    double ax = fabs(x);
    double r;
    if (ax <= 0.41421356237309504880) {
        r = atan_core(ax);
    } else if (ax <= 2.41421356237309504880) {
        r = K_PI_4 + atan_core((ax - 1.0) / (ax + 1.0));
    } else {
        r = K_PI_2 - atan_core(1.0 / ax);
    }
    return copysign(r, x);
}

float atanf(float x) {
    return (float)atan((double)x);
}

long double atanl(long double x) {
    return (long double)atan((double)x);
}

double atan2(double y, double x) {
    if (isnan(x) || isnan(y)) {
        return x + y;
    }
    if (y == 0.0) {
        if (signbit(x)) {
            return copysign(K_PI, y);
        }
        return y;
    }
    if (x == 0.0) {
        return copysign(K_PI_2, y);
    }
    if (isinf(y)) {
        if (isinf(x)) {
            return copysign(x > 0.0 ? K_PI_4 : (3.0 * K_PI_4), y);
        }
        return copysign(K_PI_2, y);
    }
    if (isinf(x)) {
        return copysign(x > 0.0 ? 0.0 : K_PI, y);
    }
    double a = atan(y / x);
    if (x > 0.0) {
        return a;
    }
    return y >= 0.0 ? (a + K_PI) : (a - K_PI);
}

float atan2f(float y, float x) {
    return (float)atan2((double)y, (double)x);
}

long double atan2l(long double y, long double x) {
    return (long double)atan2((double)y, (double)x);
}

double sinh(double x) {
    if (isnan(x) || x == 0.0 || isinf(x)) {
        return x;
    }
    double ax = fabs(x);
    if (ax > 710.4758600739439) {
        return math_overflow_err(signbit(x));
    }
    double r;
    if (ax < 22.0) {
        double t = expm1(ax);
        r = 0.5 * (t + t / (t + 1.0));
    } else if (ax < 709.0) {
        r = 0.5 * exp(ax);
    } else {
        double h = exp(0.5 * ax);
        r = (0.5 * h) * h;
        if (isinf(r)) {
            return math_overflow_err(signbit(x));
        }
    }
    return copysign(r, x);
}

float sinhf(float x) {
    return (float)sinh((double)x);
}

long double sinhl(long double x) {
    return (long double)sinh((double)x);
}

double cosh(double x) {
    if (isnan(x)) {
        return x;
    }
    if (isinf(x)) {
        return (double)INFINITY;
    }
    if (x == 0.0) {
        return 1.0;
    }
    double ax = fabs(x);
    if (ax > 710.4758600739439) {
        return math_overflow_err(0);
    }
    if (ax < 22.0) {
        double e = exp(ax);
        return 0.5 * (e + 1.0 / e);
    }
    if (ax < 709.0) {
        return 0.5 * exp(ax);
    }
    double h = exp(0.5 * ax);
    double r = (0.5 * h) * h;
    if (isinf(r)) {
        return math_overflow_err(0);
    }
    return r;
}

float coshf(float x) {
    return (float)cosh((double)x);
}

long double coshl(long double x) {
    return (long double)cosh((double)x);
}

double tanh(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    double ax = fabs(x);
    if (ax >= 22.0) {
        return copysign(1.0, x);
    }
    double t = expm1(2.0 * ax);
    return copysign(t / (t + 2.0), x);
}

float tanhf(float x) {
    return (float)tanh((double)x);
}

long double tanhl(long double x) {
    return (long double)tanh((double)x);
}

double asinh(double x) {
    if (isnan(x) || x == 0.0 || isinf(x)) {
        return x;
    }
    double ax = fabs(x);
    double r;
    if (ax > 0x1.0p28) {
        r = log(ax) + K_LN2;
    } else if (ax > 2.0) {
        r = log(2.0 * ax + 1.0 / (sqrt(fma(ax, ax, 1.0)) + ax));
    } else {
        double ax2 = ax * ax;
        r = log1p(ax + ax2 / (1.0 + sqrt(1.0 + ax2)));
    }
    return copysign(r, x);
}

float asinhf(float x) {
    return (float)asinh((double)x);
}

long double asinhl(long double x) {
    return (long double)asinh((double)x);
}

double acosh(double x) {
    if (isnan(x)) {
        return x;
    }
    if (x < 1.0) {
        return math_domain_err();
    }
    if (x == 1.0) {
        return 0.0;
    }
    if (isinf(x)) {
        return x;
    }
    if (x > 0x1.0p28) {
        return log(x) + K_LN2;
    }
    double xm1 = x - 1.0;
    return log1p(xm1 + sqrt(fma(xm1, xm1, 2.0 * xm1)));
}

float acoshf(float x) {
    return (float)acosh((double)x);
}

long double acoshl(long double x) {
    return (long double)acosh((double)x);
}

double atanh(double x) {
    if (isnan(x) || x == 0.0) {
        return x;
    }
    double ax = fabs(x);
    if (ax > 1.0) {
        return math_domain_err();
    }
    if (ax == 1.0) {
        return math_pole_err(signbit(x));
    }
    double r = 0.5 * log1p(2.0 * ax / (1.0 - ax));
    return copysign(r, x);
}

float atanhf(float x) {
    return (float)atanh((double)x);
}

long double atanhl(long double x) {
    return (long double)atanh((double)x);
}
