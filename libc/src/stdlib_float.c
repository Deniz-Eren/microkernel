/**
 * @file     stdlib_float.c
 * @brief    POSIX.1-2024 IEEE-754 strtod, strtof, strtold, and atof parsers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0009 */

#define BIGINT_CAP 128

struct bigint {
    uint32_t w[BIGINT_CAP];
    int len;
};

static void bi_zero(struct bigint *a) {
    a->len = 0;
}

static void bi_trim(struct bigint *a) {
    while (a->len > 0 && a->w[a->len - 1] == 0u) {
        a->len--;
    }
}

static void bi_set_u32(struct bigint *a, uint32_t v) {
    if (v == 0u) {
        a->len = 0;
    } else {
        a->w[0] = v;
        a->len = 1;
    }
}

static void bi_mul_add(struct bigint *a, uint32_t mul, uint32_t add) {
    uint64_t carry = add;
    for (int i = 0; i < a->len; i++) {
        uint64_t prod = (uint64_t)a->w[i] * (uint64_t)mul + carry;
        a->w[i] = (uint32_t)prod;
        carry = prod >> 32;
    }
    if (carry != 0ULL && a->len < BIGINT_CAP) {
        a->w[a->len++] = (uint32_t)carry;
    }
}

static void bi_mul_pow5(struct bigint *a, int e) {
    static const uint32_t k_p5[14] = {
        1u,     5u,      25u,      125u,     625u,      3125u,      15625u,
        78125u, 390625u, 1953125u, 9765625u, 48828125u, 244140625u, 1220703125u,
    };
    while (e > 0) {
        int step = e > 13 ? 13 : e;
        bi_mul_add(a, k_p5[step], 0u);
        e -= step;
    }
}

static void bi_shl(struct bigint *a, int shift) {
    if (a->len == 0 || shift <= 0) {
        return;
    }
    int ws = shift / 32;
    int bs = shift % 32;
    int old_len = a->len;
    int new_len = old_len + ws + (bs > 0 ? 1 : 0);
    if (new_len > BIGINT_CAP) {
        new_len = BIGINT_CAP;
    }
    for (int i = new_len - 1; i >= 0; i--) {
        uint32_t hi = (i - ws >= 0 && i - ws < old_len) ? a->w[i - ws] : 0u;
        uint32_t lo = (bs > 0 && i - ws - 1 >= 0 && i - ws - 1 < old_len)
                          ? a->w[i - ws - 1]
                          : 0u;
        a->w[i] = (bs > 0) ? ((hi << bs) | (lo >> (32 - bs))) : hi;
    }
    a->len = new_len;
    bi_trim(a);
}

static int bi_bitlen(const struct bigint *a) {
    if (a->len == 0) {
        return 0;
    }
    uint32_t top = a->w[a->len - 1];
    int bits = (a->len - 1) * 32;
    while (top != 0u) {
        bits++;
        top >>= 1;
    }
    return bits;
}

static int bi_cmp(const struct bigint *a, const struct bigint *b) {
    if (a->len != b->len) {
        return a->len < b->len ? -1 : 1;
    }
    for (int i = a->len - 1; i >= 0; i--) {
        if (a->w[i] != b->w[i]) {
            return a->w[i] < b->w[i] ? -1 : 1;
        }
    }
    return 0;
}

static void bi_sub(struct bigint *a, const struct bigint *b) {
    uint64_t borrow = 0ULL;
    for (int i = 0; i < a->len; i++) {
        uint64_t av = a->w[i];
        uint64_t bv = (i < b->len) ? (uint64_t)b->w[i] : 0ULL;
        uint64_t sub = bv + borrow;
        if (av < sub) {
            a->w[i] = (uint32_t)((1ULL << 32) + av - sub);
            borrow = 1ULL;
        } else {
            a->w[i] = (uint32_t)(av - sub);
            borrow = 0ULL;
        }
    }
    bi_trim(a);
}

static uint64_t bi_extract_top(const struct bigint *a, int top_bits,
                               int *round_bit, int *sticky_bit) {
    int bl = bi_bitlen(a);
    int shift = bl - top_bits;
    if (shift <= 0) {
        uint64_t v = 0ULL;
        for (int i = a->len - 1; i >= 0; i--) {
            v = (v << 32) | a->w[i];
        }
        *round_bit = 0;
        *sticky_bit = 0;
        return v << (-shift);
    }

    uint64_t res = 0ULL;
    for (int k = top_bits - 1; k >= 0; k--) {
        int bit_idx = shift + k;
        uint32_t bit = (a->w[bit_idx / 32] >> (bit_idx % 32)) & 1u;
        res = (res << 1) | bit;
    }

    int ridx = shift - 1;
    *round_bit = (int)((a->w[ridx / 32] >> (ridx % 32)) & 1u);
    int sticky = 0;
    for (int i = 0; i < ridx / 32; i++) {
        if (a->w[i] != 0u) {
            sticky = 1;
            break;
        }
    }
    if (!sticky && (ridx % 32) > 0) {
        uint32_t mask = (1u << (ridx % 32)) - 1u;
        if ((a->w[ridx / 32] & mask) != 0u) {
            sticky = 1;
        }
    }
    *sticky_bit = sticky;
    return res;
}

