/**
 * @file     eval.c
 * @brief    Tree-walking VM, built-ins, and I/O for Ring 3 POSIX.1-2024 awk
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <math.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "awk.h"

/* need:: COMP_LIBC_0024 */

static int parse_pure_number(const char *s, double *out) {
    if (!s) {
        return 0;
    }
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        return 0;
    }
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) {
        return 0;
    }
    while (*end == ' ' || *end == '\t') {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }
    *out = v;
    return 1;
}

awk_val_t val_num(double n) {
    awk_val_t v = {.kind = VAL_NUM, .num = n, .str = NULL};
    return v;
}

awk_val_t val_str(const char *s) {
    awk_val_t v = {.kind = VAL_STR, .num = 0.0, .str = strdup(s ? s : "")};
    return v;
}

awk_val_t val_numstr(const char *s) {
    awk_val_t v = {.kind = VAL_STR, .num = 0.0, .str = strdup(s ? s : "")};
    double n = 0.0;
    if (parse_pure_number(s, &n)) {
        v.kind = VAL_NUMSTR;
        v.num = n;
    }
    return v;
}

awk_val_t val_copy(const awk_val_t *v) {
    awk_val_t c = *v;
    if (v->str) {
        c.str = strdup(v->str);
    }
    return c;
}

void val_free(awk_val_t *v) {
    free(v->str);
    v->str = NULL;
    v->kind = VAL_UNINIT;
    v->num = 0.0;
}

double val_to_num(const awk_val_t *v) {
    if (v->kind == VAL_UNINIT) {
        return 0.0;
    }
    if (v->kind == VAL_NUM || v->kind == VAL_NUMSTR) {
        return v->num;
    }
    return v->str ? strtod(v->str, NULL) : 0.0;
}

static const char *get_fmt_var(awk_vm_t *vm, const char *name,
                               const char *def) {
    awk_var_t *v = vm_get_var(vm, name);
    if (v && v->val.str && v->val.str[0] != '\0') {
        return v->val.str;
    }
    return def;
}

static char *format_num(double n, const char *fmt) {
    char buf[128];
    if (n == (double)(long long)n && fabs(n) < 1e15) {
        (void)snprintf(buf, sizeof(buf), "%lld", (long long)n);
    } else {
        (void)snprintf(buf, sizeof(buf), fmt ? fmt : "%.6g", n);
    }
    return strdup(buf);
}

char *val_to_str(awk_vm_t *vm, const awk_val_t *v) {
    if (v->kind == VAL_UNINIT) {
        return strdup("");
    }
    if (v->kind == VAL_STR || v->kind == VAL_NUMSTR) {
        return strdup(v->str ? v->str : "");
    }
    return format_num(v->num, get_fmt_var(vm, "CONVFMT", "%.6g"));
}

static char *val_to_ofmt(awk_vm_t *vm, const awk_val_t *v) {
    if (v->kind == VAL_UNINIT) {
        return strdup("");
    }
    if (v->kind == VAL_STR || v->kind == VAL_NUMSTR) {
        return strdup(v->str ? v->str : "");
    }
    return format_num(v->num, get_fmt_var(vm, "OFMT", "%.6g"));
}

static int val_truthy(const awk_val_t *v) {
    if (v->kind == VAL_UNINIT) {
        return 0;
    }
    if (v->kind == VAL_NUM) {
        return v->num != 0.0;
    }
    return v->str && v->str[0] != '\0';
}

static int val_cmp(awk_vm_t *vm, const awk_val_t *a, const awk_val_t *b) {
    int a_num =
        (a->kind == VAL_NUM || a->kind == VAL_NUMSTR || a->kind == VAL_UNINIT);
    int b_num =
        (b->kind == VAL_NUM || b->kind == VAL_NUMSTR || b->kind == VAL_UNINIT);
    if (a_num && b_num && !(a->kind == VAL_UNINIT && b->kind == VAL_STR) &&
        !(b->kind == VAL_UNINIT && a->kind == VAL_STR)) {
        double da = val_to_num(a);
        double db = val_to_num(b);
        return (da < db) ? -1 : ((da > db) ? 1 : 0);
    }
    char *sa = val_to_str(vm, a);
    char *sb = val_to_str(vm, b);
    int c = strcmp(sa ? sa : "", sb ? sb : "");
    free(sa);
    free(sb);
    return (c < 0) ? -1 : ((c > 0) ? 1 : 0);
}

static void arr_free(awk_arr_t *arr) {
    for (size_t i = 0; i < arr->len; i++) {
        free(arr->items[i].key);
        val_free(&arr->items[i].val);
    }
    free(arr->items);
    arr->items = NULL;
    arr->len = 0;
    arr->cap = 0;
}

static awk_var_t *vm_find_global(awk_vm_t *vm, const char *name) {
    for (size_t i = 0; i < vm->nvars; i++) {
        if (strcmp(vm->vars[i].name, name) == 0) {
            return &vm->vars[i];
        }
    }
    if (vm->nvars == vm->cap_vars) {
        size_t nc = (vm->cap_vars == 0) ? 32 : vm->cap_vars * 2;
        awk_var_t *nv = realloc(vm->vars, nc * sizeof(*nv));
        if (!nv) {
            return NULL;
        }
        (void)memset(nv + vm->cap_vars, 0, (nc - vm->cap_vars) * sizeof(*nv));
        vm->vars = nv;
        vm->cap_vars = nc;
    }
    size_t idx = vm->nvars++;
    vm->vars[idx].name = strdup(name);
    return &vm->vars[idx];
}

awk_var_t *vm_get_var(awk_vm_t *vm, const char *name) {
    if (vm->frame) {
        for (size_t i = 0; i < vm->frame->nlocals; i++) {
            if (strcmp(vm->frame->names[i], name) == 0) {
                if (vm->frame->arr_refs && vm->frame->arr_refs[i]) {
                    return vm->frame->arr_refs[i];
                }
                return &vm->frame->locals[i];
            }
        }
    }
    return vm_find_global(vm, name);
}

void vm_set_str(awk_vm_t *vm, const char *name, const char *s) {
    awk_var_t *v = vm_get_var(vm, name);
    if (!v) {
        return;
    }
    val_free(&v->val);
    v->val = val_str(s);
}

void vm_set_num(awk_vm_t *vm, const char *name, double n) {
    awk_var_t *v = vm_get_var(vm, name);
    if (!v) {
        return;
    }
    val_free(&v->val);
    v->val = val_num(n);
}

static arr_entry_t *arr_find(awk_arr_t *arr, const char *key) {
    for (size_t i = 0; i < arr->len; i++) {
        if (strcmp(arr->items[i].key, key) == 0) {
            return &arr->items[i];
        }
    }
    return NULL;
}

void vm_arr_set(awk_var_t *var, const char *key, awk_val_t v) {
    if (!var) {
        val_free(&v);
        return;
    }
    var->is_arr = 1;
    arr_entry_t *e = arr_find(&var->arr, key);
    if (e) {
        val_free(&e->val);
        e->val = v;
        return;
    }
    if (var->arr.len == var->arr.cap) {
        size_t nc = (var->arr.cap == 0) ? 16 : var->arr.cap * 2;
        arr_entry_t *ni = realloc(var->arr.items, nc * sizeof(*ni));
        if (!ni) {
            val_free(&v);
            return;
        }
        (void)memset(ni + var->arr.cap, 0, (nc - var->arr.cap) * sizeof(*ni));
        var->arr.items = ni;
        var->arr.cap = nc;
    }
    size_t idx = var->arr.len++;
    var->arr.items[idx].key = strdup(key);
    var->arr.items[idx].val = v;
}

