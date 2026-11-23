/**
 * @file     eval.c
 * @brief    Recursive-descent interpreter for Ring 3 POSIX.1-2024 XCU bc
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

typedef struct {
    const char *src;
    size_t pos;
    bc_vm_t *vm;
} bc_cur_t;

void bc_vm_init(bc_vm_t *vm, int mathlib) {
    (void)memset(vm, 0, sizeof(*vm));
    vm->scale = mathlib ? 20 : 0;
    vm->ibase = 10;
    vm->obase = 10;
    vm->mathlib = mathlib;
    vm->ret_val = bc_zero();
}

void bc_vm_free(bc_vm_t *vm) {
    for (size_t i = 0; i < vm->nvars; i++) {
        free(vm->vars[i].name);
        bc_free(&vm->vars[i].val);
        for (size_t j = 0; j < vm->vars[i].arr_len; j++) {
            bc_free(&vm->vars[i].arr[j]);
        }
        free(vm->vars[i].arr);
    }
    free(vm->vars);
    for (size_t i = 0; i < vm->nfuncs; i++) {
        free(vm->funcs[i].name);
        for (size_t j = 0; j < vm->funcs[i].nparams; j++) {
            free(vm->funcs[i].params[j]);
        }
        free(vm->funcs[i].params);
        free(vm->funcs[i].param_is_arr);
        for (size_t j = 0; j < vm->funcs[i].nautos; j++) {
            free(vm->funcs[i].autos[j]);
        }
        free(vm->funcs[i].autos);
        free(vm->funcs[i].auto_is_arr);
        free(vm->funcs[i].body_src);
    }
    free(vm->funcs);
    bc_free(&vm->ret_val);
    (void)memset(vm, 0, sizeof(*vm));
}

static bc_var_t *vm_var(bc_vm_t *vm, const char *name) {
    for (size_t i = 0; i < vm->nvars; i++) {
        if (strcmp(vm->vars[i].name, name) == 0) {
            return &vm->vars[i];
        }
    }
    if (vm->nvars == vm->cap_vars) {
        size_t nc = (vm->cap_vars == 0) ? 16 : vm->cap_vars * 2;
        bc_var_t *nv = realloc(vm->vars, nc * sizeof(*nv));
        if (!nv) {
            return NULL;
        }
        (void)memset(nv + vm->cap_vars, 0, (nc - vm->cap_vars) * sizeof(*nv));
        vm->vars = nv;
        vm->cap_vars = nc;
    }
    size_t idx = vm->nvars++;
    vm->vars[idx].name = strdup(name);
    vm->vars[idx].val = bc_zero();
    return &vm->vars[idx];
}

static bc_num_t *vm_arr_slot(bc_var_t *v, size_t idx) {
    if (!v) {
        return NULL;
    }
    if (idx >= v->arr_len) {
        size_t need = idx + 1;
        if (need > v->arr_cap) {
            size_t nc = (v->arr_cap == 0) ? 16 : v->arr_cap * 2;
            while (nc < need) {
                nc *= 2;
            }
            bc_num_t *na = realloc(v->arr, nc * sizeof(*na));
            if (!na) {
                return NULL;
            }
            (void)memset(na + v->arr_cap, 0, (nc - v->arr_cap) * sizeof(*na));
            v->arr = na;
            v->arr_cap = nc;
        }
        for (size_t i = v->arr_len; i < need; i++) {
            v->arr[i] = bc_zero();
        }
        v->arr_len = need;
    }
    return &v->arr[idx];
}

static void skip_ws(bc_cur_t *c, int skip_nl) {
    const char *s = c->src;
    for (;;) {
        char ch = s[c->pos];
        if (ch == ' ' || ch == '\t' || ch == '\r' || (skip_nl && ch == '\n')) {
            c->pos++;
            continue;
        }
        if (ch == '\\' && s[c->pos + 1] == '\n') {
            c->pos += 2;
            continue;
        }
        if (ch == '/' && s[c->pos + 1] == '*') {
            c->pos += 2;
            while (s[c->pos] != '\0' &&
                   !(s[c->pos] == '*' && s[c->pos + 1] == '/')) {
                c->pos++;
            }
            if (s[c->pos] == '*') {
                c->pos += 2;
            }
            continue;
        }
        break;
    }
}

static int match_kw(bc_cur_t *c, const char *kw) {
    skip_ws(c, 0);
    size_t l = strlen(kw);
    if (strncmp(c->src + c->pos, kw, l) == 0 &&
        !(isalnum((unsigned char)c->src[c->pos + l]) ||
          c->src[c->pos + l] == '_')) {
        c->pos += l;
        return 1;
    }
    return 0;
}

static int parse_ident(bc_cur_t *c, char *buf, size_t cap) {
    skip_ws(c, 0);
    if (!islower((unsigned char)c->src[c->pos])) {
        return 0;
    }
    size_t n = 0;
    while ((islower((unsigned char)c->src[c->pos]) ||
            isdigit((unsigned char)c->src[c->pos]) || c->src[c->pos] == '_') &&
           n + 1 < cap) {
        buf[n++] = c->src[c->pos++];
    }
    buf[n] = '\0';
    return 1;
}

static bc_num_t eval_expr(bc_cur_t *c, int *was_assign);
static void exec_stmt(bc_cur_t *c);
static void skip_stmt(bc_cur_t *c);

static void skip_balanced(bc_cur_t *c, char open_ch, char close_ch) {
    skip_ws(c, 1);
    if (c->src[c->pos] != open_ch) {
        return;
    }
    int depth = 0;
    while (c->src[c->pos] != '\0') {
        skip_ws(c, 1);
        char ch = c->src[c->pos];
        if (ch == '"') {
            c->pos++;
            while (c->src[c->pos] != '\0' && c->src[c->pos] != '"') {
                c->pos++;
            }
            if (c->src[c->pos] == '"') {
                c->pos++;
            }
            continue;
        }
        if (ch == open_ch) {
            depth++;
        } else if (ch == close_ch) {
            depth--;
            c->pos++;
            if (depth == 0) {
                return;
            }
            continue;
        }
        if (c->src[c->pos] != '\0') {
            c->pos++;
        }
    }
}

static void skip_stmt(bc_cur_t *c) {
    skip_ws(c, 1);
    if (c->src[c->pos] == '\0') {
        return;
    }
    if (c->src[c->pos] == '{') {
        skip_balanced(c, '{', '}');
        return;
    }
    if (match_kw(c, "if")) {
        skip_balanced(c, '(', ')');
        skip_stmt(c);
        size_t save = c->pos;
        skip_ws(c, 1);
        if (match_kw(c, "else")) {
            skip_stmt(c);
        } else {
            c->pos = save;
        }
        return;
    }
    if (match_kw(c, "while") || match_kw(c, "for")) {
        skip_balanced(c, '(', ')');
        skip_stmt(c);
        return;
    }
    int paren = 0;
    while (c->src[c->pos] != '\0') {
        char ch = c->src[c->pos];
        if (ch == '"') {
            c->pos++;
            while (c->src[c->pos] != '\0' && c->src[c->pos] != '"') {
                c->pos++;
            }
            if (c->src[c->pos] == '"') {
                c->pos++;
            }
            continue;
        }
        if (ch == '(') {
            paren++;
        } else if (ch == ')' && paren > 0) {
            paren--;
        } else if (paren == 0 && (ch == ';' || ch == '\n' || ch == '}')) {
            if (ch != '}') {
                c->pos++;
            }
            return;
        }
        c->pos++;
    }
}

static bc_num_t call_user_func(bc_vm_t *vm, const bc_func_t *fn,
                               bc_num_t *arg_vals, bc_var_t **arg_arrs,
                               size_t nargs) {
    size_t nsave = fn->nparams + fn->nautos;
    bc_var_t *saved = calloc(nsave > 0 ? nsave : 1, sizeof(*saved));
    bc_var_t **targets = calloc(nsave > 0 ? nsave : 1, sizeof(*targets));
    if (!saved || !targets) {
        free(saved);
        free(targets);
        return bc_zero();
    }

    for (size_t i = 0; i < fn->nparams; i++) {
        bc_var_t *v = vm_var(vm, fn->params[i]);
        targets[i] = v;
        if (v) {
            saved[i].val = v->val;
            saved[i].arr = v->arr;
            saved[i].arr_len = v->arr_len;
            saved[i].arr_cap = v->arr_cap;
            v->arr = NULL;
            v->arr_len = 0;
            v->arr_cap = 0;
            if (fn->param_is_arr[i] && i < nargs && arg_arrs[i]) {
                v->val = bc_zero();
                v->arr_len = arg_arrs[i]->arr_len;
                v->arr_cap = arg_arrs[i]->arr_len;
                if (v->arr_len > 0) {
                    v->arr = calloc(v->arr_len, sizeof(*v->arr));
                    for (size_t k = 0; k < v->arr_len; k++) {
                        v->arr[k] = bc_copy(&arg_arrs[i]->arr[k]);
                    }
                }
            } else {
                v->val = (i < nargs) ? bc_copy(&arg_vals[i]) : bc_zero();
            }
        }
    }
    for (size_t i = 0; i < fn->nautos; i++) {
        size_t si = fn->nparams + i;
        bc_var_t *v = vm_var(vm, fn->autos[i]);
        targets[si] = v;
        if (v) {
            saved[si].val = v->val;
            saved[si].arr = v->arr;
            saved[si].arr_len = v->arr_len;
            saved[si].arr_cap = v->arr_cap;
            v->val = bc_zero();
            v->arr = NULL;
            v->arr_len = 0;
            v->arr_cap = 0;
        }
    }

    bc_cur_t body = {.src = fn->body_src, .pos = 0, .vm = vm};
    while (vm->flow == BC_FLOW_NONE) {
        skip_ws(&body, 1);
        if (body.src[body.pos] == '\0') {
            break;
        }
        exec_stmt(&body);
    }

    bc_num_t ret = bc_zero();
    if (vm->flow == BC_FLOW_RETURN) {
        ret = vm->ret_val;
        vm->ret_val = bc_zero();
        vm->flow = BC_FLOW_NONE;
    }

    /* Restore autos and params in reverse order */
    for (size_t i = nsave; i > 0; i--) {
        size_t idx = i - 1;
        bc_var_t *v = targets[idx];
        if (v) {
            bc_free(&v->val);
            for (size_t k = 0; k < v->arr_len; k++) {
                bc_free(&v->arr[k]);
            }
            free(v->arr);
            v->val = saved[idx].val;
            v->arr = saved[idx].arr;
            v->arr_len = saved[idx].arr_len;
            v->arr_cap = saved[idx].arr_cap;
        }
    }
    free(saved);
    free(targets);
    return ret;
}

