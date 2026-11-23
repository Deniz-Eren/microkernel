/**
 * @file     num.c
 * @brief    Arbitrary-precision decimal arithmetic and math library for bc
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bc.h"

/* need:: COMP_LIBC_0025 */

static void bc_norm(bc_num_t *a) {
    while (a->len > a->scale + 1 && a->digs[a->len - 1] == 0) {
        a->len--;
    }
    if (a->len == 0) {
        a->neg = 0;
        a->scale = 0;
        return;
    }
    int nz = 0;
    for (size_t i = 0; i < a->len; i++) {
        if (a->digs[i] != 0) {
            nz = 1;
            break;
        }
    }
    if (!nz) {
        a->neg = 0;
    }
}

static bc_num_t bc_alloc(size_t len, size_t scale) {
    bc_num_t n = {0};
    size_t need = (len > 0) ? len : 1;
    n.digs = calloc(need, 1);
    if (n.digs) {
        n.len = need;
        n.cap = need;
        n.scale = scale;
    }
    return n;
}

bc_num_t bc_zero(void) {
    return bc_alloc(1, 0);
}

bc_num_t bc_from_long(long v) {
    int neg = 0;
    unsigned long uv = 0;
    if (v < 0) {
        neg = 1;
        uv = (unsigned long)(-(v + 1)) + 1ul;
    } else {
        uv = (unsigned long)v;
    }
    bc_num_t n = bc_alloc(24, 0);
    if (!n.digs) {
        return n;
    }
    size_t i = 0;
    do {
        n.digs[i++] = (uint8_t)(uv % 10ul);
        uv /= 10ul;
    } while (uv > 0);
    n.len = i;
    n.neg = neg;
    bc_norm(&n);
    return n;
}

bc_num_t bc_copy(const bc_num_t *a) {
    bc_num_t c = bc_alloc(a->len > 0 ? a->len : 1, a->scale);
    if (c.digs && a->digs && a->len > 0) {
        (void)memcpy(c.digs, a->digs, a->len);
        c.len = a->len;
        c.neg = a->neg;
    }
    bc_norm(&c);
    return c;
}

void bc_free(bc_num_t *a) {
    free(a->digs);
    a->digs = NULL;
    a->len = 0;
    a->cap = 0;
    a->scale = 0;
    a->neg = 0;
}

int bc_is_zero(const bc_num_t *a) {
    for (size_t i = 0; i < a->len; i++) {
        if (a->digs[i] != 0) {
            return 0;
        }
    }
    return 1;
}

long bc_to_long(const bc_num_t *a) {
    unsigned long v = 0;
    for (size_t i = a->len; i > a->scale; i--) {
        v = v * 10ul + a->digs[i - 1];
    }
    return a->neg ? -(long)v : (long)v;
}

double bc_to_double(const bc_num_t *a) {
    double v = 0.0;
    for (size_t i = a->len; i > 0; i--) {
        v = v * 10.0 + (double)a->digs[i - 1];
    }
    for (size_t i = 0; i < a->scale; i++) {
        v /= 10.0;
    }
    return a->neg ? -v : v;
}

bc_num_t bc_from_double(double d, size_t scale) {
    char buf[128];
    int sc = (scale > 15) ? 15 : (int)scale;
    (void)snprintf(buf, sizeof(buf), "%.*f", sc, d);
    bc_num_t r = bc_from_str(buf, 10);
    bc_num_t out = bc_rescale(&r, scale);
    bc_free(&r);
    return out;
}

size_t bc_num_length(const bc_num_t *a) {
    size_t top = a->len;
    while (top > a->scale && a->digs[top - 1] == 0) {
        top--;
    }
    if (top == a->scale) {
        return (a->scale > 0) ? a->scale : 1;
    }
    return top;
}