static void arr_delete_key(awk_var_t *var, const char *key) {
    if (!var) {
        return;
    }
    for (size_t i = 0; i < var->arr.len; i++) {
        if (strcmp(var->arr.items[i].key, key) == 0) {
            free(var->arr.items[i].key);
            val_free(&var->arr.items[i].val);
            if (i + 1 < var->arr.len) {
                (void)memmove(&var->arr.items[i], &var->arr.items[i + 1],
                              (var->arr.len - i - 1) * sizeof(*var->arr.items));
            }
            var->arr.len--;
            return;
        }
    }
}

void vm_init(awk_vm_t *vm, prog_t *prog) {
    (void)memset(vm, 0, sizeof(*vm));
    vm->prog = prog;
    vm->rng_state = 1u;
    vm_set_str(vm, "FS", " ");
    vm_set_str(vm, "RS", "\n");
    vm_set_str(vm, "OFS", " ");
    vm_set_str(vm, "ORS", "\n");
    vm_set_str(vm, "OFMT", "%.6g");
    vm_set_str(vm, "CONVFMT", "%.6g");
    vm_set_str(vm, "SUBSEP", "\034");
    vm_set_str(vm, "FILENAME", "");
    vm_set_num(vm, "NF", 0.0);
    vm_set_num(vm, "NR", 0.0);
    vm_set_num(vm, "FNR", 0.0);
    vm_set_num(vm, "RSTART", 0.0);
    vm_set_num(vm, "RLENGTH", 0.0);
    vm->record = strdup("");
}

static void close_all_io(awk_vm_t *vm) {
    for (size_t i = 0; i < vm->nios; i++) {
        if (vm->ios[i].fp) {
            if (vm->ios[i].is_pipe) {
                (void)pclose(vm->ios[i].fp);
            } else {
                (void)fclose(vm->ios[i].fp);
            }
        }
        free(vm->ios[i].name);
    }
    free(vm->ios);
    vm->ios = NULL;
    vm->nios = 0;
    vm->cap_ios = 0;
}

void vm_free(awk_vm_t *vm) {
    close_all_io(vm);
    for (size_t i = 0; i < vm->nvars; i++) {
        free(vm->vars[i].name);
        val_free(&vm->vars[i].val);
        arr_free(&vm->vars[i].arr);
    }
    free(vm->vars);
    free(vm->record);
    for (size_t i = 0; i < vm->nf; i++) {
        free(vm->fields[i]);
    }
    free(vm->fields);
    val_free(&vm->ret_val);
    (void)memset(vm, 0, sizeof(*vm));
}

static int push_field(char ***arr, size_t *len, size_t *cap, const char *s,
                      size_t n) {
    if (*len == *cap) {
        size_t nc = (*cap == 0) ? 16 : (*cap) * 2;
        char **na = realloc(*arr, nc * sizeof(*na));
        if (!na) {
            return -1;
        }
        *arr = na;
        *cap = nc;
    }
    char *cp = malloc(n + 1);
    if (!cp) {
        return -1;
    }
    if (n > 0) {
        (void)memcpy(cp, s, n);
    }
    cp[n] = '\0';
    (*arr)[(*len)++] = cp;
    return 0;
}

static size_t split_string(const char *s, const char *fs, char ***out_fields) {
    char **arr = NULL;
    size_t len = 0;
    size_t cap = 0;
    if (!s || *s == '\0') {
        *out_fields = NULL;
        return 0;
    }
    if (!fs || strcmp(fs, " ") == 0) {
        const char *p = s;
        while (*p != '\0') {
            while (*p == ' ' || *p == '\t' || *p == '\n') {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            const char *st = p;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                p++;
            }
            (void)push_field(&arr, &len, &cap, st, (size_t)(p - st));
        }
        *out_fields = arr;
        return len;
    }
    if (fs[0] == '\0') {
        for (size_t i = 0; s[i] != '\0'; i++) {
            (void)push_field(&arr, &len, &cap, s + i, 1);
        }
        *out_fields = arr;
        return len;
    }
    if (fs[1] == '\0') {
        const char *p = s;
        for (;;) {
            const char *sep = strchr(p, fs[0]);
            if (!sep) {
                (void)push_field(&arr, &len, &cap, p, strlen(p));
                break;
            }
            (void)push_field(&arr, &len, &cap, p, (size_t)(sep - p));
            p = sep + 1;
        }
        *out_fields = arr;
        return len;
    }
    regex_t re;
    if (regcomp(&re, fs, REG_EXTENDED) != 0) {
        (void)push_field(&arr, &len, &cap, s, strlen(s));
        *out_fields = arr;
        return len;
    }
    const char *p = s;
    for (;;) {
        regmatch_t pm;
        if (regexec(&re, p, 1, &pm, 0) != 0) {
            (void)push_field(&arr, &len, &cap, p, strlen(p));
            break;
        }
        if (pm.rm_eo == 0) {
            if (*p == '\0') {
                (void)push_field(&arr, &len, &cap, "", 0);
                break;
            }
            (void)push_field(&arr, &len, &cap, p, 1);
            p++;
            continue;
        }
        (void)push_field(&arr, &len, &cap, p, (size_t)pm.rm_so);
        p += pm.rm_eo;
    }
    regfree(&re);
    *out_fields = arr;
    return len;
}

void vm_set_record(awk_vm_t *vm, const char *rec) {
    free(vm->record);
    vm->record = strdup(rec ? rec : "");
    for (size_t i = 0; i < vm->nf; i++) {
        free(vm->fields[i]);
    }
    free(vm->fields);
    vm->fields = NULL;
    vm->nf = 0;
    vm->cap_fields = 0;
    const char *fs = get_fmt_var(vm, "FS", " ");
    vm->nf = split_string(vm->record, fs, &vm->fields);
    vm->cap_fields = vm->nf;
    vm_set_num(vm, "NF", (double)vm->nf);
}

static void vm_rebuild_record(awk_vm_t *vm) {
    const char *ofs = get_fmt_var(vm, "OFS", " ");
    size_t ofslen = strlen(ofs);
    size_t total = 1;
    for (size_t i = 0; i < vm->nf; i++) {
        total += strlen(vm->fields[i] ? vm->fields[i] : "") + ofslen;
    }
    char *buf = malloc(total);
    if (!buf) {
        return;
    }
    size_t pos = 0;
    for (size_t i = 0; i < vm->nf; i++) {
        if (i > 0 && ofslen > 0) {
            (void)memcpy(buf + pos, ofs, ofslen);
            pos += ofslen;
        }
        const char *f = vm->fields[i] ? vm->fields[i] : "";
        size_t fl = strlen(f);
        if (fl > 0) {
            (void)memcpy(buf + pos, f, fl);
            pos += fl;
        }
    }
    buf[pos] = '\0';
    free(vm->record);
    vm->record = buf;
}

static void vm_set_field(awk_vm_t *vm, size_t idx, const char *s) {
    if (idx == 0) {
        vm_set_record(vm, s);
        return;
    }
    if (idx > vm->nf) {
        char **nf = realloc(vm->fields, idx * sizeof(*nf));
        if (!nf) {
            return;
        }
        for (size_t i = vm->nf; i < idx; i++) {
            nf[i] = strdup("");
        }
        vm->fields = nf;
        vm->nf = idx;
        vm->cap_fields = idx;
        vm_set_num(vm, "NF", (double)vm->nf);
    }
    free(vm->fields[idx - 1]);
    vm->fields[idx - 1] = strdup(s ? s : "");
    vm_rebuild_record(vm);
}