static bc_num_t get_lval(bc_vm_t *vm, const char *name, int is_arr,
                         size_t arr_idx) {
    if (!is_arr) {
        if (strcmp(name, "scale") == 0) {
            return bc_from_long(vm->scale);
        }
        if (strcmp(name, "ibase") == 0) {
            return bc_from_long(vm->ibase);
        }
        if (strcmp(name, "obase") == 0) {
            return bc_from_long(vm->obase);
        }
    }
    bc_var_t *v = vm_var(vm, name);
    if (!v) {
        return bc_zero();
    }
    if (is_arr) {
        bc_num_t *slot = vm_arr_slot(v, arr_idx);
        return slot ? bc_copy(slot) : bc_zero();
    }
    return bc_copy(&v->val);
}

static void set_lval(bc_vm_t *vm, const char *name, int is_arr, size_t arr_idx,
                     bc_num_t val) {
    if (!is_arr) {
        if (strcmp(name, "scale") == 0) {
            long s = bc_to_long(&val);
            vm->scale = (s >= 0) ? s : 0;
            bc_free(&val);
            return;
        }
        if (strcmp(name, "ibase") == 0) {
            long b = bc_to_long(&val);
            if (b >= 2 && b <= 16) {
                vm->ibase = b;
            }
            bc_free(&val);
            return;
        }
        if (strcmp(name, "obase") == 0) {
            long b = bc_to_long(&val);
            if (b >= 2) {
                vm->obase = b;
            }
            bc_free(&val);
            return;
        }
    }
    bc_var_t *v = vm_var(vm, name);
    if (!v) {
        bc_free(&val);
        return;
    }
    if (is_arr) {
        bc_num_t *slot = vm_arr_slot(v, arr_idx);
        if (slot) {
            bc_free(slot);
            *slot = val;
        } else {
            bc_free(&val);
        }
    } else {
        bc_free(&v->val);
        v->val = val;
    }
}

