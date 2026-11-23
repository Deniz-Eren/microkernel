/**
 * @file     complex_arith.c
 * @brief    ISO C17 Annex G compiler-rt complex multiply and divide helpers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <complex.h>
#include <math.h>

/* need:: COMP_LIBC_0010 */

double complex __muldc3(double a, double b, double c, double d) {
    double ac = a * c;
    double bd = b * d;
    double ad = a * d;
    double bc = b * c;
    double x = ac - bd;
    double y = ad + bc;
    if (isnan(x) && isnan(y)) {
        int recalc = 0;
        if (isinf(a) || isinf(b)) {
            a = copysign(isinf(a) ? 1.0 : 0.0, a);
            b = copysign(isinf(b) ? 1.0 : 0.0, b);
            if (isnan(c)) {
                c = copysign(0.0, c);
            }
            if (isnan(d)) {
                d = copysign(0.0, d);
            }
            recalc = 1;
        }
        if (isinf(c) || isinf(d)) {
            c = copysign(isinf(c) ? 1.0 : 0.0, c);
            d = copysign(isinf(d) ? 1.0 : 0.0, d);
            if (isnan(a)) {
                a = copysign(0.0, a);
            }
            if (isnan(b)) {
                b = copysign(0.0, b);
            }
            recalc = 1;
        }
        if (!recalc && (isinf(ac) || isinf(bd) || isinf(ad) || isinf(bc))) {
            if (isnan(a)) {
                a = copysign(0.0, a);
            }
            if (isnan(b)) {
                b = copysign(0.0, b);
            }
            if (isnan(c)) {
                c = copysign(0.0, c);
            }
            if (isnan(d)) {
                d = copysign(0.0, d);
            }
            recalc = 1;
        }
        if (recalc) {
            x = (double)INFINITY * (a * c - b * d);
            y = (double)INFINITY * (a * d + b * c);
        }
    }
    return CMPLX(x, y);
}

float complex __mulsc3(float a, float b, float c, float d) {
    double complex r = __muldc3((double)a, (double)b, (double)c, (double)d);
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex __mulxc3(long double a, long double b, long double c,
                             long double d) {
    long double ac = a * c;
    long double bd = b * d;
    long double ad = a * d;
    long double bc = b * c;
    long double x = ac - bd;
    long double y = ad + bc;
    if (isnan(x) && isnan(y)) {
        int recalc = 0;
        if (isinf(a) || isinf(b)) {
            a = copysignl(isinf(a) ? 1.0L : 0.0L, a);
            b = copysignl(isinf(b) ? 1.0L : 0.0L, b);
            if (isnan(c)) {
                c = copysignl(0.0L, c);
            }
            if (isnan(d)) {
                d = copysignl(0.0L, d);
            }
            recalc = 1;
        }
        if (isinf(c) || isinf(d)) {
            c = copysignl(isinf(c) ? 1.0L : 0.0L, c);
            d = copysignl(isinf(d) ? 1.0L : 0.0L, d);
            if (isnan(a)) {
                a = copysignl(0.0L, a);
            }
            if (isnan(b)) {
                b = copysignl(0.0L, b);
            }
            recalc = 1;
        }
        if (!recalc && (isinf(ac) || isinf(bd) || isinf(ad) || isinf(bc))) {
            if (isnan(a)) {
                a = copysignl(0.0L, a);
            }
            if (isnan(b)) {
                b = copysignl(0.0L, b);
            }
            if (isnan(c)) {
                c = copysignl(0.0L, c);
            }
            if (isnan(d)) {
                d = copysignl(0.0L, d);
            }
            recalc = 1;
        }
        if (recalc) {
            x = (long double)INFINITY * (a * c - b * d);
            y = (long double)INFINITY * (a * d + b * c);
        }
    }
    return CMPLXL(x, y);
}

double complex __divdc3(double a, double b, double c, double d) {
    int ilogbw = 0;
    double logbw = logb(fmax(fabs(c), fabs(d)));
    if (isfinite(logbw)) {
        ilogbw = (int)logbw;
        c = scalbn(c, -ilogbw);
        d = scalbn(d, -ilogbw);
    }
    double denom = c * c + d * d;
    double x = scalbn((a * c + b * d) / denom, -ilogbw);
    double y = scalbn((b * c - a * d) / denom, -ilogbw);
    if (isnan(x) && isnan(y)) {
        if (denom == 0.0 && (!isnan(a) || !isnan(b))) {
            x = copysign((double)INFINITY, c) * a;
            y = copysign((double)INFINITY, c) * b;
        } else if ((isinf(a) || isinf(b)) && isfinite(c) && isfinite(d)) {
            a = copysign(isinf(a) ? 1.0 : 0.0, a);
            b = copysign(isinf(b) ? 1.0 : 0.0, b);
            x = (double)INFINITY * (a * c + b * d);
            y = (double)INFINITY * (b * c - a * d);
        } else if (isinf(logbw) && logbw > 0.0 && isfinite(a) && isfinite(b)) {
            c = copysign(isinf(c) ? 1.0 : 0.0, c);
            d = copysign(isinf(d) ? 1.0 : 0.0, d);
            x = 0.0 * (a * c + b * d);
            y = 0.0 * (b * c - a * d);
        }
    }
    return CMPLX(x, y);
}

float complex __divsc3(float a, float b, float c, float d) {
    double complex r = __divdc3((double)a, (double)b, (double)c, (double)d);
    return CMPLXF((float)creal(r), (float)cimag(r));
}

long double complex __divxc3(long double a, long double b, long double c,
                             long double d) {
    int ilogbw = 0;
    long double logbw = logbl(fmaxl(fabsl(c), fabsl(d)));
    if (isfinite(logbw)) {
        ilogbw = (int)logbw;
        c = scalbnl(c, -ilogbw);
        d = scalbnl(d, -ilogbw);
    }
    long double denom = c * c + d * d;
    long double x = scalbnl((a * c + b * d) / denom, -ilogbw);
    long double y = scalbnl((b * c - a * d) / denom, -ilogbw);
    if (isnan(x) && isnan(y)) {
        if (denom == 0.0L && (!isnan(a) || !isnan(b))) {
            x = copysignl((long double)INFINITY, c) * a;
            y = copysignl((long double)INFINITY, c) * b;
        } else if ((isinf(a) || isinf(b)) && isfinite(c) && isfinite(d)) {
            a = copysignl(isinf(a) ? 1.0L : 0.0L, a);
            b = copysignl(isinf(b) ? 1.0L : 0.0L, b);
            x = (long double)INFINITY * (a * c + b * d);
            y = (long double)INFINITY * (b * c - a * d);
        } else if (isinf(logbw) && logbw > 0.0L && isfinite(a) && isfinite(b)) {
            c = copysignl(isinf(c) ? 1.0L : 0.0L, c);
            d = copysignl(isinf(d) ? 1.0L : 0.0L, d);
            x = 0.0L * (a * c + b * d);
            y = 0.0L * (b * c - a * d);
        }
    }
    return CMPLXL(x, y);
}