int vm_read_record(awk_vm_t *vm, FILE *fp, char **out) {
    const char *rs = get_fmt_var(vm, "RS", "\n");
    int ch = fgetc(fp);
    if (ch == EOF) {
        return 0;
    }
    size_t cap = 256;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return -1;
    }
    if (rs[0] == '\0') {
        while (ch == '\n') {
            ch = fgetc(fp);
            if (ch == EOF) {
                free(buf);
                return 0;
            }
        }
        int prev_nl = 0;
        while (ch != EOF) {
            if (ch == '\n' && prev_nl) {
                while (len > 0 && buf[len - 1] == '\n') {
                    len--;
                }
                break;
            }
            prev_nl = (ch == '\n');
            if (len + 2 > cap) {
                size_t nc = cap * 2;
                char *nb = realloc(buf, nc);
                if (!nb) {
                    free(buf);
                    return -1;
                }
                buf = nb;
                cap = nc;
            }
            buf[len++] = (char)ch;
            ch = fgetc(fp);
        }
        if (ch == EOF) {
            while (len > 0 && buf[len - 1] == '\n') {
                len--;
            }
        }
    } else {
        int sep = (unsigned char)rs[0];
        while (ch != EOF && ch != sep) {
            if (len + 2 > cap) {
                size_t nc = cap * 2;
                char *nb = realloc(buf, nc);
                if (!nb) {
                    free(buf);
                    return -1;
                }
                buf = nb;
                cap = nc;
            }
            buf[len++] = (char)ch;
            ch = fgetc(fp);
        }
    }
    buf[len] = '\0';
    *out = buf;
    return 1;
}

static FILE *vm_open_io(awk_vm_t *vm, const char *name, int op) {
    int is_pipe = (op == TOK_PIPE);
    int is_write = (op == TOK_GT || op == TOK_APPEND || op == TOK_PIPE);
    for (size_t i = 0; i < vm->nios; i++) {
        if (strcmp(vm->ios[i].name, name) == 0 &&
            vm->ios[i].is_pipe == is_pipe) {
            return vm->ios[i].fp;
        }
    }
    FILE *fp = NULL;
    if (op == TOK_GT) {
        fp = fopen(name, "wb");
    } else if (op == TOK_APPEND) {
        fp = fopen(name, "ab");
    } else if (op == TOK_LT) {
        fp = fopen(name, "rb");
        is_write = 0;
    } else if (op == TOK_PIPE) {
        fp = popen(name, "w");
    } else if (op == -TOK_PIPE) {
        is_pipe = 1;
        is_write = 0;
        fp = popen(name, "r");
    }
    if (!fp) {
        return NULL;
    }
    if (vm->nios == vm->cap_ios) {
        size_t nc = (vm->cap_ios == 0) ? 8 : vm->cap_ios * 2;
        io_entry_t *ni = realloc(vm->ios, nc * sizeof(*ni));
        if (!ni) {
            if (is_pipe) {
                (void)pclose(fp);
            } else {
                (void)fclose(fp);
            }
            return NULL;
        }
        vm->ios = ni;
        vm->cap_ios = nc;
    }
    size_t idx = vm->nios++;
    vm->ios[idx].name = strdup(name);
    vm->ios[idx].fp = fp;
    vm->ios[idx].is_pipe = is_pipe;
    vm->ios[idx].is_write = is_write;
    return fp;
}

static int vm_close_io(awk_vm_t *vm, const char *name) {
    for (size_t i = 0; i < vm->nios; i++) {
        if (strcmp(vm->ios[i].name, name) == 0) {
            int rc = 0;
            if (vm->ios[i].fp) {
                rc = vm->ios[i].is_pipe ? pclose(vm->ios[i].fp)
                                        : fclose(vm->ios[i].fp);
            }
            free(vm->ios[i].name);
            if (i + 1 < vm->nios) {
                (void)memmove(&vm->ios[i], &vm->ios[i + 1],
                              (vm->nios - i - 1) * sizeof(*vm->ios));
            }
            vm->nios--;
            return rc;
        }
    }
    return -1;
}

static awk_val_t eval_expr(awk_vm_t *vm, const expr_t *e);
static void eval_stmt(awk_vm_t *vm, const stmt_t *s);