static bc_num_t eval_primary(bc_cur_t *c, int *was_assign) {
    skip_ws(c, 0);
    const char *s = c->src;
    char ch = s[c->pos];

    if (ch == '(') {
        c->pos++;
        bc_num_t v = eval_expr(c, NULL);
        skip_ws(c, 0);
        if (c->src[c->pos] == ')') {
            c->pos++;
        }
        return v;
    }
    if ((ch == '+' && s[c->pos + 1] == '+') ||
        (ch == '-' && s[c->pos + 1] == '-')) {
        int delta = (ch == '+') ? 1 : -1;
        c->pos += 2;
        char id[64];
        if (!parse_ident(c, id, sizeof(id))) {
            c->vm->err = 1;
            return bc_zero();
        }
        int is_arr = 0;
        size_t idx = 0;
        skip_ws(c, 0);
        if (c->src[c->pos] == '[') {
            c->pos++;
            bc_num_t iv = eval_expr(c, NULL);
            long lv = bc_to_long(&iv);
            bc_free(&iv);
            idx = (lv >= 0) ? (size_t)lv : 0;
            skip_ws(c, 0);
            if (c->src[c->pos] == ']') {
                c->pos++;
            }
            is_arr = 1;
        }
        bc_num_t cur = get_lval(c->vm, id, is_arr, idx);
        bc_num_t one = bc_from_long(delta);
        bc_num_t nxt = bc_add(&cur, &one);
        bc_free(&cur);
        bc_free(&one);
        set_lval(c->vm, id, is_arr, idx, bc_copy(&nxt));
        return nxt;
    }
    if (isdigit((unsigned char)ch) || (ch >= 'A' && ch <= 'F') || ch == '.') {
        size_t start = c->pos;
        while (isdigit((unsigned char)s[c->pos]) ||
               (s[c->pos] >= 'A' && s[c->pos] <= 'F') || s[c->pos] == '.') {
            c->pos++;
        }
        size_t len = c->pos - start;
        char *buf = malloc(len + 1);
        if (!buf) {
            return bc_zero();
        }
        (void)memcpy(buf, s + start, len);
        buf[len] = '\0';
        bc_num_t v = bc_from_str(buf, c->vm->ibase);
        free(buf);
        return v;
    }
    char id[64];
    if (parse_ident(c, id, sizeof(id))) {
        skip_ws(c, 0);
        if (c->src[c->pos] == '(') {
            c->pos++;
            bc_num_t args[16];
            bc_var_t *arrs[16];
            size_t nargs = 0;
            (void)memset(arrs, 0, sizeof(arrs));
            skip_ws(c, 0);
            if (c->src[c->pos] != ')') {
                while (nargs < 16) {
                    skip_ws(c, 0);
                    size_t save = c->pos;
                    char aid[64];
                    if (parse_ident(c, aid, sizeof(aid))) {
                        skip_ws(c, 0);
                        if (c->src[c->pos] == '[' &&
                            c->src[c->pos + 1] == ']') {
                            c->pos += 2;
                            args[nargs] = bc_zero();
                            arrs[nargs] = vm_var(c->vm, aid);
                            nargs++;
                            skip_ws(c, 0);
                            if (c->src[c->pos] == ',') {
                                c->pos++;
                                continue;
                            }
                            break;
                        }
                    }
                    c->pos = save;
                    args[nargs++] = eval_expr(c, NULL);
                    skip_ws(c, 0);
                    if (c->src[c->pos] == ',') {
                        c->pos++;
                        continue;
                    }
                    break;
                }
            }
            skip_ws(c, 0);
            if (c->src[c->pos] == ')') {
                c->pos++;
            }

            bc_num_t res = bc_zero();
            size_t sc = (size_t)c->vm->scale;
            if (strcmp(id, "sqrt") == 0 && nargs >= 1) {
                res = bc_sqrt(&args[0], sc, &c->vm->err);
            } else if (strcmp(id, "length") == 0 && nargs >= 1) {
                res = bc_from_long((long)bc_num_length(&args[0]));
            } else if (strcmp(id, "scale") == 0 && nargs >= 1) {
                res = bc_from_long((long)args[0].scale);
            } else if (c->vm->mathlib && strcmp(id, "e") == 0 && nargs >= 1) {
                res = bc_math_exp(&args[0], sc);
            } else if (c->vm->mathlib && strcmp(id, "l") == 0 && nargs >= 1) {
                res = bc_math_ln(&args[0], sc, &c->vm->err);
            } else if (c->vm->mathlib && strcmp(id, "s") == 0 && nargs >= 1) {
                res = bc_math_sin(&args[0], sc);
            } else if (c->vm->mathlib && strcmp(id, "c") == 0 && nargs >= 1) {
                res = bc_math_cos(&args[0], sc);
            } else if (c->vm->mathlib && strcmp(id, "a") == 0 && nargs >= 1) {
                res = bc_math_atan(&args[0], sc);
            } else if (c->vm->mathlib && strcmp(id, "j") == 0 && nargs >= 2) {
                long n = bc_to_long(&args[0]);
                double x = bc_to_double(&args[1]);
                double sum = 0.0;
                double term = 1.0;
                for (long k = 1; k <= n; k++) {
                    term *= (x * 0.5) / (double)k;
                }
                for (long k = 0; k < 40; k++) {
                    sum += term;
                    term *= -(x * x * 0.25) /
                            ((double)(k + 1) * (double)(k + 1 + n));
                }
                res = bc_from_double(sum, sc);
            } else {
                for (size_t fi = 0; fi < c->vm->nfuncs; fi++) {
                    if (strcmp(c->vm->funcs[fi].name, id) == 0) {
                        res = call_user_func(c->vm, &c->vm->funcs[fi], args,
                                             arrs, nargs);
                        break;
                    }
                }
            }
            for (size_t i = 0; i < nargs; i++) {
                bc_free(&args[i]);
            }
            return res;
        }

        int is_arr = 0;
        size_t idx = 0;
        if (c->src[c->pos] == '[') {
            c->pos++;
            bc_num_t iv = eval_expr(c, NULL);
            long lv = bc_to_long(&iv);
            bc_free(&iv);
            idx = (lv >= 0) ? (size_t)lv : 0;
            skip_ws(c, 0);
            if (c->src[c->pos] == ']') {
                c->pos++;
            }
            is_arr = 1;
        }
        skip_ws(c, 0);
        /* Postfix ++ / -- */
        if ((c->src[c->pos] == '+' && c->src[c->pos + 1] == '+') ||
            (c->src[c->pos] == '-' && c->src[c->pos + 1] == '-')) {
            int delta = (c->src[c->pos] == '+') ? 1 : -1;
            c->pos += 2;
            bc_num_t cur = get_lval(c->vm, id, is_arr, idx);
            bc_num_t one = bc_from_long(delta);
            bc_num_t nxt = bc_add(&cur, &one);
            bc_free(&one);
            set_lval(c->vm, id, is_arr, idx, nxt);
            return cur;
        }
        /* Assignment operators */
        char op = c->src[c->pos];
        char op2 = c->src[c->pos + 1];
        if ((op == '=' && op2 != '=') ||
            ((op == '+' || op == '-' || op == '*' || op == '/' || op == '%' ||
              op == '^') &&
             op2 == '=')) {
            if (was_assign) {
                *was_assign = 1;
            }
            if (op == '=') {
                c->pos++;
                bc_num_t rhs = eval_expr(c, NULL);
                set_lval(c->vm, id, is_arr, idx, bc_copy(&rhs));
                return rhs;
            }
            c->pos += 2;
            bc_num_t lhs = get_lval(c->vm, id, is_arr, idx);
            bc_num_t rhs = eval_expr(c, NULL);
            bc_num_t res = bc_zero();
            size_t sc = (size_t)c->vm->scale;
            if (op == '+') {
                res = bc_add(&lhs, &rhs);
            } else if (op == '-') {
                res = bc_sub(&lhs, &rhs);
            } else if (op == '*') {
                res = bc_mul(&lhs, &rhs, sc);
            } else if (op == '/') {
                res = bc_div(&lhs, &rhs, sc, &c->vm->err);
            } else if (op == '%') {
                res = bc_mod(&lhs, &rhs, sc, &c->vm->err);
            } else if (op == '^') {
                res = bc_pow(&lhs, &rhs, sc, &c->vm->err);
            }
            bc_free(&lhs);
            bc_free(&rhs);
            set_lval(c->vm, id, is_arr, idx, bc_copy(&res));
            return res;
        }
        return get_lval(c->vm, id, is_arr, idx);
    }

    return bc_zero();
}