bc_num_t bc_rescale(const bc_num_t *a, size_t new_scale) {
    if (a->scale == new_scale) {
        return bc_copy(a);
    }
    if (new_scale > a->scale) {
        size_t diff = new_scale - a->scale;
        bc_num_t r = bc_alloc(a->len + diff, new_scale);
        if (r.digs && a->digs) {
            (void)memcpy(r.digs + diff, a->digs, a->len);
            r.neg = a->neg;
        }
        bc_norm(&r);
        return r;
    }
    size_t drop = a->scale - new_scale;
    if (drop >= a->len) {
        bc_num_t z = bc_alloc(new_scale + 1, new_scale);
        return z;
    }
    size_t nlen = a->len - drop;
    bc_num_t r = bc_alloc(nlen > new_scale ? nlen : new_scale + 1, new_scale);
    if (r.digs && a->digs) {
        (void)memcpy(r.digs, a->digs + drop, nlen);
        r.neg = a->neg;
    }
    bc_norm(&r);
    return r;
}

static int cmp_mag_aligned(const bc_num_t *a, const bc_num_t *b) {
    size_t la = a->len;
    size_t lb = b->len;
    while (la > 1 && a->digs[la - 1] == 0) {
        la--;
    }
    while (lb > 1 && b->digs[lb - 1] == 0) {
        lb--;
    }
    if (la != lb) {
        return (la < lb) ? -1 : 1;
    }
    for (size_t i = la; i > 0; i--) {
        if (a->digs[i - 1] != b->digs[i - 1]) {
            return (a->digs[i - 1] < b->digs[i - 1]) ? -1 : 1;
        }
    }
    return 0;
}

int bc_cmp(const bc_num_t *a, const bc_num_t *b) {
    int az = bc_is_zero(a);
    int bz = bc_is_zero(b);
    if (az && bz) {
        return 0;
    }
    if (a->neg != b->neg) {
        return a->neg ? -1 : 1;
    }
    size_t sc = (a->scale > b->scale) ? a->scale : b->scale;
    bc_num_t ra = bc_rescale(a, sc);
    bc_num_t rb = bc_rescale(b, sc);
    int c = cmp_mag_aligned(&ra, &rb);
    bc_free(&ra);
    bc_free(&rb);
    return a->neg ? -c : c;
}

static bc_num_t add_mag(const bc_num_t *a, const bc_num_t *b, size_t sc) {
    size_t m = (a->len > b->len) ? a->len : b->len;
    bc_num_t r = bc_alloc(m + 1, sc);
    if (!r.digs) {
        return r;
    }
    unsigned int carry = 0;
    for (size_t i = 0; i < m; i++) {
        unsigned int da = (i < a->len) ? a->digs[i] : 0u;
        unsigned int db = (i < b->len) ? b->digs[i] : 0u;
        unsigned int s = da + db + carry;
        r.digs[i] = (uint8_t)(s % 10u);
        carry = s / 10u;
    }
    r.digs[m] = (uint8_t)carry;
    bc_norm(&r);
    return r;
}