static char *eval_subscript(awk_vm_t *vm, expr_t *const *args, size_t nargs) {
    const char *subsep = get_fmt_var(vm, "SUBSEP", "\034");
    size_t sslen = strlen(subsep);
    size_t cap = 64;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    buf[0] = '\0';
    for (size_t i = 0; i < nargs; i++) {
        awk_val_t v = eval_expr(vm, args[i]);
        char *s = val_to_str(vm, &v);
        val_free(&v);
        size_t sl = s ? strlen(s) : 0;
        size_t add = sl + (i > 0 ? sslen : 0);
        if (len + add + 1 > cap) {
            while (len + add + 1 > cap) {
                cap *= 2;
            }
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(s);
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        if (i > 0 && sslen > 0) {
            (void)memcpy(buf + len, subsep, sslen);
            len += sslen;
        }
        if (sl > 0) {
            (void)memcpy(buf + len, s, sl);
            len += sl;
        }
        buf[len] = '\0';
        free(s);
    }
    return buf;
}

static awk_val_t eval_lvalue_get(awk_vm_t *vm, const expr_t *lv) {
    if (lv->kind == EX_VAR) {
        awk_var_t *v = vm_get_var(vm, lv->str);
        return v ? val_copy(&v->val) : val_num(0.0);
    }
    if (lv->kind == EX_INDEX) {
        awk_var_t *v = vm_get_var(vm, lv->str);
        char *key = eval_subscript(vm, lv->args, lv->nargs);
        if (!v || !key) {
            free(key);
            return val_num(0.0);
        }
        arr_entry_t *e = arr_find(&v->arr, key);
        if (!e) {
            vm_arr_set(v, key, (awk_val_t){.kind = VAL_UNINIT});
            free(key);
            return (awk_val_t){.kind = VAL_UNINIT};
        }
        free(key);
        return val_copy(&e->val);
    }
    if (lv->kind == EX_FIELD) {
        awk_val_t iv = eval_expr(vm, lv->a);
        long idx = (long)val_to_num(&iv);
        val_free(&iv);
        if (idx == 0) {
            return val_numstr(vm->record ? vm->record : "");
        }
        if (idx > 0 && (size_t)idx <= vm->nf) {
            return val_numstr(vm->fields[idx - 1]);
        }
        return val_str("");
    }
    return val_num(0.0);
}

static void eval_lvalue_set(awk_vm_t *vm, const expr_t *lv, awk_val_t val) {
    if (lv->kind == EX_VAR) {
        awk_var_t *v = vm_get_var(vm, lv->str);
        if (v) {
            val_free(&v->val);
            v->val = val;
        } else {
            val_free(&val);
        }
        return;
    }
    if (lv->kind == EX_INDEX) {
        awk_var_t *v = vm_get_var(vm, lv->str);
        char *key = eval_subscript(vm, lv->args, lv->nargs);
        if (v && key) {
            vm_arr_set(v, key, val);
        } else {
            val_free(&val);
        }
        free(key);
        return;
    }
    if (lv->kind == EX_FIELD) {
        awk_val_t iv = eval_expr(vm, lv->a);
        long idx = (long)val_to_num(&iv);
        val_free(&iv);
        char *s = val_to_str(vm, &val);
        val_free(&val);
        if (idx >= 0) {
            vm_set_field(vm, (size_t)idx, s ? s : "");
        }
        free(s);
        return;
    }
    val_free(&val);
}

static char *awk_sprintf_eval(awk_vm_t *vm, const char *fmt, awk_val_t *vals,
                              size_t nvals) {
    size_t cap = 256;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    out[0] = '\0';
    size_t vi = 0;
    for (const char *p = fmt; *p != '\0';) {
        if (*p != '%') {
            if (len + 2 > cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[len++] = *p++;
            out[len] = '\0';
            continue;
        }
        if (p[1] == '%') {
            if (len + 2 > cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[len++] = '%';
            out[len] = '\0';
            p += 2;
            continue;
        }
        const char *spec_start = p++;
        char spec[64];
        size_t sl = 0;
        spec[sl++] = '%';
        while (*p != '\0' && strchr("-+ #0", *p) && sl + 2 < sizeof(spec)) {
            spec[sl++] = *p++;
        }
        int dyn_w = 0;
        int w_val = 0;
        if (*p == '*') {
            dyn_w = 1;
            w_val = (vi < nvals) ? (int)val_to_num(&vals[vi++]) : 0;
            if (sl + 2 < sizeof(spec)) {
                spec[sl++] = '*';
            }
            p++;
        } else {
            while (isdigit((unsigned char)*p) && sl + 2 < sizeof(spec)) {
                spec[sl++] = *p++;
            }
        }
        int dyn_p = 0;
        int p_val = 0;
        if (*p == '.') {
            if (sl + 2 < sizeof(spec)) {
                spec[sl++] = *p++;
            } else {
                p++;
            }
            if (*p == '*') {
                dyn_p = 1;
                p_val = (vi < nvals) ? (int)val_to_num(&vals[vi++]) : 0;
                if (sl + 2 < sizeof(spec)) {
                    spec[sl++] = '*';
                }
                p++;
            } else {
                while (isdigit((unsigned char)*p) && sl + 2 < sizeof(spec)) {
                    spec[sl++] = *p++;
                }
            }
        }
        char conv = *p ? *p++ : '\0';
        awk_val_t arg = (vi < nvals) ? vals[vi++] : val_num(0.0);
        char chunk[512];
        char *heap_chunk = NULL;
        const char *piece = chunk;
        if (conv == 's') {
            spec[sl++] = 's';
            spec[sl] = '\0';
            char *sv = val_to_str(vm, &arg);
            size_t need = (sv ? strlen(sv) : 0) + 128;
            heap_chunk = malloc(need);
            if (heap_chunk) {
                if (dyn_w && dyn_p) {
                    (void)snprintf(heap_chunk, need, spec, w_val, p_val,
                                   sv ? sv : "");
                } else if (dyn_w) {
                    (void)snprintf(heap_chunk, need, spec, w_val, sv ? sv : "");
                } else if (dyn_p) {
                    (void)snprintf(heap_chunk, need, spec, p_val, sv ? sv : "");
                } else {
                    (void)snprintf(heap_chunk, need, spec, sv ? sv : "");
                }
                piece = heap_chunk;
            } else {
                chunk[0] = '\0';
            }
            free(sv);
        } else if (conv == 'c') {
            spec[sl++] = 'c';
            spec[sl] = '\0';
            int cv = 0;
            if (arg.kind == VAL_STR) {
                cv = (arg.str && arg.str[0]) ? (unsigned char)arg.str[0] : 0;
            } else {
                cv = (int)val_to_num(&arg);
            }
            (void)snprintf(chunk, sizeof(chunk), spec, cv);
        } else if (conv == 'd' || conv == 'i') {
            spec[sl++] = 'l';
            spec[sl++] = 'l';
            spec[sl++] = conv;
            spec[sl] = '\0';
            long long iv = (long long)val_to_num(&arg);
            if (dyn_w && dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, p_val, iv);
            } else if (dyn_w) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, iv);
            } else if (dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, p_val, iv);
            } else {
                (void)snprintf(chunk, sizeof(chunk), spec, iv);
            }
        } else if (conv == 'u' || conv == 'o' || conv == 'x' || conv == 'X') {
            spec[sl++] = 'l';
            spec[sl++] = 'l';
            spec[sl++] = conv;
            spec[sl] = '\0';
            unsigned long long uv =
                (unsigned long long)(long long)val_to_num(&arg);
            if (dyn_w && dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, p_val, uv);
            } else if (dyn_w) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, uv);
            } else if (dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, p_val, uv);
            } else {
                (void)snprintf(chunk, sizeof(chunk), spec, uv);
            }
        } else if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' ||
                   conv == 'g' || conv == 'G') {
            spec[sl++] = conv;
            spec[sl] = '\0';
            double dv = val_to_num(&arg);
            if (dyn_w && dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, p_val, dv);
            } else if (dyn_w) {
                (void)snprintf(chunk, sizeof(chunk), spec, w_val, dv);
            } else if (dyn_p) {
                (void)snprintf(chunk, sizeof(chunk), spec, p_val, dv);
            } else {
                (void)snprintf(chunk, sizeof(chunk), spec, dv);
            }
        } else {
            size_t rl = (size_t)(p - spec_start);
            if (rl >= sizeof(chunk)) {
                rl = sizeof(chunk) - 1;
            }
            (void)memcpy(chunk, spec_start, rl);
            chunk[rl] = '\0';
        }
        size_t plen = strlen(piece);
        if (len + plen + 1 > cap) {
            while (len + plen + 1 > cap) {
                cap *= 2;
            }
            char *nb = realloc(out, cap);
            if (!nb) {
                free(heap_chunk);
                free(out);
                return NULL;
            }
            out = nb;
        }
        (void)memcpy(out + len, piece, plen);
        len += plen;
        out[len] = '\0';
        free(heap_chunk);
    }
    return out;
}

static char *awk_sub_impl(const char *ere, const char *repl, const char *in,
                          int global, int *count_out) {
    regex_t re;
    *count_out = 0;
    if (regcomp(&re, ere, REG_EXTENDED) != 0) {
        return strdup(in);
    }
    size_t cap = strlen(in) + 64;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        regfree(&re);
        return NULL;
    }
    const char *p = in;
    int eflags = 0;
    for (;;) {
        regmatch_t pm;
        if (regexec(&re, p, 1, &pm, eflags) != 0) {
            size_t rem = strlen(p);
            if (len + rem + 1 > cap) {
                cap = len + rem + 1;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    regfree(&re);
                    return NULL;
                }
                out = nb;
            }
            (void)memcpy(out + len, p, rem);
            len += rem;
            break;
        }
        (*count_out)++;
        size_t pre = (size_t)pm.rm_so;
        size_t mlen = (size_t)(pm.rm_eo - pm.rm_so);
        if (len + pre + 1 > cap) {
            cap = (len + pre + 64) * 2;
            char *nb = realloc(out, cap);
            if (!nb) {
                free(out);
                regfree(&re);
                return NULL;
            }
            out = nb;
        }
        if (pre > 0) {
            (void)memcpy(out + len, p, pre);
            len += pre;
        }
        for (const char *r = repl; *r != '\0'; r++) {
            if (*r == '\\' && (r[1] == '&' || r[1] == '\\')) {
                r++;
                if (len + 2 > cap) {
                    cap *= 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        regfree(&re);
                        return NULL;
                    }
                    out = nb;
                }
                out[len++] = *r;
            } else if (*r == '&') {
                if (len + mlen + 1 > cap) {
                    cap = (len + mlen + 64) * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        regfree(&re);
                        return NULL;
                    }
                    out = nb;
                }
                if (mlen > 0) {
                    (void)memcpy(out + len, p + pm.rm_so, mlen);
                    len += mlen;
                }
            } else {
                if (len + 2 > cap) {
                    cap *= 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        regfree(&re);
                        return NULL;
                    }
                    out = nb;
                }
                out[len++] = *r;
            }
        }
        p += pm.rm_eo;
        eflags = REG_NOTBOL;
        if (!global) {
            size_t rem = strlen(p);
            if (len + rem + 1 > cap) {
                cap = len + rem + 1;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    regfree(&re);
                    return NULL;
                }
                out = nb;
            }
            (void)memcpy(out + len, p, rem);
            len += rem;
            break;
        }
        if (mlen == 0) {
            if (*p == '\0') {
                break;
            }
            if (len + 2 > cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    regfree(&re);
                    return NULL;
                }
                out = nb;
            }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    regfree(&re);
    return out;
}