static bc_num_t eval_unary(bc_cur_t *c, int *was_assign);

static bc_num_t eval_pow(bc_cur_t *c, int *was_assign) {
    bc_num_t left = eval_primary(c, was_assign);
    skip_ws(c, 0);
    if (c->src[c->pos] == '^' && c->src[c->pos + 1] != '=') {
        if (was_assign) {
            *was_assign = 0;
        }
        c->pos++;
        bc_num_t right = eval_unary(c, NULL);
        bc_num_t r = bc_pow(&left, &right, (size_t)c->vm->scale, &c->vm->err);
        bc_free(&left);
        bc_free(&right);
        return r;
    }
    return left;
}

static bc_num_t eval_unary(bc_cur_t *c, int *was_assign) {
    skip_ws(c, 0);
    if (c->src[c->pos] == '-' && c->src[c->pos + 1] != '-' &&
        c->src[c->pos + 1] != '=') {
        c->pos++;
        bc_num_t v = eval_unary(c, NULL);
        v.neg = !v.neg;
        if (bc_is_zero(&v)) {
            v.neg = 0;
        }
        return v;
    }
    if (c->src[c->pos] == '!' && c->src[c->pos + 1] != '=') {
        c->pos++;
        bc_num_t v = eval_unary(c, NULL);
        int z = bc_is_zero(&v);
        bc_free(&v);
        return bc_from_long(z ? 1 : 0);
    }
    return eval_pow(c, was_assign);
}

