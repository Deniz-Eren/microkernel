/**
 * @file     expr.c
 * @brief    Ring 3 expr utility targeting POSIX.1-2024 XCU over <regex.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0018 */

struct expr_val {
    int is_num;
    int64_t num;
    char *str;
};

struct ectx {
    char **argv;
    int argc;
    int pos;
    int err;
};

static struct expr_val val_num(int64_t v) {
    char buf[32];
    (void)snprintf(buf, sizeof(buf), "%lld", (long long)v);
    struct expr_val r = {.is_num = 1, .num = v, .str = strdup(buf)};
    return r;
}

static int parse_int(const char *s, int64_t *out) {
    if (!s || s[0] == '\0') {
        return 0;
    }
    const char *p = s;
    if (*p == '-') {
        p++;
    }
    if (!isdigit((unsigned char)*p)) {
        return 0;
    }
    while (isdigit((unsigned char)*p)) {
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    errno = 0;
    long long v = strtoll(s, NULL, 10);
    if (errno == ERANGE) {
        return 0;
    }
    *out = (int64_t)v;
    return 1;
}

static struct expr_val val_str(const char *s) {
    int64_t n = 0;
    int is_n = parse_int(s, &n);
    struct expr_val r = {.is_num = is_n, .num = n, .str = strdup(s ? s : "")};
    return r;
}

static int val_truthy(const struct expr_val *v) {
    if (!v->str || v->str[0] == '\0') {
        return 0;
    }
    if (v->is_num && v->num == 0) {
        return 0;
    }
    return 1;
}

static int peek_is(const struct ectx *c, const char *tok) {
    return c->pos < c->argc && strcmp(c->argv[c->pos], tok) == 0;
}

static struct expr_val parse_or(struct ectx *c);

static struct expr_val parse_atom(struct ectx *c) {
    if (c->pos >= c->argc) {
        c->err = 1;
        return val_str("");
    }
    if (peek_is(c, "(")) {
        c->pos++;
        struct expr_val v = parse_or(c);
        if (!peek_is(c, ")")) {
            c->err = 1;
            return v;
        }
        c->pos++;
        return v;
    }
    return val_str(c->argv[c->pos++]);
}

static struct expr_val parse_match(struct ectx *c) {
    struct expr_val left = parse_atom(c);
    while (!c->err && peek_is(c, ":")) {
        c->pos++;
        struct expr_val right = parse_atom(c);
        if (c->err) {
            free(right.str);
            return left;
        }
        size_t plen = strlen(right.str);
        char *anchored = (char *)malloc(plen + 2u);
        if (!anchored) {
            free(right.str);
            c->err = 1;
            return left;
        }
        anchored[0] = '^';
        memcpy(anchored + 1u, right.str, plen + 1u);
        regex_t re;
        if (regcomp(&re, anchored, 0) != 0) {
            free(anchored);
            free(right.str);
            c->err = 1;
            return left;
        }
        free(anchored);
        regmatch_t pm[2];
        pm[0].rm_so = -1;
        pm[1].rm_so = -1;
        int matched = (regexec(&re, left.str, 2u, pm, 0) == 0);
        int has_sub = (re.re_nsub > 0u);
        regfree(&re);
        free(right.str);

        struct expr_val res;
        if (has_sub) {
            if (matched && pm[1].rm_so >= 0) {
                char *sub = strndup(left.str + pm[1].rm_so,
                                    (size_t)(pm[1].rm_eo - pm[1].rm_so));
                res = val_str(sub ? sub : "");
                free(sub);
            } else {
                res = val_str("");
            }
        } else {
            res = val_num(matched ? (int64_t)(pm[0].rm_eo - pm[0].rm_so) : 0);
        }
        free(left.str);
        left = res;
    }
    return left;
}

static struct expr_val parse_mul(struct ectx *c) {
    struct expr_val left = parse_match(c);
    while (!c->err && (peek_is(c, "*") || peek_is(c, "/") || peek_is(c, "%"))) {
        char op = c->argv[c->pos++][0];
        struct expr_val right = parse_match(c);
        if (!left.is_num || !right.is_num || (op != '*' && right.num == 0)) {
            free(right.str);
            c->err = 1;
            return left;
        }
        int64_t v = 0;
        if (op == '*') {
            v = left.num * right.num;
        } else if (op == '/') {
            v = left.num / right.num;
        } else {
            v = left.num % right.num;
        }
        free(left.str);
        free(right.str);
        left = val_num(v);
    }
    return left;
}

static struct expr_val parse_add(struct ectx *c) {
    struct expr_val left = parse_mul(c);
    while (!c->err && (peek_is(c, "+") || peek_is(c, "-"))) {
        char op = c->argv[c->pos++][0];
        struct expr_val right = parse_mul(c);
        if (!left.is_num || !right.is_num) {
            free(right.str);
            c->err = 1;
            return left;
        }
        int64_t v =
            (op == '+') ? (left.num + right.num) : (left.num - right.num);
        free(left.str);
        free(right.str);
        left = val_num(v);
    }
    return left;
}

static struct expr_val parse_rel(struct ectx *c) {
    struct expr_val left = parse_add(c);
    while (!c->err &&
           (peek_is(c, "=") || peek_is(c, "!=") || peek_is(c, "<") ||
            peek_is(c, "<=") || peek_is(c, ">") || peek_is(c, ">="))) {
        const char *op = c->argv[c->pos++];
        struct expr_val right = parse_add(c);
        int64_t cmp = 0;
        if (left.is_num && right.is_num) {
            cmp =
                (left.num < right.num) ? -1 : ((left.num > right.num) ? 1 : 0);
        } else {
            cmp = strcoll(left.str, right.str);
        }
        int res = 0;
        if (strcmp(op, "=") == 0) {
            res = (cmp == 0);
        } else if (strcmp(op, "!=") == 0) {
            res = (cmp != 0);
        } else if (strcmp(op, "<") == 0) {
            res = (cmp < 0);
        } else if (strcmp(op, "<=") == 0) {
            res = (cmp <= 0);
        } else if (strcmp(op, ">") == 0) {
            res = (cmp > 0);
        } else {
            res = (cmp >= 0);
        }
        free(left.str);
        free(right.str);
        left = val_num(res);
    }
    return left;
}

static struct expr_val parse_and(struct ectx *c) {
    struct expr_val left = parse_rel(c);
    while (!c->err && peek_is(c, "&")) {
        c->pos++;
        struct expr_val right = parse_rel(c);
        if (val_truthy(&left) && val_truthy(&right)) {
            free(right.str);
        } else {
            free(left.str);
            free(right.str);
            left = val_num(0);
        }
    }
    return left;
}

static struct expr_val parse_or(struct ectx *c) {
    struct expr_val left = parse_and(c);
    while (!c->err && peek_is(c, "|")) {
        c->pos++;
        struct expr_val right = parse_and(c);
        if (val_truthy(&left)) {
            free(right.str);
        } else {
            free(left.str);
            left = right;
        }
    }
    return left;
}

int main(int argc, char **argv) {
    int start = 1;
    if (start < argc && strcmp(argv[start], "--") == 0) {
        start++;
    }
    if (start >= argc) {
        fprintf(stderr, "expr: missing operand\n");
        return 2;
    }
    struct ectx c = {.argv = argv, .argc = argc, .pos = start, .err = 0};
    struct expr_val res = parse_or(&c);
    if (c.err || c.pos < argc) {
        fprintf(stderr, "expr: syntax error\n");
        free(res.str);
        return 2;
    }
    puts(res.str ? res.str : "");
    int ok = val_truthy(&res);
    free(res.str);
    return ok ? 0 : 1;
}