static int regex_matches(const char *str, const char *ere) {
    regex_t re;
    if (regcomp(&re, ere ? ere : "", REG_EXTENDED | REG_NOSUB) != 0) {
        return 0;
    }
    int ok = (regexec(&re, str ? str : "", 0, NULL, 0) == 0);
    regfree(&re);
    return ok;
}

static awk_val_t call_user_func(awk_vm_t *vm, const func_t *fn,
                                expr_t *const *args, size_t nargs) {
    frame_t fr = {0};
    fr.parent = vm->frame;
    fr.nlocals = fn->nparams;
    fr.names = fn->params;
    if (fn->nparams > 0) {
        fr.locals = calloc(fn->nparams, sizeof(*fr.locals));
        fr.arr_refs = calloc(fn->nparams, sizeof(*fr.arr_refs));
    }
    for (size_t i = 0; i < fn->nparams; i++) {
        if (i < nargs) {
            if (args[i]->kind == EX_VAR) {
                awk_var_t *gv = vm_get_var(vm, args[i]->str);
                if (gv && gv->is_arr) {
                    fr.arr_refs[i] = gv;
                    continue;
                }
            }
            fr.locals[i].val = eval_expr(vm, args[i]);
        }
    }
    vm->frame = &fr;
    eval_stmt(vm, fn->body);
    vm->frame = fr.parent;

    awk_val_t ret = val_num(0.0);
    if (vm->flow == FLOW_RETURN) {
        ret = vm->ret_val;
        vm->ret_val = (awk_val_t){0};
        vm->flow = FLOW_NONE;
    }
    for (size_t i = 0; i < fr.nlocals; i++) {
        val_free(&fr.locals[i].val);
        arr_free(&fr.locals[i].arr);
    }
    free(fr.locals);
    free(fr.arr_refs);
    return ret;
}

