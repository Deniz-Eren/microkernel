/**
 * @file     complex.c
 * @brief    ISO C17 and POSIX.1-2024 <complex.h> mathematical functions
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <complex.h>
#include <math.h>
#include "math_internal.h"

/* need:: COMP_LIBC_0010 */

/* ---- Manipulation functions ---- */

double creal(double complex z) {
    union {
        double complex z;
        double a[2];
    } u = {.z = z};
    return u.a[0];
}

float crealf(float complex z) {
    union {
        float complex z;
        float a[2];
    } u = {.z = z};
    return u.a[0];
}

long double creall(long double complex z) {
    union {
        long double complex z;
        long double a[2];
    } u = {.z = z};
    return u.a[0];
}

double cimag(double complex z) {
    union {
        double complex z;
        double a[2];
    } u = {.z = z};
    return u.a[1];
}

float cimagf(float complex z) {
    union {
        float complex z;
        float a[2];
    } u = {.z = z};
    return u.a[1];
}

long double cimagl(long double complex z) {
    union {
        long double complex z;
        long double a[2];
    } u = {.z = z};
    return u.a[1];
}

double carg(double complex z) {
    return atan2(cimag(z), creal(z));
}

float cargf(float complex z) {
    return atan2f(cimagf(z), crealf(z));
}

long double cargl(long double complex z) {
    return atan2l(cimagl(z), creall(z));
}

double complex conj(double complex z) {
    return CMPLX(creal(z), -cimag(z));
}

float complex conjf(float complex z) {
    return CMPLXF(crealf(z), -cimagf(z));
}

long double complex conjl(long double complex z) {
    return CMPLXL(creall(z), -cimagl(z));
}

double complex cproj(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isinf(x) || isinf(y)) {
        return CMPLX((double)INFINITY, copysign(0.0, y));
    }
    return z;
}

float complex cprojf(float complex z) {
    float x = crealf(z);
    float y = cimagf(z);
    if (isinf(x) || isinf(y)) {
        return CMPLXF((float)INFINITY, copysignf(0.0f, y));
    }
    return z;
}

long double complex cprojl(long double complex z) {
    long double x = creall(z);
    long double y = cimagl(z);
    if (isinf(x) || isinf(y)) {
        return CMPLXL((long double)INFINITY, copysignl(0.0L, y));
    }
    return z;
}

/* ---- Absolute value, square root, exp, log, pow ---- */

double cabs(double complex z) {
    return hypot(creal(z), cimag(z));
}

float cabsf(float complex z) {
    return hypotf(crealf(z), cimagf(z));
}

long double cabsl(long double complex z) {
    return hypotl(creall(z), cimagl(z));
}