static int ci_match(const char *s, const char *lit) {
    while (*lit) {
        if (tolower((unsigned char)*s) != *lit) {
            return 0;
        }
        s++;
        lit++;
    }
    return 1;
}

static long double scale_result(uint64_t sig, int exp2, int neg, int prec,
                                int emin, int emax, int sticky_in) {
    if (sig == 0ULL) {
        return neg ? -0.0L : 0.0L;
    }

    /* Normalize sig to prec bits [2^(prec-1), 2^prec). */
    int round_bit = 0;
    int sticky = sticky_in;

    if (exp2 < emin) {
        int shift = emin - exp2;
        if (shift > prec + 1) {
            errno = ERANGE;
            return neg ? -0.0L : 0.0L;
        }
        for (int i = 0; i < shift; i++) {
            sticky |= round_bit;
            round_bit = (int)(sig & 1ULL);
            sig >>= 1;
        }
        exp2 = emin;
        if (round_bit && (sticky || (sig & 1ULL))) {
            sig++;
        }
        if (sig == 0ULL) {
            errno = ERANGE;
            return neg ? -0.0L : 0.0L;
        }
        if (prec < 64 && sig == (1ULL << prec)) {
            sig >>= 1;
            exp2++;
        }
        if (exp2 == emin && prec < 64 && sig < (1ULL << (prec - 1))) {
            errno = ERANGE;
        }
    }

    if (exp2 > emax) {
        errno = ERANGE;
        return neg ? -HUGE_VALL : HUGE_VALL;
    }

    long double v = ldexpl((long double)sig, exp2 - (prec - 1));
    if (!isfinite(v)) {
        errno = ERANGE;
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    return neg ? -v : v;
}

static long double parse_hex_float(const char *s, const char **end_out, int neg,
                                   int prec, int emin, int emax) {
    uint64_t sig = 0ULL;
    int sig_bits = 0;
    int round_bit = 0;
    int sticky = 0;
    int seen_dot = 0;
    int any_digit = 0;
    long long exp2 = 0;

    while (*s) {
        int d = -1;
        if (*s >= '0' && *s <= '9') {
            d = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            d = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            d = *s - 'A' + 10;
        } else if (*s == '.' && !seen_dot) {
            seen_dot = 1;
            s++;
            continue;
        } else {
            break;
        }
        any_digit = 1;
        if (sig == 0ULL && d == 0) {
            if (seen_dot) {
                exp2 -= 4;
            }
            s++;
            continue;
        }
        for (int b = 3; b >= 0; b--) {
            int bit = (d >> b) & 1;
            if (sig_bits == 0 && bit == 0) {
                if (seen_dot) {
                    exp2--;
                }
                continue;
            }
            if (sig_bits < prec) {
                sig = (sig << 1) | (uint64_t)bit;
                sig_bits++;
                if (!seen_dot) {
                    exp2++;
                }
            } else if (sig_bits == prec) {
                round_bit = bit;
                sig_bits++;
                if (!seen_dot) {
                    exp2++;
                }
            } else {
                sticky |= bit;
                if (!seen_dot) {
                    exp2++;
                }
            }
        }
        s++;
    }

    if (!any_digit) {
        *end_out = 0;
        return 0.0L;
    }

    const char *after_sig = s;
    if (*s == 'p' || *s == 'P') {
        const char *p = s + 1;
        int eneg = 0;
        if (*p == '-') {
            eneg = 1;
            p++;
        } else if (*p == '+') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            long long pe = 0;
            while (*p >= '0' && *p <= '9') {
                if (pe < 1000000LL) {
                    pe = pe * 10LL + (*p - '0');
                }
                p++;
            }
            exp2 += eneg ? -pe : pe;
            after_sig = p;
        }
    }
    *end_out = after_sig;

    if (sig == 0ULL) {
        return neg ? -0.0L : 0.0L;
    }

    if (sig_bits < prec) {
        sig <<= (prec - sig_bits);
    }
    /* exp2 is currently position of MSB + 1 when !seen_dot adjusted;
     * compute exact unbiased exponent of MSB. */
    long long msb_exp = exp2 - 1;

    if (msb_exp >= emin) {
        if (round_bit && (sticky || (sig & 1ULL))) {
            sig++;
            if ((prec < 64 && sig == (1ULL << prec)) ||
                (prec == 64 && sig == 0ULL)) {
                sig = (prec == 64) ? (1ULL << 63) : (sig >> 1);
                msb_exp++;
            }
        }
    }

    if (msb_exp > emax) {
        errno = ERANGE;
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    if (msb_exp < emin - prec - 2) {
        errno = ERANGE;
        return neg ? -0.0L : 0.0L;
    }
    return scale_result(sig, (int)msb_exp, neg, prec, emin, emax,
                        round_bit | sticky);
}

static long double parse_dec_bigint(const char *digits, int ndig, int exp10,
                                    int sticky_dig, int neg, int prec, int emin,
                                    int emax) {
    struct bigint num;
    bi_zero(&num);
    for (int i = 0; i < ndig; i++) {
        bi_mul_add(&num, 10u, (uint32_t)(digits[i] - '0'));
    }

    uint64_t sig = 0ULL;
    int exp2 = 0;
    int round_bit = 0;
    int sticky = sticky_dig;

    if (exp10 >= 0) {
        bi_mul_pow5(&num, exp10);
        int bl = bi_bitlen(&num);
        exp2 = bl - 1 + exp10;
        if (bl <= prec) {
            sig = bi_extract_top(&num, prec, &round_bit, &sticky);
            sticky |= sticky_dig;
        } else {
            int rb = 0;
            int sb = 0;
            sig = bi_extract_top(&num, prec, &rb, &sb);
            round_bit = rb;
            sticky = sb | sticky_dig;
        }
    } else {
        struct bigint den;
        bi_set_u32(&den, 1u);
        bi_mul_pow5(&den, -exp10);

        int nbits = bi_bitlen(&num);
        int dbits = bi_bitlen(&den);
        int k = dbits - nbits;
        if (k > 0) {
            bi_shl(&num, k);
        } else if (k < 0) {
            bi_shl(&den, -k);
        }
        int msb_exp = exp10 - k;
        if (bi_cmp(&num, &den) < 0) {
            bi_shl(&num, 1);
            msb_exp--;
        }

        int qbits = prec;
        if (msb_exp < emin) {
            int sub_drop = emin - msb_exp;
            if (sub_drop > prec + 1) {
                errno = ERANGE;
                return neg ? -0.0L : 0.0L;
            }
            qbits = prec - sub_drop;
            if (qbits < 0) {
                qbits = 0;
            }
        }

        sig = 0ULL;
        for (int i = 0; i < qbits; i++) {
            sig <<= 1;
            if (bi_cmp(&num, &den) >= 0) {
                bi_sub(&num, &den);
                sig |= 1ULL;
            }
            bi_shl(&num, 1);
        }
        /* Next bit is round_bit. */
        if (bi_cmp(&num, &den) >= 0) {
            bi_sub(&num, &den);
            round_bit = 1;
        }
        if (num.len > 0 || sticky_dig) {
            sticky = 1;
        }

        if (msb_exp < emin) {
            if (round_bit && (sticky || (sig & 1ULL))) {
                sig++;
            }
            if (sig == 0ULL) {
                errno = ERANGE;
                return neg ? -0.0L : 0.0L;
            }
            long double v = ldexpl((long double)sig, emin - (prec - 1));
            if (prec < 64 && sig < (1ULL << (prec - 1))) {
                errno = ERANGE;
            }
            return neg ? -v : v;
        }
        exp2 = msb_exp;
    }

    if (exp2 >= emin) {
        if (round_bit && (sticky || (sig & 1ULL))) {
            sig++;
            if ((prec < 64 && sig == (1ULL << prec)) ||
                (prec == 64 && sig == 0ULL)) {
                sig = (prec == 64) ? (1ULL << 63) : (sig >> 1);
                exp2++;
            }
        }
    }

    return scale_result(sig, exp2, neg, prec, emin, emax, round_bit | sticky);
}

static long double parse_dec_ldbl_wide(const char *digits, int ndig, int exp10,
                                       int neg) {
    long double v = 0.0L;
    int lim = ndig > 24 ? 24 : ndig;
    for (int i = 0; i < lim; i++) {
        v = v * 10.0L + (long double)(digits[i] - '0');
    }
    int e = exp10 + (ndig - lim);
    int ae = e < 0 ? -e : e;
    long double p10 = 1.0L;
    long double base = 10.0L;
    while (ae > 0) {
        if (ae & 1) {
            p10 *= base;
        }
        ae >>= 1;
        if (ae > 0) {
            base *= base;
        }
    }
    v = (e >= 0) ? (v * p10) : (v / p10);
    if (!isfinite(v)) {
        errno = ERANGE;
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    if (v == 0.0L) {
        errno = ERANGE;
    }
    return neg ? -v : v;
}

static long double parse_float_core(const char *nptr, char **endptr, int prec,
                                    int emin, int emax) {
    const char *s = nptr;
    while (isspace((unsigned char)*s)) {
        s++;
    }

    int neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (ci_match(s, "infinity")) {
        if (endptr) {
            *endptr = (char *)(s + 8);
        }
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    if (ci_match(s, "inf")) {
        if (endptr) {
            *endptr = (char *)(s + 3);
        }
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    if (ci_match(s, "nan")) {
        const char *p = s + 3;
        if (*p == '(') {
            const char *q = p + 1;
            while (isalnum((unsigned char)*q) || *q == '_') {
                q++;
            }
            if (*q == ')') {
                p = q + 1;
            }
        }
        if (endptr) {
            *endptr = (char *)p;
        }
        long double nv = (long double)NAN;
        return neg ? -nv : nv;
    }

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        const char *hex_end = 0;
        long double hv =
            parse_hex_float(s + 2, &hex_end, neg, prec, emin, emax);
        if (hex_end != 0) {
            if (endptr) {
                *endptr = (char *)hex_end;
            }
            return hv;
        }
        /* "0x" with no following hex digit parses as "0" followed by "x". */
        if (endptr) {
            *endptr = (char *)(s + 1);
        }
        return neg ? -0.0L : 0.0L;
    }

    char digits[800];
    int ndig = 0;
    int sticky_dig = 0;
    int seen_dot = 0;
    int any_digit = 0;
    long long exp10 = 0;

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            any_digit = 1;
            if (ndig == 0 && *s == '0') {
                if (seen_dot) {
                    exp10--;
                }
            } else if (ndig < (int)sizeof(digits)) {
                digits[ndig++] = *s;
                if (seen_dot) {
                    exp10--;
                }
            } else {
                if (*s != '0') {
                    sticky_dig = 1;
                }
                if (!seen_dot) {
                    exp10++;
                }
            }
            s++;
        } else if (*s == '.' && !seen_dot) {
            seen_dot = 1;
            s++;
        } else {
            break;
        }
    }

    if (!any_digit) {
        if (endptr) {
            *endptr = (char *)nptr;
        }
        return 0.0L;
    }

    const char *after_mant = s;
    if (*s == 'e' || *s == 'E') {
        const char *p = s + 1;
        int eneg = 0;
        if (*p == '-') {
            eneg = 1;
            p++;
        } else if (*p == '+') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            long long pe = 0;
            while (*p >= '0' && *p <= '9') {
                if (pe < 1000000LL) {
                    pe = pe * 10LL + (*p - '0');
                }
                p++;
            }
            exp10 += eneg ? -pe : pe;
            after_mant = p;
        }
    }
    if (endptr) {
        *endptr = (char *)after_mant;
    }

    if (ndig == 0) {
        return neg ? -0.0L : 0.0L;
    }

    while (ndig > 1 && digits[ndig - 1] == '0') {
        ndig--;
        exp10++;
    }

    long long total_exp = exp10 + ndig - 1;
    int max_dec = (prec == 24) ? 45 : (prec == 53 ? 320 : 4950);
    int min_dec = (prec == 24) ? -55 : (prec == 53 ? -340 : -4965);
    if (total_exp > max_dec) {
        errno = ERANGE;
        return neg ? -HUGE_VALL : HUGE_VALL;
    }
    if (total_exp < min_dec) {
        errno = ERANGE;
        return neg ? -0.0L : 0.0L;
    }

    if (exp10 < -350 || exp10 > 350) {
        return parse_dec_ldbl_wide(digits, ndig, (int)exp10, neg);
    }
    return parse_dec_bigint(digits, ndig, (int)exp10, sticky_dig, neg, prec,
                            emin, emax);
}

double strtod(const char *restrict nptr, char **restrict endptr) {
    long double v = parse_float_core(nptr, endptr, DBL_MANT_DIG,
                                     DBL_MIN_EXP - 1, DBL_MAX_EXP - 1);
    double d = (double)v;
    if (isinf(v)) {
        return signbit(v) ? -HUGE_VAL : HUGE_VAL;
    }
    return d;
}

float strtof(const char *restrict nptr, char **restrict endptr) {
    long double v = parse_float_core(nptr, endptr, FLT_MANT_DIG,
                                     FLT_MIN_EXP - 1, FLT_MAX_EXP - 1);
    float f = (float)v;
    if (isinf(v)) {
        return signbit(v) ? -HUGE_VALF : HUGE_VALF;
    }
    return f;
}

long double strtold(const char *restrict nptr, char **restrict endptr) {
    return parse_float_core(nptr, endptr, LDBL_MANT_DIG, LDBL_MIN_EXP - 1,
                            LDBL_MAX_EXP - 1);
}

double atof(const char *str) {
    return strtod(str, (char **)0);
}