static awk_val_t eval_call(awk_vm_t *vm, const expr_t *e) {
    const char *fn = e->str;
    /* Built-ins needing special lvalue / array handling first */
    if (strcmp(fn, "sub") == 0 || strcmp(fn, "gsub") == 0) {
        int global = (fn[0] == 'g');
        if (e->nargs < 2) {
            return val_num(0.0);
        }
        char *pat = NULL;
        if (e->args[0]->kind == EX_REGEX) {
            pat = strdup(e->args[0]->str);
        } else {
            awk_val_t pv = eval_expr(vm, e->args[0]);
            pat = val_to_str(vm, &pv);
            val_free(&pv);
        }
        awk_val_t rv = eval_expr(vm, e->args[1]);
        char *repl = val_to_str(vm, &rv);
        val_free(&rv);
        char *target = NULL;
        if (e->nargs >= 3) {
            awk_val_t tv = eval_lvalue_get(vm, e->args[2]);
            target = val_to_str(vm, &tv);
            val_free(&tv);
        } else {
            target = strdup(vm->record ? vm->record : "");
        }
        int cnt = 0;
        char *res = awk_sub_impl(pat ? pat : "", repl ? repl : "",
                                 target ? target : "", global, &cnt);
        if (res) {
            if (e->nargs >= 3) {
                eval_lvalue_set(vm, e->args[2], val_str(res));
            } else {
                vm_set_record(vm, res);
            }
            free(res);
        }
        free(pat);
        free(repl);
        free(target);
        return val_num((double)cnt);
    }
    if (strcmp(fn, "split") == 0) {
        if (e->nargs < 2 || e->args[1]->kind != EX_VAR) {
            return val_num(0.0);
        }
        awk_val_t sv = eval_expr(vm, e->args[0]);
        char *s = val_to_str(vm, &sv);
        val_free(&sv);
        char *fs = NULL;
        if (e->nargs >= 3) {
            if (e->args[2]->kind == EX_REGEX) {
                fs = strdup(e->args[2]->str);
            } else {
                awk_val_t fv = eval_expr(vm, e->args[2]);
                fs = val_to_str(vm, &fv);
                val_free(&fv);
            }
        } else {
            fs = strdup(get_fmt_var(vm, "FS", " "));
        }
        awk_var_t *arr = vm_get_var(vm, e->args[1]->str);
        if (arr) {
            arr_free(&arr->arr);
            arr->is_arr = 1;
        }
        char **parts = NULL;
        size_t np = split_string(s ? s : "", fs ? fs : " ", &parts);
        for (size_t i = 0; i < np; i++) {
            char key[32];
            (void)snprintf(key, sizeof(key), "%zu", i + 1);
            vm_arr_set(arr, key, val_numstr(parts[i]));
            free(parts[i]);
        }
        free(parts);
        free(s);
        free(fs);
        return val_num((double)np);
    }
    if (strcmp(fn, "length") == 0) {
        if (e->nargs == 0) {
            return val_num((double)strlen(vm->record ? vm->record : ""));
        }
        if (e->args[0]->kind == EX_VAR) {
            awk_var_t *v = vm_get_var(vm, e->args[0]->str);
            if (v && v->is_arr) {
                return val_num((double)v->arr.len);
            }
        }
        awk_val_t sv = eval_expr(vm, e->args[0]);
        char *s = val_to_str(vm, &sv);
        val_free(&sv);
        size_t l = s ? strlen(s) : 0;
        free(s);
        return val_num((double)l);
    }
    if (strcmp(fn, "match") == 0) {
        if (e->nargs < 2) {
            return val_num(0.0);
        }
        awk_val_t sv = eval_expr(vm, e->args[0]);
        char *s = val_to_str(vm, &sv);
        val_free(&sv);
        char *pat = NULL;
        if (e->args[1]->kind == EX_REGEX) {
            pat = strdup(e->args[1]->str);
        } else {
            awk_val_t pv = eval_expr(vm, e->args[1]);
            pat = val_to_str(vm, &pv);
            val_free(&pv);
        }
        regex_t re;
        int rstart = 0;
        int rlen = -1;
        if (regcomp(&re, pat ? pat : "", REG_EXTENDED) == 0) {
            regmatch_t pm;
            if (regexec(&re, s ? s : "", 1, &pm, 0) == 0) {
                rstart = (int)pm.rm_so + 1;
                rlen = (int)(pm.rm_eo - pm.rm_so);
            }
            regfree(&re);
        }
        vm_set_num(vm, "RSTART", (double)rstart);
        vm_set_num(vm, "RLENGTH", (double)rlen);
        free(s);
        free(pat);
        return val_num((double)rstart);
    }

    /* Check user-defined functions */
    for (size_t i = 0; i < vm->prog->nfuncs; i++) {
        if (strcmp(vm->prog->funcs[i].name, fn) == 0) {
            return call_user_func(vm, &vm->prog->funcs[i], e->args, e->nargs);
        }
    }

    /* Evaluate arguments for standard built-ins */
    awk_val_t *vals = NULL;
    if (e->nargs > 0) {
        vals = calloc(e->nargs, sizeof(*vals));
        for (size_t i = 0; i < e->nargs; i++) {
            vals[i] = eval_expr(vm, e->args[i]);
        }
    }

    awk_val_t res = val_num(0.0);
    if (strcmp(fn, "int") == 0 && e->nargs >= 1) {
        double d = val_to_num(&vals[0]);
        res = val_num(d >= 0.0 ? floor(d) : ceil(d));
    } else if (strcmp(fn, "sqrt") == 0 && e->nargs >= 1) {
        res = val_num(sqrt(val_to_num(&vals[0])));
    } else if (strcmp(fn, "sin") == 0 && e->nargs >= 1) {
        res = val_num(sin(val_to_num(&vals[0])));
    } else if (strcmp(fn, "cos") == 0 && e->nargs >= 1) {
        res = val_num(cos(val_to_num(&vals[0])));
    } else if (strcmp(fn, "exp") == 0 && e->nargs >= 1) {
        res = val_num(exp(val_to_num(&vals[0])));
    } else if (strcmp(fn, "log") == 0 && e->nargs >= 1) {
        res = val_num(log(val_to_num(&vals[0])));
    } else if (strcmp(fn, "atan2") == 0 && e->nargs >= 2) {
        res = val_num(atan2(val_to_num(&vals[0]), val_to_num(&vals[1])));
    } else if (strcmp(fn, "rand") == 0) {
        vm->rng_state = vm->rng_state * 1103515245u + 12345u;
        res = val_num((double)((vm->rng_state >> 16) & 0x7fffu) / 32768.0);
    } else if (strcmp(fn, "srand") == 0) {
        unsigned int prev = vm->rng_state;
        vm->rng_state = (e->nargs >= 1) ? (unsigned int)val_to_num(&vals[0])
                                        : (unsigned int)time(NULL);
        res = val_num((double)prev);
    } else if (strcmp(fn, "index") == 0 && e->nargs >= 2) {
        char *s = val_to_str(vm, &vals[0]);
        char *t = val_to_str(vm, &vals[1]);
        const char *f = (s && t) ? strstr(s, t) : NULL;
        res = val_num(f ? (double)((f - s) + 1) : 0.0);
        free(s);
        free(t);
    } else if (strcmp(fn, "substr") == 0 && e->nargs >= 2) {
        char *s = val_to_str(vm, &vals[0]);
        size_t slen = s ? strlen(s) : 0;
        long m = (long)val_to_num(&vals[1]);
        if (m < 1) {
            m = 1;
        }
        size_t start = (size_t)(m - 1);
        if (start >= slen) {
            res = val_str("");
        } else {
            size_t take = slen - start;
            if (e->nargs >= 3) {
                long n = (long)val_to_num(&vals[2]);
                if (n <= 0) {
                    take = 0;
                } else if ((size_t)n < take) {
                    take = (size_t)n;
                }
            }
            char *sub = malloc(take + 1);
            if (sub) {
                if (take > 0) {
                    (void)memcpy(sub, s + start, take);
                }
                sub[take] = '\0';
                res = val_str(sub);
                free(sub);
            }
        }
        free(s);
    } else if (strcmp(fn, "tolower") == 0 && e->nargs >= 1) {
        char *s = val_to_str(vm, &vals[0]);
        if (s) {
            for (size_t i = 0; s[i] != '\0'; i++) {
                s[i] = (char)tolower((unsigned char)s[i]);
            }
            res = val_str(s);
            free(s);
        }
    } else if (strcmp(fn, "toupper") == 0 && e->nargs >= 1) {
        char *s = val_to_str(vm, &vals[0]);
        if (s) {
            for (size_t i = 0; s[i] != '\0'; i++) {
                s[i] = (char)toupper((unsigned char)s[i]);
            }
            res = val_str(s);
            free(s);
        }
    } else if (strcmp(fn, "sprintf") == 0 && e->nargs >= 1) {
        char *fmt = val_to_str(vm, &vals[0]);
        char *formatted =
            awk_sprintf_eval(vm, fmt ? fmt : "", vals + 1, e->nargs - 1);
        res = val_str(formatted ? formatted : "");
        free(fmt);
        free(formatted);
    } else if (strcmp(fn, "close") == 0 && e->nargs >= 1) {
        char *nm = val_to_str(vm, &vals[0]);
        res = val_num((double)vm_close_io(vm, nm ? nm : ""));
        free(nm);
    } else if (strcmp(fn, "system") == 0 && e->nargs >= 1) {
        char *cmd = val_to_str(vm, &vals[0]);
        (void)fflush(stdout);
        res = val_num((double)system(cmd ? cmd : ""));
        free(cmd);
    } else if (strcmp(fn, "fflush") == 0) {
        if (e->nargs == 0) {
            (void)fflush(NULL);
        } else {
            char *nm = val_to_str(vm, &vals[0]);
            if (!nm || nm[0] == '\0') {
                (void)fflush(NULL);
            } else {
                for (size_t i = 0; i < vm->nios; i++) {
                    if (strcmp(vm->ios[i].name, nm) == 0 && vm->ios[i].fp) {
                        (void)fflush(vm->ios[i].fp);
                    }
                }
            }
            free(nm);
        }
        res = val_num(0.0);
    }

    for (size_t i = 0; i < e->nargs; i++) {
        val_free(&vals[i]);
    }
    free(vals);
    return res;
}