double complex csqrt(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isinf(y)) {
        return CMPLX((double)INFINITY, y);
    }
    if (isinf(x)) {
        if (x < 0.0) {
            return CMPLX(isnan(y) ? y : 0.0, copysign((double)INFINITY, y));
        }
        return CMPLX(x, isnan(y) ? y : copysign(0.0, y));
    }
    if (isnan(x) || isnan(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    if (x == 0.0 && y == 0.0) {
        return CMPLX(0.0, y);
    }
    double r = hypot(x, y);
    if (x >= 0.0) {
        double u = sqrt(0.5 * (r + x));
        double v = (u == 0.0) ? y : (0.5 * y / u);
        return CMPLX(u, v);
    }
    double v = copysign(sqrt(0.5 * (r - x)), y);
    double u = (v == 0.0) ? 0.0 : fabs(0.5 * y / v);
    return CMPLX(u, v);
}

float complex csqrtf(float complex z) {
    double complex r = csqrt(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex csqrtl(long double complex z) {
    double complex r = csqrt(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex cexp(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isinf(x)) {
        if (x < 0.0) {
            if (!isfinite(y)) {
                return CMPLX(0.0, 0.0);
            }
            return CMPLX(copysign(0.0, cos(y)), copysign(0.0, sin(y)));
        }
        if (y == 0.0) {
            return CMPLX((double)INFINITY, y);
        }
        if (!isfinite(y)) {
            return CMPLX((double)INFINITY, (double)NAN);
        }
        return CMPLX(x * cos(y), x * sin(y));
    }
    if (y == 0.0) {
        return CMPLX(exp(x), y);
    }
    if (!isfinite(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    double ex = exp(x);
    return CMPLX(ex * cos(y), ex * sin(y));
}

float complex cexpf(float complex z) {
    double complex r = cexp(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex cexpl(long double complex z) {
    double complex r = cexp(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex clog(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    return CMPLX(log(hypot(x, y)), atan2(y, x));
}

float complex clogf(float complex z) {
    double complex r = clog(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex clogl(long double complex z) {
    double complex r = clog(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex cpow(double complex x, double complex y) {
    return cexp(y * clog(x));
}

float complex cpowf(float complex x, float complex y) {
    double complex r = cpow(CMPLX((double)crealf(x), (double)cimagf(x)),
                            CMPLX((double)crealf(y), (double)cimagf(y)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex cpowl(long double complex x, long double complex y) {
    double complex r = cpow(CMPLX((double)creall(x), (double)cimagl(x)),
                            CMPLX((double)creall(y), (double)cimagl(y)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

/* ---- Hyperbolic functions ---- */

double complex csinh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (y == 0.0) {
        return CMPLX(sinh(x), y);
    }
    if (x == 0.0) {
        if (!isfinite(y)) {
            return CMPLX(x, (double)NAN);
        }
        return CMPLX(x, sin(y));
    }
    if (isinf(x)) {
        if (!isfinite(y)) {
            return CMPLX(x, (double)NAN);
        }
        return CMPLX(x * cos(y), fabs(x) * sin(y));
    }
    if (!isfinite(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    return CMPLX(sinh(x) * cos(y), cosh(x) * sin(y));
}

float complex csinhf(float complex z) {
    double complex r = csinh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex csinhl(long double complex z) {
    double complex r = csinh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex ccosh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (y == 0.0) {
        return CMPLX(cosh(x), copysign(0.0, x * y));
    }
    if (x == 0.0) {
        if (!isfinite(y)) {
            return CMPLX((double)NAN, copysign(0.0, x));
        }
        return CMPLX(cos(y), copysign(0.0, x * sin(y)));
    }
    if (isinf(x)) {
        if (!isfinite(y)) {
            return CMPLX((double)INFINITY, (double)NAN);
        }
        return CMPLX((double)INFINITY * cos(y), x * sin(y));
    }
    if (!isfinite(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    return CMPLX(cosh(x) * cos(y), sinh(x) * sin(y));
}

float complex ccoshf(float complex z) {
    double complex r = ccosh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex ccoshl(long double complex z) {
    double complex r = ccosh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex ctanh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (y == 0.0) {
        return CMPLX(tanh(x), y);
    }
    if (isinf(x)) {
        return CMPLX(copysign(1.0, x), isfinite(y) ? copysign(0.0, sin(2.0 * y))
                                                   : copysign(0.0, y));
    }
    if (isnan(x)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    if (!isfinite(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    if (fabs(x) > 22.0) {
        return CMPLX(copysign(1.0, x), copysign(0.0, sin(2.0 * y)));
    }
    double den = cosh(2.0 * x) + cos(2.0 * y);
    return CMPLX(sinh(2.0 * x) / den, sin(2.0 * y) / den);
}

float complex ctanhf(float complex z) {
    double complex r = ctanh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex ctanhl(long double complex z) {
    double complex r = ctanh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

/* ---- Trigonometric functions (defined via hyperbolic identities) ---- */

double complex csin(double complex z) {
    double complex w = csinh(CMPLX(-cimag(z), creal(z)));
    return CMPLX(cimag(w), -creal(w));
}

float complex csinf(float complex z) {
    double complex r = csin(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex csinl(long double complex z) {
    double complex r = csin(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex ccos(double complex z) {
    return ccosh(CMPLX(-cimag(z), creal(z)));
}

float complex ccosf(float complex z) {
    double complex r = ccos(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex ccosl(long double complex z) {
    double complex r = ccos(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex ctan(double complex z) {
    double complex w = ctanh(CMPLX(-cimag(z), creal(z)));
    return CMPLX(cimag(w), -creal(w));
}

float complex ctanf(float complex z) {
    double complex r = ctan(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex ctanl(long double complex z) {
    double complex r = ctan(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

/* ---- Inverse hyperbolic and inverse trigonometric functions ---- */

double complex casinh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isnan(x) || isnan(y)) {
        if (isinf(x)) {
            return CMPLX(x, (double)NAN);
        }
        if (isinf(y)) {
            return CMPLX(copysign((double)INFINITY, x), (double)NAN);
        }
        return CMPLX((double)NAN, (double)NAN);
    }
    if (isinf(x) || isinf(y)) {
        double re = copysign((double)INFINITY, x);
        double im = isinf(x)
                        ? (isinf(y) ? copysign(K_PI_4, y) : copysign(0.0, y))
                        : copysign(K_PI_2, y);
        return CMPLX(re, im);
    }
    /* Use odd symmetry casinh(-z) = -casinh(z) to keep Re(z) >= 0. */
    int neg = signbit(x) != 0;
    double complex p = neg ? CMPLX(-x, -y) : z;
    double complex root = csqrt(p * p + CMPLX(1.0, 0.0));
    double complex w = clog(p + root);
    if (neg) {
        return CMPLX(-creal(w), -cimag(w));
    }
    return w;
}

float complex casinhf(float complex z) {
    double complex r = casinh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex casinhl(long double complex z) {
    double complex r = casinh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex cacosh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isnan(x) || isnan(y)) {
        if (isinf(x) || isinf(y)) {
            return CMPLX((double)INFINITY, (double)NAN);
        }
        return CMPLX((double)NAN, (double)NAN);
    }
    if (isinf(x) || isinf(y)) {
        double im = 0.0;
        if (isinf(x)) {
            if (isinf(y)) {
                im = (x < 0.0) ? (3.0 * K_PI_4) : K_PI_4;
            } else {
                im = (x < 0.0) ? K_PI : 0.0;
            }
        } else {
            im = K_PI_2;
        }
        return CMPLX((double)INFINITY, copysign(im, y));
    }
    double complex s1 = csqrt(CMPLX(x - 1.0, y));
    double complex s2 = csqrt(CMPLX(x + 1.0, y));
    double complex w = clog(z + s1 * s2);
    double re = creal(w);
    double im = cimag(w);
    if (re < 0.0) {
        re = -re;
        im = -im;
    }
    return CMPLX(re, copysign(fabs(im), y));
}

float complex cacoshf(float complex z) {
    double complex r = cacosh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex cacoshl(long double complex z) {
    double complex r = cacosh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex catanh(double complex z) {
    double x = creal(z);
    double y = cimag(z);
    if (isinf(y)) {
        return CMPLX(copysign(0.0, x), copysign(K_PI_2, y));
    }
    if (isinf(x)) {
        return CMPLX(copysign(0.0, x), isnan(y) ? y : copysign(K_PI_2, y));
    }
    if (isnan(x) || isnan(y)) {
        return CMPLX((double)NAN, (double)NAN);
    }
    if (y == 0.0 && fabs(x) == 1.0) {
        return CMPLX(copysign((double)INFINITY, x), y);
    }
    int neg = signbit(x) != 0;
    double ax = fabs(x);
    double ay = fabs(y);
    double r = (1.0 - ax) * (1.0 - ax) + ay * ay;
    double re = 0.25 * log1p(4.0 * ax / r);
    double im = 0.5 * atan2(2.0 * ay, (1.0 - ax) * (1.0 + ax) - ay * ay);
    return CMPLX(neg ? -re : re, copysign(im, y));
}

float complex catanhf(float complex z) {
    double complex r = catanh(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex catanhl(long double complex z) {
    double complex r = catanh(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex casin(double complex z) {
    double complex w = casinh(CMPLX(-cimag(z), creal(z)));
    return CMPLX(cimag(w), -creal(w));
}

float complex casinf(float complex z) {
    double complex r = casin(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex casinl(long double complex z) {
    double complex r = casin(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex cacos(double complex z) {
    double complex a = casin(z);
    return CMPLX(K_PI_2 - creal(a), -cimag(a));
}

float complex cacosf(float complex z) {
    double complex r = cacos(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex cacosl(long double complex z) {
    double complex r = cacos(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}

double complex catan(double complex z) {
    double complex w = catanh(CMPLX(-cimag(z), creal(z)));
    return CMPLX(cimag(w), -creal(w));
}

float complex catanf(float complex z) {
    double complex r = catan(CMPLX((double)crealf(z), (double)cimagf(z)));
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex catanl(long double complex z) {
    double complex r = catan(CMPLX((double)creall(z), (double)cimagl(z)));
    return CMPLXL((long double)creal(r), (long double)cimag(r));
}