static bc_num_t eval_mul(bc_cur_t *c, int *was_assign) {
    bc_num_t left = eval_unary(c, was_assign);
    for (;;) {
        skip_ws(c, 0);
        char op = c->src[c->pos];
        if ((op == '*' || op == '/' || op == '%') &&
            c->src[c->pos + 1] != '=') {
            if (was_assign) {
                *was_assign = 0;
            }
            c->pos++;
            bc_num_t right = eval_unary(c, NULL);
            bc_num_t r = bc_zero();
            size_t sc = (size_t)c->vm->scale;
            if (op == '*') {
                r = bc_mul(&left, &right, sc);
            } else if (op == '/') {
                r = bc_div(&left, &right, sc, &c->vm->err);
            } else {
                r = bc_mod(&left, &right, sc, &c->vm->err);
            }
            bc_free(&left);
            bc_free(&right);
            left = r;
        } else {
            break;
        }
    }
    return left;
}

static bc_num_t eval_add(bc_cur_t *c, int *was_assign) {
    bc_num_t left = eval_mul(c, was_assign);
    for (;;) {
        skip_ws(c, 0);
        char op = c->src[c->pos];
        if ((op == '+' || op == '-') && c->src[c->pos + 1] != '=' &&
            c->src[c->pos + 1] != op) {
            if (was_assign) {
                *was_assign = 0;
            }
            c->pos++;
            bc_num_t right = eval_mul(c, NULL);
            bc_num_t r =
                (op == '+') ? bc_add(&left, &right) : bc_sub(&left, &right);
            bc_free(&left);
            bc_free(&right);
            left = r;
        } else {
            break;
        }
    }
    return left;
}