static awk_val_t eval_expr(awk_vm_t *vm, const expr_t *e) {
    if (!e) {
        return val_num(0.0);
    }
    switch (e->kind) {
    case EX_NUM:
        return val_num(e->num);
    case EX_STR:
        return val_str(e->str);
    case EX_REGEX:
        return val_num(regex_matches(vm->record, e->str) ? 1.0 : 0.0);
    case EX_VAR:
    case EX_INDEX:
    case EX_FIELD:
        return eval_lvalue_get(vm, e);
    case EX_UNARY: {
        awk_val_t v = eval_expr(vm, e->a);
        awk_val_t r = val_num(0.0);
        if (e->op == TOK_PLUS) {
            r = val_num(val_to_num(&v));
        } else if (e->op == TOK_MINUS) {
            r = val_num(-val_to_num(&v));
        } else if (e->op == TOK_NOT) {
            r = val_num(val_truthy(&v) ? 0.0 : 1.0);
        }
        val_free(&v);
        return r;
    }
    case EX_INCDEC: {
        awk_val_t cur = eval_lvalue_get(vm, e->a);
        double old = val_to_num(&cur);
        val_free(&cur);
        double nxt = (e->op == TOK_INC) ? (old + 1.0) : (old - 1.0);
        eval_lvalue_set(vm, e->a, val_num(nxt));
        return val_num(e->prefix ? nxt : old);
    }
    case EX_ASSIGN: {
        awk_val_t rhs = eval_expr(vm, e->b);
        if (e->op == TOK_ASSIGN) {
            awk_val_t cp = val_copy(&rhs);
            eval_lvalue_set(vm, e->a, rhs);
            return cp;
        }
        awk_val_t lhs = eval_lvalue_get(vm, e->a);
        double a = val_to_num(&lhs);
        double b = val_to_num(&rhs);
        val_free(&lhs);
        val_free(&rhs);
        double r = 0.0;
        if (e->op == TOK_ADD_ASSIGN) {
            r = a + b;
        } else if (e->op == TOK_SUB_ASSIGN) {
            r = a - b;
        } else if (e->op == TOK_MUL_ASSIGN) {
            r = a * b;
        } else if (e->op == TOK_DIV_ASSIGN) {
            r = (b != 0.0) ? (a / b) : 0.0;
        } else if (e->op == TOK_MOD_ASSIGN) {
            r = (b != 0.0) ? fmod(a, b) : 0.0;
        } else if (e->op == TOK_POW_ASSIGN) {
            r = pow(a, b);
        }
        eval_lvalue_set(vm, e->a, val_num(r));
        return val_num(r);
    }
    case EX_CONCAT: {
        awk_val_t va = eval_expr(vm, e->a);
        awk_val_t vb = eval_expr(vm, e->b);
        char *sa = val_to_str(vm, &va);
        char *sb = val_to_str(vm, &vb);
        val_free(&va);
        val_free(&vb);
        size_t la = sa ? strlen(sa) : 0;
        size_t lb = sb ? strlen(sb) : 0;
        char *out = malloc(la + lb + 1);
        if (out) {
            if (la > 0) {
                (void)memcpy(out, sa, la);
            }
            if (lb > 0) {
                (void)memcpy(out + la, sb, lb);
            }
            out[la + lb] = '\0';
        }
        free(sa);
        free(sb);
        awk_val_t res = val_str(out ? out : "");
        free(out);
        return res;
    }
    case EX_TERNARY: {
        awk_val_t cond = eval_expr(vm, e->a);
        int t = val_truthy(&cond);
        val_free(&cond);
        return eval_expr(vm, t ? e->b : e->c);
    }
    case EX_IN: {
        awk_var_t *v = vm_get_var(vm, e->str);
        char *key = eval_subscript(vm, e->args, e->nargs);
        int found = (v && key && arr_find(&v->arr, key) != NULL);
        free(key);
        return val_num(found ? 1.0 : 0.0);
    }
    case EX_CALL:
        return eval_call(vm, e);
    case EX_GETLINE: {
        FILE *fp = vm->cur_in;
        if (e->op == TOK_LT || e->op == TOK_PIPE) {
            awk_val_t nv = eval_expr(vm, e->b);
            char *nm = val_to_str(vm, &nv);
            val_free(&nv);
            fp = vm_open_io(vm, nm ? nm : "",
                            (e->op == TOK_PIPE) ? -TOK_PIPE : TOK_LT);
            free(nm);
        }
        if (!fp) {
            return val_num(-1.0);
        }
        char *rec = NULL;
        int rr = vm_read_record(vm, fp, &rec);
        if (rr <= 0) {
            return val_num((double)rr);
        }
        if (e->a) {
            eval_lvalue_set(vm, e->a, val_numstr(rec));
        } else {
            vm_set_record(vm, rec);
        }
        if (e->op == 0) {
            awk_var_t *nr = vm_get_var(vm, "NR");
            awk_var_t *fnr = vm_get_var(vm, "FNR");
            vm_set_num(vm, "NR", (nr ? val_to_num(&nr->val) : 0.0) + 1.0);
            vm_set_num(vm, "FNR", (fnr ? val_to_num(&fnr->val) : 0.0) + 1.0);
        }
        free(rec);
        return val_num(1.0);
    }
    case EX_BINARY: {
        if (e->op == TOK_AND) {
            awk_val_t a = eval_expr(vm, e->a);
            int ta = val_truthy(&a);
            val_free(&a);
            if (!ta) {
                return val_num(0.0);
            }
            awk_val_t b = eval_expr(vm, e->b);
            int tb = val_truthy(&b);
            val_free(&b);
            return val_num(tb ? 1.0 : 0.0);
        }
        if (e->op == TOK_OR) {
            awk_val_t a = eval_expr(vm, e->a);
            int ta = val_truthy(&a);
            val_free(&a);
            if (ta) {
                return val_num(1.0);
            }
            awk_val_t b = eval_expr(vm, e->b);
            int tb = val_truthy(&b);
            val_free(&b);
            return val_num(tb ? 1.0 : 0.0);
        }
        if (e->op == TOK_MATCH || e->op == TOK_NOMATCH) {
            awk_val_t a = eval_expr(vm, e->a);
            char *sa = val_to_str(vm, &a);
            val_free(&a);
            char *pat = NULL;
            if (e->b->kind == EX_REGEX) {
                pat = strdup(e->b->str);
            } else {
                awk_val_t b = eval_expr(vm, e->b);
                pat = val_to_str(vm, &b);
                val_free(&b);
            }
            int m = regex_matches(sa, pat);
            free(sa);
            free(pat);
            return val_num(((e->op == TOK_MATCH) ? m : !m) ? 1.0 : 0.0);
        }
        awk_val_t va = eval_expr(vm, e->a);
        awk_val_t vb = eval_expr(vm, e->b);
        awk_val_t res = val_num(0.0);
        if (e->op == TOK_LT || e->op == TOK_LE || e->op == TOK_EQ ||
            e->op == TOK_NE || e->op == TOK_GT || e->op == TOK_GE) {
            int c = val_cmp(vm, &va, &vb);
            int ok = 0;
            if (e->op == TOK_LT) {
                ok = (c < 0);
            } else if (e->op == TOK_LE) {
                ok = (c <= 0);
            } else if (e->op == TOK_EQ) {
                ok = (c == 0);
            } else if (e->op == TOK_NE) {
                ok = (c != 0);
            } else if (e->op == TOK_GT) {
                ok = (c > 0);
            } else {
                ok = (c >= 0);
            }
            res = val_num(ok ? 1.0 : 0.0);
        } else {
            double a = val_to_num(&va);
            double b = val_to_num(&vb);
            if (e->op == TOK_PLUS) {
                res = val_num(a + b);
            } else if (e->op == TOK_MINUS) {
                res = val_num(a - b);
            } else if (e->op == TOK_STAR) {
                res = val_num(a * b);
            } else if (e->op == TOK_SLASH) {
                res = val_num(b != 0.0 ? a / b : 0.0);
            } else if (e->op == TOK_PERCENT) {
                res = val_num(b != 0.0 ? fmod(a, b) : 0.0);
            } else if (e->op == TOK_CARET) {
                res = val_num(pow(a, b));
            }
        }
        val_free(&va);
        val_free(&vb);
        return res;
    }
    }
    return val_num(0.0);
}

