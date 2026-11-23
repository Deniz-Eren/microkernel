/**
 * @file     bc.h
 * @brief    Internal types and interfaces for Ring 3 POSIX.1-2024 XCU bc
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef BC_H
#define BC_H

#include <stddef.h>
#include <stdint.h>

/* need:: COMP_LIBC_0025 */

typedef struct {
    int neg;
    size_t scale;
    size_t len;
    size_t cap;
    uint8_t *digs; /* LSB-first base-10 digits */
} bc_num_t;

typedef struct {
    char *name;
    bc_num_t val;
    bc_num_t *arr;
    size_t arr_len;
    size_t arr_cap;
} bc_var_t;

typedef struct {
    char *name;
    char **params;
    int *param_is_arr;
    size_t nparams;
    char **autos;
    int *auto_is_arr;
    size_t nautos;
    char *body_src;
} bc_func_t;

typedef enum {
    BC_FLOW_NONE = 0,
    BC_FLOW_BREAK,
    BC_FLOW_CONTINUE,
    BC_FLOW_RETURN,
    BC_FLOW_QUIT
} bc_flow_t;

typedef struct {
    long scale;
    long ibase;
    long obase;
    int mathlib;
    int err;
    bc_flow_t flow;
    bc_num_t ret_val;
    bc_var_t *vars;
    size_t nvars;
    size_t cap_vars;
    bc_func_t *funcs;
    size_t nfuncs;
    size_t cap_funcs;
} bc_vm_t;

/* Arbitrary-precision decimal numbers (num.c) */
bc_num_t bc_zero(void);
bc_num_t bc_from_long(long v);
bc_num_t bc_from_str(const char *s, long ibase);
bc_num_t bc_copy(const bc_num_t *a);
void bc_free(bc_num_t *a);
int bc_is_zero(const bc_num_t *a);
long bc_to_long(const bc_num_t *a);
double bc_to_double(const bc_num_t *a);
bc_num_t bc_from_double(double d, size_t scale);
size_t bc_num_length(const bc_num_t *a);
int bc_cmp(const bc_num_t *a, const bc_num_t *b);
bc_num_t bc_rescale(const bc_num_t *a, size_t new_scale);
bc_num_t bc_add(const bc_num_t *a, const bc_num_t *b);
bc_num_t bc_sub(const bc_num_t *a, const bc_num_t *b);
bc_num_t bc_mul(const bc_num_t *a, const bc_num_t *b, size_t scale);
bc_num_t bc_div(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err);
bc_num_t bc_mod(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err);
bc_num_t bc_pow(const bc_num_t *a, const bc_num_t *b, size_t scale, int *err);
bc_num_t bc_sqrt(const bc_num_t *a, size_t scale, int *err);
bc_num_t bc_math_exp(const bc_num_t *x, size_t scale);
bc_num_t bc_math_ln(const bc_num_t *x, size_t scale, int *err);
bc_num_t bc_math_sin(const bc_num_t *x, size_t scale);
bc_num_t bc_math_cos(const bc_num_t *x, size_t scale);
bc_num_t bc_math_atan(const bc_num_t *x, size_t scale);
void bc_print_num(const bc_num_t *a, long obase);

/* Interpreter (eval.c) */
void bc_vm_init(bc_vm_t *vm, int mathlib);
void bc_vm_free(bc_vm_t *vm);
int bc_exec_source(bc_vm_t *vm, const char *src);

#endif /* BC_H */