static bc_num_t sub_mag(const bc_num_t *a, const bc_num_t *b, size_t sc) {
    /* Assumes |a| >= |b| */
    bc_num_t r = bc_alloc(a->len > sc ? a->len : sc + 1, sc);
    if (!r.digs) {
        return r;
    }
    int borrow = 0;
    for (size_t i = 0; i < a->len; i++) {
        int da = (int)a->digs[i];
        int db = (i < b->len) ? (int)b->digs[i] : 0;
        int d = da - db - borrow;
        if (d < 0) {
            d += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r.digs[i] = (uint8_t)d;
    }
    bc_norm(&r);
    return r;
}

bc_num_t bc_add(const bc_num_t *a, const bc_num_t *b) {
    size_t sc = (a->scale > b->scale) ? a->scale : b->scale;
    bc_num_t ra = bc_rescale(a, sc);
    bc_num_t rb = bc_rescale(b, sc);
    bc_num_t r;
    if (ra.neg == rb.neg) {
        r = add_mag(&ra, &rb, sc);
        r.neg = ra.neg;
    } else {
        int c = cmp_mag_aligned(&ra, &rb);
        if (c >= 0) {
            r = sub_mag(&ra, &rb, sc);
            r.neg = ra.neg;
        } else {
            r = sub_mag(&rb, &ra, sc);
            r.neg = rb.neg;
        }
    }
    bc_free(&ra);
    bc_free(&rb);
    bc_norm(&r);
    return r;
}

bc_num_t bc_sub(const bc_num_t *a, const bc_num_t *b) {
    bc_num_t nb = bc_copy(b);
    nb.neg = !nb.neg;
    bc_norm(&nb);
    bc_num_t r = bc_add(a, &nb);
    bc_free(&nb);
    return r;
}

bc_num_t bc_mul(const bc_num_t *a, const bc_num_t *b, size_t scale) {
    size_t raw_sc = a->scale + b->scale;
    size_t m = (a->scale > b->scale) ? a->scale : b->scale;
    size_t target = (scale > m) ? scale : m;
    if (target > raw_sc) {
        target = raw_sc;
    }
    bc_num_t prod = bc_alloc(a->len + b->len + 1, raw_sc);
    if (!prod.digs) {
        return prod;
    }
    for (size_t i = 0; i < a->len; i++) {
        unsigned int carry = 0;
        for (size_t j = 0; j < b->len; j++) {
            unsigned int cur =
                (unsigned int)prod.digs[i + j] +
                (unsigned int)a->digs[i] * (unsigned int)b->digs[j] + carry;
            prod.digs[i + j] = (uint8_t)(cur % 10u);
            carry = cur / 10u;
        }
        size_t k = i + b->len;
        while (carry > 0 && k < prod.len) {
            unsigned int cur = (unsigned int)prod.digs[k] + carry;
            prod.digs[k] = (uint8_t)(cur % 10u);
            carry = cur / 10u;
            k++;
        }
    }
    prod.neg = (a->neg != b->neg) ? 1 : 0;
    bc_norm(&prod);
    bc_num_t res = bc_rescale(&prod, target);
    bc_free(&prod);
    return res;
}

static bc_num_t div_int_mag(const bc_num_t *num, const bc_num_t *den) {
    /* Integer magnitude quotient floor(num / den), scale = 0 */
    size_t n = num->len;
    bc_num_t q = bc_alloc(n > 0 ? n : 1, 0);
    bc_num_t rem = bc_zero();
    bc_num_t ten = bc_from_long(10);
    for (size_t idx = n; idx > 0; idx--) {
        bc_num_t shifted = bc_mul(&rem, &ten, 0);
        bc_num_t dig = bc_from_long((long)num->digs[idx - 1]);
        bc_num_t nxt = add_mag(&shifted, &dig, 0);
        bc_free(&shifted);
        bc_free(&dig);
        bc_free(&rem);
        rem = nxt;

        uint8_t qd = 0;
        while (cmp_mag_aligned(&rem, den) >= 0) {
            bc_num_t sub = sub_mag(&rem, den, 0);
            bc_free(&rem);
            rem = sub;
            qd++;
        }
        q.digs[idx - 1] = qd;
    }
    bc_free(&rem);
    bc_free(&ten);
    bc_norm(&q);
    return q;
}

bc_num_t bc_div(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err) {
    if (bc_is_zero(b)) {
        (void)fprintf(stderr, "bc: divide by zero\n");
        if (err) {
            *err = 1;
        }
        return bc_zero();
    }
    /* Compute floor(|a| * 10^(scale + b->scale) / (|b| * 10^(a->scale))) */
    size_t num_shift = scale + b->scale;
    bc_num_t num = bc_alloc(a->len + num_shift + 1, 0);
    if (num.digs && a->digs) {
        (void)memcpy(num.digs + num_shift, a->digs, a->len);
    }
    bc_norm(&num);
    bc_num_t den = bc_alloc(b->len + a->scale + 1, 0);
    if (den.digs && b->digs) {
        (void)memcpy(den.digs + a->scale, b->digs, b->len);
    }
    bc_norm(&den);

    bc_num_t q = div_int_mag(&num, &den);
    bc_free(&num);
    bc_free(&den);
    if (q.len <= scale) {
        bc_num_t grown = bc_alloc(scale + 1, scale);
        if (grown.digs && q.digs) {
            (void)memcpy(grown.digs, q.digs, q.len);
        }
        bc_free(&q);
        q = grown;
    } else {
        q.scale = scale;
    }
    q.neg = (a->neg != b->neg) ? 1 : 0;
    bc_norm(&q);
    return q;
}

bc_num_t bc_mod(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err) {
    bc_num_t q = bc_div(a, b, scale, err);
    if (err && *err) {
        bc_free(&q);
        return bc_zero();
    }
    size_t msc = scale + b->scale;
    if (a->scale > msc) {
        msc = a->scale;
    }
    bc_num_t qb = bc_mul(&q, b, msc);
    bc_num_t rem = bc_sub(a, &qb);
    bc_free(&q);
    bc_free(&qb);
    return rem;
}

bc_num_t bc_pow(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err) {
    long exp_val = bc_to_long(b);
    int neg_exp = 0;
    unsigned long e = 0;
    if (exp_val < 0) {
        neg_exp = 1;
        e = (unsigned long)(-(exp_val + 1)) + 1ul;
    } else {
        e = (unsigned long)exp_val;
    }
    size_t work_sc = scale;
    if (!neg_exp) {
        size_t prod_sc = a->scale * (size_t)e;
        size_t lim = (scale > a->scale) ? scale : a->scale;
        work_sc = (prod_sc < lim) ? prod_sc : lim;
    }
    bc_num_t res = bc_from_long(1);
    bc_num_t base = bc_copy(a);
    while (e > 0) {
        if (e & 1ul) {
            bc_num_t t = bc_mul(&res, &base, work_sc + 4);
            bc_free(&res);
            res = t;
        }
        e >>= 1;
        if (e > 0) {
            bc_num_t t = bc_mul(&base, &base, work_sc + 4);
            bc_free(&base);
            base = t;
        }
    }
    bc_free(&base);
    if (neg_exp) {
        bc_num_t one = bc_from_long(1);
        bc_num_t inv = bc_div(&one, &res, scale, err);
        bc_free(&one);
        bc_free(&res);
        return inv;
    }
    bc_num_t out = bc_rescale(&res, work_sc);
    bc_free(&res);
    return out;
}

bc_num_t bc_sqrt(const bc_num_t *a, size_t scale, int *err) {
    if (a->neg && !bc_is_zero(a)) {
        (void)fprintf(stderr, "bc: square root of negative number\n");
        if (err) {
            *err = 1;
        }
        return bc_zero();
    }
    size_t rscale = (scale > a->scale) ? scale : a->scale;
    if (bc_is_zero(a)) {
        return bc_alloc(rscale + 1, rscale);
    }
    size_t wsc = rscale + 4;
    bc_num_t two = bc_from_long(2);
    bc_num_t x = bc_from_double(sqrt(bc_to_double(a)), wsc);
    if (bc_is_zero(&x)) {
        bc_free(&x);
        x = bc_from_long(1);
    }
    for (int iter = 0; iter < 64; iter++) {
        bc_num_t q = bc_div(a, &x, wsc, err);
        bc_num_t s = bc_add(&x, &q);
        bc_num_t nxt = bc_div(&s, &two, wsc, err);
        bc_free(&q);
        bc_free(&s);
        bc_num_t diff = bc_sub(&nxt, &x);
        diff.neg = 0;
        bc_free(&x);
        x = nxt;
        bc_num_t tol = bc_alloc(rscale + 2, rscale + 1);
        if (tol.digs) {
            tol.digs[0] = 1;
        }
        int done = (bc_cmp(&diff, &tol) <= 0);
        bc_free(&diff);
        bc_free(&tol);
        if (done) {
            break;
        }
    }
    bc_free(&two);
    bc_num_t out = bc_rescale(&x, rscale);
    bc_free(&x);
    return out;
}

bc_num_t bc_math_exp(const bc_num_t *x, size_t scale) {
    size_t wsc = scale + 6;
    double xd = bc_to_double(x);
    /* Range reduce: x = k * ln(2) is complex, so halve x until |x| < 0.5 */
    int halvings = 0;
    bc_num_t xr = bc_rescale(x, wsc);
    bc_num_t two = bc_from_long(2);
    while (fabs(xd) > 0.5 && halvings < 30) {
        bc_num_t h = bc_div(&xr, &two, wsc, NULL);
        bc_free(&xr);
        xr = h;
        xd *= 0.5;
        halvings++;
    }
    bc_free(&two);

    bc_num_t sum = bc_from_long(1);
    bc_num_t term = bc_from_long(1);
    for (long k = 1; k <= 200; k++) {
        bc_num_t t1 = bc_mul(&term, &xr, wsc);
        bc_num_t kv = bc_from_long(k);
        bc_num_t t2 = bc_div(&t1, &kv, wsc, NULL);
        bc_free(&t1);
        bc_free(&kv);
        bc_free(&term);
        term = t2;
        if (bc_is_zero(&term)) {
            break;
        }
        bc_num_t ns = bc_add(&sum, &term);
        bc_free(&sum);
        sum = ns;
    }
    bc_free(&term);
    bc_free(&xr);
    for (int i = 0; i < halvings; i++) {
        bc_num_t sq = bc_mul(&sum, &sum, wsc);
        bc_free(&sum);
        sum = sq;
    }
    bc_num_t out = bc_rescale(&sum, scale);
    bc_free(&sum);
    return out;
}

bc_num_t bc_math_ln(const bc_num_t *x, size_t scale, int *err) {
    if (x->neg || bc_is_zero(x)) {
        (void)fprintf(stderr, "bc: ln of non-positive number\n");
        if (err) {
            *err = 1;
        }
        return bc_zero();
    }
    size_t wsc = scale + 6;
    bc_num_t cur = bc_rescale(x, wsc);
    int sqrts = 0;
    while ((bc_to_double(&cur) > 1.5 || bc_to_double(&cur) < 0.7) &&
           sqrts < 20) {
        bc_num_t r = bc_sqrt(&cur, wsc, err);
        bc_free(&cur);
        cur = r;
        sqrts++;
    }
    bc_num_t one = bc_from_long(1);
    bc_num_t num = bc_sub(&cur, &one);
    bc_num_t den = bc_add(&cur, &one);
    bc_num_t y = bc_div(&num, &den, wsc, NULL);
    bc_num_t y2 = bc_mul(&y, &y, wsc);
    bc_free(&cur);
    bc_free(&one);
    bc_free(&num);
    bc_free(&den);

    bc_num_t sum = bc_copy(&y);
    bc_num_t pwr = bc_copy(&y);
    for (long k = 3; k <= 300; k += 2) {
        bc_num_t np = bc_mul(&pwr, &y2, wsc);
        bc_free(&pwr);
        pwr = np;
        bc_num_t kv = bc_from_long(k);
        bc_num_t term = bc_div(&pwr, &kv, wsc, NULL);
        bc_free(&kv);
        if (bc_is_zero(&term)) {
            bc_free(&term);
            break;
        }
        bc_num_t ns = bc_add(&sum, &term);
        bc_free(&sum);
        bc_free(&term);
        sum = ns;
    }
    bc_free(&y);
    bc_free(&y2);
    bc_free(&pwr);
    bc_num_t mult = bc_from_long(2L << sqrts);
    bc_num_t full = bc_mul(&sum, &mult, wsc);
    bc_free(&sum);
    bc_free(&mult);
    bc_num_t out = bc_rescale(&full, scale);
    bc_free(&full);
    return out;
}

bc_num_t bc_math_atan(const bc_num_t *x, size_t scale) {
    size_t wsc = scale + 6;
    int neg = x->neg;
    bc_num_t cur = bc_rescale(x, wsc);
    cur.neg = 0;
    /* Half-angle identity: atan(x) = 2 * atan(x / (1 + sqrt(1 + x^2))) */
    int halves = 0;
    bc_num_t one = bc_from_long(1);
    while (bc_to_double(&cur) > 0.2 && halves < 12) {
        bc_num_t x2 = bc_mul(&cur, &cur, wsc);
        bc_num_t opx2 = bc_add(&one, &x2);
        bc_num_t sr = bc_sqrt(&opx2, wsc, NULL);
        bc_num_t den = bc_add(&one, &sr);
        bc_num_t nxt = bc_div(&cur, &den, wsc, NULL);
        bc_free(&x2);
        bc_free(&opx2);
        bc_free(&sr);
        bc_free(&den);
        bc_free(&cur);
        cur = nxt;
        halves++;
    }
    bc_free(&one);
    bc_num_t x2 = bc_mul(&cur, &cur, wsc);
    bc_num_t sum = bc_copy(&cur);
    bc_num_t pwr = bc_copy(&cur);
    int sign = -1;
    for (long k = 3; k <= 300; k += 2) {
        bc_num_t np = bc_mul(&pwr, &x2, wsc);
        bc_free(&pwr);
        pwr = np;
        bc_num_t kv = bc_from_long(k);
        bc_num_t term = bc_div(&pwr, &kv, wsc, NULL);
        bc_free(&kv);
        if (bc_is_zero(&term)) {
            bc_free(&term);
            break;
        }
        term.neg = (sign < 0) ? 1 : 0;
        bc_num_t ns = bc_add(&sum, &term);
        bc_free(&sum);
        bc_free(&term);
        sum = ns;
        sign = -sign;
    }
    bc_free(&cur);
    bc_free(&x2);
    bc_free(&pwr);
    bc_num_t mult = bc_from_long(1L << halves);
    bc_num_t full = bc_mul(&sum, &mult, wsc);
    bc_free(&sum);
    bc_free(&mult);
    full.neg = neg;
    bc_num_t out = bc_rescale(&full, scale);
    bc_free(&full);
    return out;
}

bc_num_t bc_math_sin(const bc_num_t *x, size_t scale) {
    size_t wsc = scale + 6;
    bc_num_t x2 = bc_mul(x, x, wsc);
    bc_num_t sum = bc_rescale(x, wsc);
    bc_num_t term = bc_rescale(x, wsc);
    for (long k = 1; k <= 120; k++) {
        bc_num_t t1 = bc_mul(&term, &x2, wsc);
        bc_num_t den = bc_from_long((2 * k) * (2 * k + 1));
        bc_num_t t2 = bc_div(&t1, &den, wsc, NULL);
        bc_free(&t1);
        bc_free(&den);
        bc_free(&term);
        t2.neg = !t2.neg;
        term = t2;
        if (bc_is_zero(&term)) {
            break;
        }
        bc_num_t ns = bc_add(&sum, &term);
        bc_free(&sum);
        sum = ns;
    }
    bc_free(&x2);
    bc_free(&term);
    bc_num_t out = bc_rescale(&sum, scale);
    bc_free(&sum);
    return out;
}

bc_num_t bc_math_cos(const bc_num_t *x, size_t scale) {
    size_t wsc = scale + 6;
    bc_num_t x2 = bc_mul(x, x, wsc);
    bc_num_t sum = bc_from_long(1);
    bc_num_t term = bc_from_long(1);
    for (long k = 1; k <= 120; k++) {
        bc_num_t t1 = bc_mul(&term, &x2, wsc);
        bc_num_t den = bc_from_long((2 * k - 1) * (2 * k));
        bc_num_t t2 = bc_div(&t1, &den, wsc, NULL);
        bc_free(&t1);
        bc_free(&den);
        bc_free(&term);
        t2.neg = !t2.neg;
        term = t2;
        if (bc_is_zero(&term)) {
            break;
        }
        bc_num_t ns = bc_add(&sum, &term);
        bc_free(&sum);
        sum = ns;
    }
    bc_free(&x2);
    bc_free(&term);
    bc_num_t out = bc_rescale(&sum, scale);
    bc_free(&sum);
    return out;
}

static int digit_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

bc_num_t bc_from_str(const char *s, long ibase) {
    int neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    const char *dot = strchr(s, '.');
    size_t int_len = dot ? (size_t)(dot - s) : strlen(s);
    size_t frac_len = dot ? strlen(dot + 1) : 0;

    if (ibase == 10) {
        size_t total = int_len + frac_len;
        if (total == 0) {
            return bc_zero();
        }
        bc_num_t n = bc_alloc(total, frac_len);
        if (!n.digs) {
            return n;
        }
        size_t pos = 0;
        for (size_t i = frac_len; i > 0; i--) {
            int d = digit_val(dot[i]);
            n.digs[pos++] = (uint8_t)(d >= 0 ? d : 0);
        }
        for (size_t i = int_len; i > 0; i--) {
            int d = digit_val(s[i - 1]);
            n.digs[pos++] = (uint8_t)(d >= 0 ? d : 0);
        }
        n.neg = neg;
        bc_norm(&n);
        return n;
    }

    bc_num_t base = bc_from_long(ibase);
    bc_num_t acc = bc_zero();
    for (size_t i = 0; i < int_len; i++) {
        int d = digit_val(s[i]);
        if (d < 0) {
            d = 0;
        }
        if (d >= (int)ibase) {
            d = (int)ibase - 1;
        }
        bc_num_t t = bc_mul(&acc, &base, 0);
        bc_num_t dv = bc_from_long(d);
        bc_num_t s2 = bc_add(&t, &dv);
        bc_free(&t);
        bc_free(&dv);
        bc_free(&acc);
        acc = s2;
    }
    if (frac_len > 0) {
        bc_num_t fnum = bc_zero();
        bc_num_t fden = bc_from_long(1);
        for (size_t i = 0; i < frac_len; i++) {
            int d = digit_val(dot[1 + i]);
            if (d < 0) {
                d = 0;
            }
            if (d >= (int)ibase) {
                d = (int)ibase - 1;
            }
            bc_num_t t = bc_mul(&fnum, &base, 0);
            bc_num_t dv = bc_from_long(d);
            bc_num_t s2 = bc_add(&t, &dv);
            bc_free(&t);
            bc_free(&dv);
            bc_free(&fnum);
            fnum = s2;
            bc_num_t fd = bc_mul(&fden, &base, 0);
            bc_free(&fden);
            fden = fd;
        }
        bc_num_t frac = bc_div(&fnum, &fden, frac_len, NULL);
        bc_num_t sum = bc_add(&acc, &frac);
        bc_free(&fnum);
        bc_free(&fden);
        bc_free(&frac);
        bc_free(&acc);
        acc = sum;
    }
    bc_free(&base);
    acc.neg = neg;
    bc_norm(&acc);
    return acc;
}

static void emit_wrap_char(char c, int *col) {
    if (*col >= 69) {
        (void)putchar('\\');
        (void)putchar('\n');
        *col = 0;
    }
    (void)putchar(c);
    (*col)++;
}

void bc_print_num(const bc_num_t *a, long obase) {
    int col = 0;
    if (a->neg && !bc_is_zero(a)) {
        emit_wrap_char('-', &col);
    }
    if (obase == 10) {
        size_t top = a->len;
        while (top > a->scale && a->digs[top - 1] == 0) {
            top--;
        }
        if (top == a->scale) {
            if (a->scale == 0) {
                emit_wrap_char('0', &col);
            }
        } else {
            for (size_t i = top; i > a->scale; i--) {
                emit_wrap_char((char)('0' + a->digs[i - 1]), &col);
            }
        }
        if (a->scale > 0) {
            emit_wrap_char('.', &col);
            for (size_t i = a->scale; i > 0; i--) {
                emit_wrap_char((char)('0' + a->digs[i - 1]), &col);
            }
        }
        (void)putchar('\n');
        return;
    }

    /* Non-base-10 conversion for integer part */
    bc_num_t ip = bc_rescale(a, 0);
    ip.neg = 0;
    bc_num_t b = bc_from_long(obase);
    long *digits = NULL;
    size_t nd = 0;
    size_t cap = 0;
    if (bc_is_zero(&ip)) {
        digits = calloc(1, sizeof(*digits));
        if (digits) {
            digits[0] = 0;
            nd = 1;
        }
    } else {
        while (!bc_is_zero(&ip)) {
            bc_num_t q = bc_div(&ip, &b, 0, NULL);
            bc_num_t qb = bc_mul(&q, &b, 0);
            bc_num_t r = bc_sub(&ip, &qb);
            bc_free(&qb);
            bc_free(&ip);
            ip = q;
            if (nd == cap) {
                size_t nc = (cap == 0) ? 32 : cap * 2;
                long *ndg = realloc(digits, nc * sizeof(*ndg));
                if (!ndg) {
                    bc_free(&r);
                    break;
                }
                digits = ndg;
                cap = nc;
            }
            digits[nd++] = bc_to_long(&r);
            bc_free(&r);
        }
    }
    bc_free(&ip);
    bc_free(&b);
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = nd; i > 0; i--) {
        if (obase <= 16) {
            emit_wrap_char(hex[digits[i - 1]], &col);
        } else {
            char grp[32];
            (void)snprintf(grp, sizeof(grp), " %ld", digits[i - 1]);
            for (const char *p = grp; *p != '\0'; p++) {
                emit_wrap_char(*p, &col);
            }
        }
    }
    free(digits);
    (void)putchar('\n');
}