static void eval_stmt(awk_vm_t *vm, const stmt_t *s) {
    if (!s || vm->flow != FLOW_NONE) {
        return;
    }
    switch (s->kind) {
    case ST_EMPTY:
        break;
    case ST_BLOCK:
        for (size_t i = 0; i < s->nlist && vm->flow == FLOW_NONE; i++) {
            eval_stmt(vm, s->list[i]);
        }
        break;
    case ST_EXPR: {
        awk_val_t v = eval_expr(vm, s->e1);
        val_free(&v);
        break;
    }
    case ST_IF: {
        awk_val_t c = eval_expr(vm, s->e1);
        int ok = val_truthy(&c);
        val_free(&c);
        if (ok) {
            eval_stmt(vm, s->s1);
        } else if (s->s2) {
            eval_stmt(vm, s->s2);
        }
        break;
    }
    case ST_WHILE:
        while (vm->flow == FLOW_NONE) {
            awk_val_t c = eval_expr(vm, s->e1);
            int ok = val_truthy(&c);
            val_free(&c);
            if (!ok) {
                break;
            }
            eval_stmt(vm, s->s1);
            if (vm->flow == FLOW_BREAK) {
                vm->flow = FLOW_NONE;
                break;
            }
            if (vm->flow == FLOW_CONTINUE) {
                vm->flow = FLOW_NONE;
            }
        }
        break;
    case ST_DO:
        for (;;) {
            eval_stmt(vm, s->s1);
            if (vm->flow == FLOW_BREAK) {
                vm->flow = FLOW_NONE;
                break;
            }
            if (vm->flow == FLOW_CONTINUE) {
                vm->flow = FLOW_NONE;
            }
            if (vm->flow != FLOW_NONE) {
                break;
            }
            awk_val_t c = eval_expr(vm, s->e1);
            int ok = val_truthy(&c);
            val_free(&c);
            if (!ok) {
                break;
            }
        }
        break;
    case ST_FOR:
        if (s->e1) {
            awk_val_t init = eval_expr(vm, s->e1);
            val_free(&init);
        }
        while (vm->flow == FLOW_NONE) {
            if (s->e2) {
                awk_val_t c = eval_expr(vm, s->e2);
                int ok = val_truthy(&c);
                val_free(&c);
                if (!ok) {
                    break;
                }
            }
            eval_stmt(vm, s->s1);
            if (vm->flow == FLOW_BREAK) {
                vm->flow = FLOW_NONE;
                break;
            }
            if (vm->flow == FLOW_CONTINUE) {
                vm->flow = FLOW_NONE;
            }
            if (vm->flow != FLOW_NONE) {
                break;
            }
            if (s->e3) {
                awk_val_t st = eval_expr(vm, s->e3);
                val_free(&st);
            }
        }
        break;
    case ST_FOR_IN: {
        awk_var_t *arr = vm_get_var(vm, s->arr_name);
        if (!arr || arr->arr.len == 0) {
            break;
        }
        size_t nk = arr->arr.len;
        char **keys = calloc(nk, sizeof(*keys));
        if (!keys) {
            break;
        }
        for (size_t i = 0; i < nk; i++) {
            keys[i] = strdup(arr->arr.items[i].key);
        }
        for (size_t i = 0; i < nk && vm->flow == FLOW_NONE; i++) {
            vm_set_str(vm, s->var_name, keys[i] ? keys[i] : "");
            eval_stmt(vm, s->s1);
            if (vm->flow == FLOW_BREAK) {
                vm->flow = FLOW_NONE;
                break;
            }
            if (vm->flow == FLOW_CONTINUE) {
                vm->flow = FLOW_NONE;
            }
        }
        for (size_t i = 0; i < nk; i++) {
            free(keys[i]);
        }
        free(keys);
        break;
    }
    case ST_BREAK:
        vm->flow = FLOW_BREAK;
        break;
    case ST_CONTINUE:
        vm->flow = FLOW_CONTINUE;
        break;
    case ST_NEXT:
        vm->flow = FLOW_NEXT;
        break;
    case ST_EXIT:
        if (s->e1) {
            awk_val_t v = eval_expr(vm, s->e1);
            vm->exit_code = (int)val_to_num(&v);
            val_free(&v);
        }
        vm->exiting = 1;
        vm->flow = FLOW_EXIT;
        break;
    case ST_RETURN:
        val_free(&vm->ret_val);
        vm->ret_val = s->e1 ? eval_expr(vm, s->e1) : val_num(0.0);
        vm->flow = FLOW_RETURN;
        break;
    case ST_DELETE: {
        awk_var_t *arr = vm_get_var(vm, s->arr_name);
        if (!arr) {
            break;
        }
        if (s->e1) {
            char *key = eval_subscript(vm, s->e1->args, s->e1->nargs);
            if (key) {
                arr_delete_key(arr, key);
                free(key);
            }
        } else {
            arr_free(&arr->arr);
        }
        break;
    }
    case ST_PRINT:
    case ST_PRINTF: {
        FILE *out = stdout;
        if (s->redir_expr) {
            awk_val_t rv = eval_expr(vm, s->redir_expr);
            char *nm = val_to_str(vm, &rv);
            val_free(&rv);
            out = vm_open_io(vm, nm ? nm : "", s->redir_op);
            free(nm);
            if (!out) {
                break;
            }
        }
        if (s->kind == ST_PRINT) {
            const char *ofs = get_fmt_var(vm, "OFS", " ");
            const char *ors = get_fmt_var(vm, "ORS", "\n");
            if (s->nargs == 0) {
                (void)fputs(vm->record ? vm->record : "", out);
            } else {
                for (size_t i = 0; i < s->nargs; i++) {
                    if (i > 0) {
                        (void)fputs(ofs, out);
                    }
                    awk_val_t v = eval_expr(vm, s->args[i]);
                    char *sv = val_to_ofmt(vm, &v);
                    val_free(&v);
                    (void)fputs(sv ? sv : "", out);
                    free(sv);
                }
            }
            (void)fputs(ors, out);
        } else if (s->nargs >= 1) {
            awk_val_t *vals = calloc(s->nargs, sizeof(*vals));
            if (vals) {
                for (size_t i = 0; i < s->nargs; i++) {
                    vals[i] = eval_expr(vm, s->args[i]);
                }
                char *fmt = val_to_str(vm, &vals[0]);
                char *formatted = awk_sprintf_eval(vm, fmt ? fmt : "", vals + 1,
                                                   s->nargs - 1);
                if (formatted) {
                    (void)fputs(formatted, out);
                    free(formatted);
                }
                free(fmt);
                for (size_t i = 0; i < s->nargs; i++) {
                    val_free(&vals[i]);
                }
                free(vals);
            }
        }
        break;
    }
    }
}

void vm_run_begin(awk_vm_t *vm) {
    for (size_t i = 0; i < vm->prog->nrules && !vm->exiting; i++) {
        if (vm->prog->rules[i].kind == RULE_BEGIN) {
            eval_stmt(vm, vm->prog->rules[i].action);
        }
    }
}

void vm_run_record(awk_vm_t *vm, const char *rec) {
    vm_set_record(vm, rec);
    vm->flow = FLOW_NONE;
    for (size_t i = 0; i < vm->prog->nrules && !vm->exiting; i++) {
        rule_t *r = &vm->prog->rules[i];
        if (r->kind != RULE_PATTERN) {
            continue;
        }
        int matched = 1;
        if (r->pat2) {
            if (!r->in_range) {
                awk_val_t v1 = eval_expr(vm, r->pat1);
                r->in_range = val_truthy(&v1);
                val_free(&v1);
            }
            matched = r->in_range;
            if (r->in_range) {
                awk_val_t v2 = eval_expr(vm, r->pat2);
                if (val_truthy(&v2)) {
                    r->in_range = 0;
                }
                val_free(&v2);
            }
        } else if (r->pat1) {
            awk_val_t v1 = eval_expr(vm, r->pat1);
            matched = val_truthy(&v1);
            val_free(&v1);
        }
        if (!matched) {
            continue;
        }
        if (r->action) {
            eval_stmt(vm, r->action);
        } else {
            (void)fputs(vm->record ? vm->record : "", stdout);
            (void)fputs(get_fmt_var(vm, "ORS", "\n"), stdout);
        }
        if (vm->flow == FLOW_NEXT) {
            vm->flow = FLOW_NONE;
            break;
        }
        if (vm->flow == FLOW_EXIT) {
            break;
        }
    }
}

void vm_run_end(awk_vm_t *vm) {
    vm->flow = FLOW_NONE;
    int saved_exit = vm->exit_code;
    vm->exiting = 0;
    for (size_t i = 0; i < vm->prog->nrules && !vm->exiting; i++) {
        if (vm->prog->rules[i].kind == RULE_END) {
            eval_stmt(vm, vm->prog->rules[i].action);
        }
    }
    if (!vm->exiting) {
        vm->exit_code = saved_exit;
    }
}