static bc_num_t eval_rel(bc_cur_t *c, int *was_assign) {
    bc_num_t left = eval_add(c, was_assign);
    skip_ws(c, 0);
    const char *s = c->src + c->pos;
    int op = 0;
    if (s[0] == '=' && s[1] == '=') {
        op = 1;
        c->pos += 2;
    } else if (s[0] == '!' && s[1] == '=') {
        op = 2;
        c->pos += 2;
    } else if (s[0] == '<' && s[1] == '=') {
        op = 3;
        c->pos += 2;
    } else if (s[0] == '>' && s[1] == '=') {
        op = 4;
        c->pos += 2;
    } else if (s[0] == '<') {
        op = 5;
        c->pos += 1;
    } else if (s[0] == '>') {
        op = 6;
        c->pos += 1;
    }
    if (op == 0) {
        return left;
    }
    if (was_assign) {
        *was_assign = 0;
    }
    bc_num_t right = eval_add(c, NULL);
    int cmp = bc_cmp(&left, &right);
    bc_free(&left);
    bc_free(&right);
    int ok = (op == 1)   ? (cmp == 0)
             : (op == 2) ? (cmp != 0)
             : (op == 3) ? (cmp <= 0)
             : (op == 4) ? (cmp >= 0)
             : (op == 5) ? (cmp < 0)
                         : (cmp > 0);
    return bc_from_long(ok ? 1 : 0);
}

static bc_num_t eval_and(bc_cur_t *c, int *was_assign) {
    bc_num_t left = eval_rel(c, was_assign);
    while (c->src[c->pos] == '&' && c->src[c->pos + 1] == '&') {
        if (was_assign) {
            *was_assign = 0;
        }
        c->pos += 2;
        bc_num_t right = eval_rel(c, NULL);
        int ok = (!bc_is_zero(&left) && !bc_is_zero(&right));
        bc_free(&left);
        bc_free(&right);
        left = bc_from_long(ok ? 1 : 0);
        skip_ws(c, 0);
    }
    return left;
}

static bc_num_t eval_expr(bc_cur_t *c, int *was_assign) {
    if (was_assign) {
        *was_assign = 0;
    }
    bc_num_t left = eval_and(c, was_assign);
    skip_ws(c, 0);
    while (c->src[c->pos] == '|' && c->src[c->pos + 1] == '|') {
        if (was_assign) {
            *was_assign = 0;
        }
        c->pos += 2;
        bc_num_t right = eval_and(c, NULL);
        int ok = (!bc_is_zero(&left) || !bc_is_zero(&right));
        bc_free(&left);
        bc_free(&right);
        left = bc_from_long(ok ? 1 : 0);
        skip_ws(c, 0);
    }
    return left;
}

static void bc_func_free(bc_func_t *fn) {
    free(fn->name);
    for (size_t j = 0; j < fn->nparams; j++) {
        free(fn->params[j]);
    }
    free(fn->params);
    free(fn->param_is_arr);
    for (size_t j = 0; j < fn->nautos; j++) {
        free(fn->autos[j]);
    }
    free(fn->autos);
    free(fn->auto_is_arr);
    free(fn->body_src);
    (void)memset(fn, 0, sizeof(*fn));
}

static void parse_func_def(bc_cur_t *c) {
    char name[64];
    if (!parse_ident(c, name, sizeof(name))) {
        c->vm->err = 1;
        return;
    }
    skip_ws(c, 0);
    if (c->src[c->pos] != '(') {
        c->vm->err = 1;
        return;
    }
    c->pos++;

    bc_func_t fn = {.name = strdup(name)};
    skip_ws(c, 0);
    while (c->src[c->pos] != '\0' && c->src[c->pos] != ')') {
        char pn[64];
        if (!parse_ident(c, pn, sizeof(pn))) {
            break;
        }
        int is_arr = 0;
        skip_ws(c, 0);
        if (c->src[c->pos] == '[' && c->src[c->pos + 1] == ']') {
            c->pos += 2;
            is_arr = 1;
        }
        char **np = realloc(fn.params, (fn.nparams + 1) * sizeof(*np));
        if (!np) {
            break;
        }
        fn.params = np;
        int *na = realloc(fn.param_is_arr, (fn.nparams + 1) * sizeof(*na));
        if (!na) {
            break;
        }
        fn.param_is_arr = na;
        fn.params[fn.nparams] = strdup(pn);
        fn.param_is_arr[fn.nparams] = is_arr;
        fn.nparams++;
        skip_ws(c, 0);
        if (c->src[c->pos] == ',') {
            c->pos++;
            skip_ws(c, 0);
        }
    }
    if (c->src[c->pos] == ')') {
        c->pos++;
    }
    skip_ws(c, 1);
    if (c->src[c->pos] != '{') {
        c->vm->err = 1;
        bc_func_free(&fn);
        return;
    }
    size_t bstart = c->pos + 1;
    skip_balanced(c, '{', '}');
    size_t bend = (c->pos > bstart) ? (c->pos - 1) : bstart;

    bc_cur_t inner = {.src = c->src, .pos = bstart, .vm = c->vm};
    skip_ws(&inner, 1);
    while (inner.pos < bend && match_kw(&inner, "auto")) {
        while (inner.pos < bend) {
            char an[64];
            if (!parse_ident(&inner, an, sizeof(an))) {
                break;
            }
            int is_arr = 0;
            skip_ws(&inner, 0);
            if (inner.src[inner.pos] == '[' &&
                inner.src[inner.pos + 1] == ']') {
                inner.pos += 2;
                is_arr = 1;
            }
            char **na = realloc(fn.autos, (fn.nautos + 1) * sizeof(*na));
            if (!na) {
                break;
            }
            fn.autos = na;
            int *nia = realloc(fn.auto_is_arr, (fn.nautos + 1) * sizeof(*nia));
            if (!nia) {
                break;
            }
            fn.auto_is_arr = nia;
            fn.autos[fn.nautos] = strdup(an);
            fn.auto_is_arr[fn.nautos] = is_arr;
            fn.nautos++;
            skip_ws(&inner, 0);
            if (inner.src[inner.pos] == ',') {
                inner.pos++;
                skip_ws(&inner, 0);
            } else {
                break;
            }
        }
        skip_ws(&inner, 0);
        if (inner.src[inner.pos] == ';' || inner.src[inner.pos] == '\n') {
            inner.pos++;
        }
        skip_ws(&inner, 1);
    }

    size_t blen = (bend > inner.pos) ? (bend - inner.pos) : 0;
    fn.body_src = malloc(blen + 1);
    if (fn.body_src) {
        if (blen > 0) {
            (void)memcpy(fn.body_src, c->src + inner.pos, blen);
        }
        fn.body_src[blen] = '\0';
    }

    /* Replace existing definition if present */
    for (size_t i = 0; i < c->vm->nfuncs; i++) {
        if (strcmp(c->vm->funcs[i].name, fn.name) == 0) {
            bc_func_free(&c->vm->funcs[i]);
            c->vm->funcs[i] = fn;
            return;
        }
    }
    if (c->vm->nfuncs == c->vm->cap_funcs) {
        size_t nc = (c->vm->cap_funcs == 0) ? 8 : c->vm->cap_funcs * 2;
        bc_func_t *nf = realloc(c->vm->funcs, nc * sizeof(*nf));
        if (!nf) {
            bc_func_free(&fn);
            return;
        }
        c->vm->funcs = nf;
        c->vm->cap_funcs = nc;
    }
    c->vm->funcs[c->vm->nfuncs++] = fn;
}

static void exec_stmt(bc_cur_t *c) {
    skip_ws(c, 1);
    if (c->vm->flow != BC_FLOW_NONE || c->src[c->pos] == '\0') {
        return;
    }
    if (c->src[c->pos] == ';') {
        c->pos++;
        return;
    }
    if (c->src[c->pos] == '{') {
        c->pos++;
        while (c->vm->flow == BC_FLOW_NONE) {
            skip_ws(c, 1);
            if (c->src[c->pos] == '\0' || c->src[c->pos] == '}') {
                break;
            }
            exec_stmt(c);
        }
        if (c->src[c->pos] == '}') {
            c->pos++;
        } else {
            /* Fast-forward to matching brace on early return/break */
            int depth = 1;
            while (c->src[c->pos] != '\0' && depth > 0) {
                if (c->src[c->pos] == '{') {
                    depth++;
                } else if (c->src[c->pos] == '}') {
                    depth--;
                }
                c->pos++;
            }
        }
        return;
    }
    if (c->src[c->pos] == '"') {
        c->pos++;
        while (c->src[c->pos] != '\0' && c->src[c->pos] != '"') {
            (void)putchar(c->src[c->pos++]);
        }
        if (c->src[c->pos] == '"') {
            c->pos++;
        }
        return;
    }
    if (match_kw(c, "define")) {
        parse_func_def(c);
        return;
    }
    if (match_kw(c, "quit")) {
        c->vm->flow = BC_FLOW_QUIT;
        return;
    }
    if (match_kw(c, "break")) {
        c->vm->flow = BC_FLOW_BREAK;
        return;
    }
    if (match_kw(c, "continue")) {
        c->vm->flow = BC_FLOW_CONTINUE;
        return;
    }
    if (match_kw(c, "return")) {
        skip_ws(c, 0);
        bc_free(&c->vm->ret_val);
        if (c->src[c->pos] == '(') {
            c->pos++;
            skip_ws(c, 0);
            if (c->src[c->pos] == ')') {
                c->pos++;
                c->vm->ret_val = bc_zero();
            } else {
                c->vm->ret_val = eval_expr(c, NULL);
                skip_ws(c, 0);
                if (c->src[c->pos] == ')') {
                    c->pos++;
                }
            }
        } else if (c->src[c->pos] != ';' && c->src[c->pos] != '\n' &&
                   c->src[c->pos] != '}' && c->src[c->pos] != '\0') {
            c->vm->ret_val = eval_expr(c, NULL);
        } else {
            c->vm->ret_val = bc_zero();
        }
        c->vm->flow = BC_FLOW_RETURN;
        return;
    }
    if (match_kw(c, "if")) {
        skip_ws(c, 0);
        if (c->src[c->pos] == '(') {
            c->pos++;
        }
        bc_num_t cond = eval_expr(c, NULL);
        int ok = !bc_is_zero(&cond);
        bc_free(&cond);
        skip_ws(c, 0);
        if (c->src[c->pos] == ')') {
            c->pos++;
        }
        if (ok) {
            exec_stmt(c);
            size_t save = c->pos;
            skip_ws(c, 1);
            if (match_kw(c, "else")) {
                skip_stmt(c);
            } else {
                c->pos = save;
            }
        } else {
            skip_stmt(c);
            size_t save = c->pos;
            skip_ws(c, 1);
            if (match_kw(c, "else")) {
                exec_stmt(c);
            } else {
                c->pos = save;
            }
        }
        return;
    }
    if (match_kw(c, "while")) {
        skip_ws(c, 0);
        if (c->src[c->pos] == '(') {
            c->pos++;
        }
        size_t cond_pos = c->pos;
        for (;;) {
            c->pos = cond_pos;
            bc_num_t cond = eval_expr(c, NULL);
            int ok = !bc_is_zero(&cond);
            bc_free(&cond);
            skip_ws(c, 0);
            if (c->src[c->pos] == ')') {
                c->pos++;
            }
            if (!ok) {
                skip_stmt(c);
                break;
            }
            exec_stmt(c);
            if (c->vm->flow == BC_FLOW_BREAK) {
                c->vm->flow = BC_FLOW_NONE;
                c->pos = cond_pos;
                skip_ws(c, 0);
                int par = 1;
                while (c->src[c->pos] != '\0' && par > 0) {
                    if (c->src[c->pos] == '(') {
                        par++;
                    } else if (c->src[c->pos] == ')') {
                        par--;
                    }
                    c->pos++;
                }
                skip_stmt(c);
                break;
            }
            if (c->vm->flow == BC_FLOW_CONTINUE) {
                c->vm->flow = BC_FLOW_NONE;
            }
            if (c->vm->flow != BC_FLOW_NONE) {
                break;
            }
        }
        return;
    }
    if (match_kw(c, "for")) {
        skip_ws(c, 0);
        if (c->src[c->pos] == '(') {
            c->pos++;
        }
        skip_ws(c, 0);
        if (c->src[c->pos] != ';') {
            bc_num_t init = eval_expr(c, NULL);
            bc_free(&init);
        }
        skip_ws(c, 0);
        if (c->src[c->pos] == ';') {
            c->pos++;
        }
        size_t cond_pos = c->pos;
        /* Locate step_pos and body_pos */
        while (c->src[c->pos] != '\0' && c->src[c->pos] != ';') {
            c->pos++;
        }
        if (c->src[c->pos] == ';') {
            c->pos++;
        }
        size_t step_pos = c->pos;
        int par = 1;
        while (c->src[c->pos] != '\0' && par > 0) {
            if (c->src[c->pos] == '(') {
                par++;
            } else if (c->src[c->pos] == ')') {
                par--;
            }
            c->pos++;
        }
        size_t body_pos = c->pos;

        for (;;) {
            c->pos = cond_pos;
            skip_ws(c, 0);
            int ok = 1;
            if (c->src[c->pos] != ';') {
                bc_num_t cond = eval_expr(c, NULL);
                ok = !bc_is_zero(&cond);
                bc_free(&cond);
            }
            if (!ok) {
                c->pos = body_pos;
                skip_stmt(c);
                break;
            }
            c->pos = body_pos;
            exec_stmt(c);
            if (c->vm->flow == BC_FLOW_BREAK) {
                c->vm->flow = BC_FLOW_NONE;
                c->pos = body_pos;
                skip_stmt(c);
                break;
            }
            if (c->vm->flow == BC_FLOW_CONTINUE) {
                c->vm->flow = BC_FLOW_NONE;
            }
            if (c->vm->flow != BC_FLOW_NONE) {
                break;
            }
            c->pos = step_pos;
            skip_ws(c, 0);
            if (c->src[c->pos] != ')') {
                bc_num_t st = eval_expr(c, NULL);
                bc_free(&st);
            }
        }
        return;
    }

    int was_assign = 0;
    bc_num_t val = eval_expr(c, &was_assign);
    if (!was_assign && c->vm->flow == BC_FLOW_NONE) {
        bc_print_num(&val, c->vm->obase);
    }
    bc_free(&val);
    skip_ws(c, 0);
    if (c->src[c->pos] == ';' || c->src[c->pos] == '\n') {
        c->pos++;
    }
}

int bc_exec_source(bc_vm_t *vm, const char *src) {
    bc_cur_t cur = {.src = src ? src : "", .pos = 0, .vm = vm};
    while (vm->flow == BC_FLOW_NONE) {
        skip_ws(&cur, 1);
        if (cur.src[cur.pos] == '\0') {
            break;
        }
        size_t before = cur.pos;
        exec_stmt(&cur);
        if (cur.pos == before && vm->flow == BC_FLOW_NONE) {
            cur.pos++;
        }
    }
    return vm->err ? -1 : 0;
}
