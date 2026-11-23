/**
 * @file     parse.c
 * @brief    Recursive-descent parser and AST for Ring 3 POSIX.1-2024 XCU awk
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "awk.h"

/* need:: COMP_LIBC_0024 */

typedef struct {
    lexer_t lx;
    int err;
    int in_print;
} parser_t;

static expr_t *expr_new(expr_kind_t k) {
    expr_t *e = calloc(1, sizeof(*e));
    if (e) {
        e->kind = k;
    }
    return e;
}

static void expr_free(expr_t *e) {
    if (!e) {
        return;
    }
    free(e->str);
    expr_free(e->a);
    expr_free(e->b);
    expr_free(e->c);
    for (size_t i = 0; i < e->nargs; i++) {
        expr_free(e->args[i]);
    }
    free(e->args);
    free(e);
}

static stmt_t *stmt_new(stmt_kind_t k) {
    stmt_t *s = calloc(1, sizeof(*s));
    if (s) {
        s->kind = k;
    }
    return s;
}

static void stmt_free(stmt_t *s) {
    if (!s) {
        return;
    }
    free(s->var_name);
    free(s->arr_name);
    expr_free(s->e1);
    expr_free(s->e2);
    expr_free(s->e3);
    expr_free(s->redir_expr);
    for (size_t i = 0; i < s->nargs; i++) {
        expr_free(s->args[i]);
    }
    free(s->args);
    stmt_free(s->s1);
    stmt_free(s->s2);
    for (size_t i = 0; i < s->nlist; i++) {
        stmt_free(s->list[i]);
    }
    free(s->list);
    free(s);
}

void prog_free(prog_t *prog) {
    for (size_t i = 0; i < prog->nrules; i++) {
        expr_free(prog->rules[i].pat1);
        expr_free(prog->rules[i].pat2);
        stmt_free(prog->rules[i].action);
    }
    free(prog->rules);
    for (size_t i = 0; i < prog->nfuncs; i++) {
        free(prog->funcs[i].name);
        for (size_t j = 0; j < prog->funcs[i].nparams; j++) {
            free(prog->funcs[i].params[j]);
        }
        free(prog->funcs[i].params);
        stmt_free(prog->funcs[i].body);
    }
    free(prog->funcs);
    (void)memset(prog, 0, sizeof(*prog));
}

static tok_kind_t peek(parser_t *p) {
    return lex_peek(&p->lx)->kind;
}

static void skip_nl(parser_t *p) {
    while (peek(p) == TOK_NL) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
    }
}

static void skip_seps(parser_t *p) {
    while (peek(p) == TOK_NL || peek(p) == TOK_SEMI) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
    }
}

static int expect(parser_t *p, tok_kind_t k) {
    if (peek(p) != k) {
        p->err = 1;
        return -1;
    }
    token_t t = lex_next(&p->lx);
    lex_free_tok(&t);
    return 0;
}

static int is_assign_op(tok_kind_t k) {
    return k == TOK_ASSIGN || k == TOK_ADD_ASSIGN || k == TOK_SUB_ASSIGN ||
           k == TOK_MUL_ASSIGN || k == TOK_DIV_ASSIGN || k == TOK_MOD_ASSIGN ||
           k == TOK_POW_ASSIGN;
}

static int can_start_concat(tok_kind_t k) {
    return k == TOK_NUM || k == TOK_STR || k == TOK_IDENT || k == TOK_LPAREN ||
           k == TOK_DOLLAR || k == TOK_NOT || k == TOK_INC || k == TOK_DEC ||
           k == TOK_GETLINE;
}

static expr_t *parse_expr(parser_t *p);
static stmt_t *parse_stmt(parser_t *p);

static expr_t *parse_arg_list_into(parser_t *p, expr_t *node, tok_kind_t end) {
    int saved = p->in_print;
    p->in_print = 0;
    skip_nl(p);
    if (peek(p) != end) {
        for (;;) {
            expr_t *arg = parse_expr(p);
            if (!arg) {
                p->in_print = saved;
                return NULL;
            }
            expr_t **na = realloc(node->args, (node->nargs + 1) * sizeof(*na));
            if (!na) {
                expr_free(arg);
                p->in_print = saved;
                return NULL;
            }
            node->args = na;
            node->args[node->nargs++] = arg;
            if (peek(p) != TOK_COMMA) {
                break;
            }
            token_t cm = lex_next(&p->lx);
            lex_free_tok(&cm);
            skip_nl(p);
        }
    }
    p->in_print = saved;
    if (expect(p, end) != 0) {
        return NULL;
    }
    return node;
}

static expr_t *parse_primary(parser_t *p) {
    tok_kind_t k = peek(p);
    if (k == TOK_NUM) {
        token_t t = lex_next(&p->lx);
        expr_t *e = expr_new(EX_NUM);
        if (e) {
            e->num = t.num;
        }
        lex_free_tok(&t);
        return e;
    }
    if (k == TOK_STR) {
        token_t t = lex_next(&p->lx);
        expr_t *e = expr_new(EX_STR);
        if (e) {
            e->str = t.text;
            t.text = NULL;
        }
        lex_free_tok(&t);
        return e;
    }
    if (k == TOK_REGEX) {
        token_t t = lex_next(&p->lx);
        expr_t *e = expr_new(EX_REGEX);
        if (e) {
            e->str = t.text;
            t.text = NULL;
        }
        lex_free_tok(&t);
        return e;
    }
    if (k == TOK_GETLINE) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        expr_t *e = expr_new(EX_GETLINE);
        if (!e) {
            return NULL;
        }
        if (peek(p) == TOK_IDENT) {
            token_t v = lex_next(&p->lx);
            expr_t *lv = expr_new(EX_VAR);
            if (lv) {
                lv->str = v.text;
                v.text = NULL;
            }
            lex_free_tok(&v);
            if (lv && peek(p) == TOK_LBRACK) {
                token_t lb = lex_next(&p->lx);
                lex_free_tok(&lb);
                expr_t *ix = expr_new(EX_INDEX);
                if (!ix) {
                    expr_free(lv);
                    expr_free(e);
                    return NULL;
                }
                ix->str = lv->str;
                lv->str = NULL;
                expr_free(lv);
                if (!parse_arg_list_into(p, ix, TOK_RBRACK)) {
                    expr_free(ix);
                    expr_free(e);
                    return NULL;
                }
                lv = ix;
            }
            e->a = lv;
        }
        if (peek(p) == TOK_LT) {
            token_t lt = lex_next(&p->lx);
            lex_free_tok(&lt);
            e->b = parse_primary(p);
            e->op = TOK_LT;
        }
        return e;
    }
    if (k == TOK_LPAREN) {
        token_t lp = lex_next(&p->lx);
        lex_free_tok(&lp);
        int saved = p->in_print;
        p->in_print = 0;
        skip_nl(p);
        expr_t *first = parse_expr(p);
        if (!first) {
            p->in_print = saved;
            return NULL;
        }
        if (peek(p) == TOK_COMMA) {
            expr_t *tup = expr_new(EX_IN);
            if (!tup) {
                expr_free(first);
                p->in_print = saved;
                return NULL;
            }
            tup->args = calloc(1, sizeof(*tup->args));
            if (!tup->args) {
                expr_free(first);
                expr_free(tup);
                p->in_print = saved;
                return NULL;
            }
            tup->args[0] = first;
            tup->nargs = 1;
            while (peek(p) == TOK_COMMA) {
                token_t cm = lex_next(&p->lx);
                lex_free_tok(&cm);
                skip_nl(p);
                expr_t *arg = parse_expr(p);
                if (!arg) {
                    expr_free(tup);
                    p->in_print = saved;
                    return NULL;
                }
                expr_t **na =
                    realloc(tup->args, (tup->nargs + 1) * sizeof(*na));
                if (!na) {
                    expr_free(arg);
                    expr_free(tup);
                    p->in_print = saved;
                    return NULL;
                }
                tup->args = na;
                tup->args[tup->nargs++] = arg;
            }
            p->in_print = saved;
            if (expect(p, TOK_RPAREN) != 0 || expect(p, TOK_IN) != 0 ||
                peek(p) != TOK_IDENT) {
                expr_free(tup);
                p->err = 1;
                return NULL;
            }
            token_t arr = lex_next(&p->lx);
            tup->str = arr.text;
            arr.text = NULL;
            lex_free_tok(&arr);
            return tup;
        }
        p->in_print = saved;
        if (expect(p, TOK_RPAREN) != 0) {
            expr_free(first);
            return NULL;
        }
        return first;
    }
    if (k == TOK_IDENT) {
        token_t t = lex_next(&p->lx);
        if (peek(p) == TOK_LPAREN) {
            token_t lp = lex_next(&p->lx);
            lex_free_tok(&lp);
            expr_t *call = expr_new(EX_CALL);
            if (!call) {
                lex_free_tok(&t);
                return NULL;
            }
            call->str = t.text;
            t.text = NULL;
            lex_free_tok(&t);
            if (!parse_arg_list_into(p, call, TOK_RPAREN)) {
                expr_free(call);
                return NULL;
            }
            return call;
        }
        if (peek(p) == TOK_LBRACK) {
            token_t lb = lex_next(&p->lx);
            lex_free_tok(&lb);
            expr_t *ix = expr_new(EX_INDEX);
            if (!ix) {
                lex_free_tok(&t);
                return NULL;
            }
            ix->str = t.text;
            t.text = NULL;
            lex_free_tok(&t);
            if (!parse_arg_list_into(p, ix, TOK_RBRACK)) {
                expr_free(ix);
                return NULL;
            }
            return ix;
        }
        expr_t *v = expr_new(EX_VAR);
        if (v) {
            v->str = t.text;
            t.text = NULL;
        }
        lex_free_tok(&t);
        return v;
    }
    p->err = 1;
    return NULL;
}

static expr_t *parse_postfix(parser_t *p) {
    expr_t *e = parse_primary(p);
    if (!e) {
        return NULL;
    }
    while (peek(p) == TOK_INC || peek(p) == TOK_DEC) {
        token_t t = lex_next(&p->lx);
        expr_t *u = expr_new(EX_INCDEC);
        if (!u) {
            expr_free(e);
            lex_free_tok(&t);
            return NULL;
        }
        u->op = t.kind;
        u->prefix = 0;
        u->a = e;
        lex_free_tok(&t);
        e = u;
    }
    return e;
}

static expr_t *parse_unary(parser_t *p);

static expr_t *parse_field(parser_t *p) {
    if (peek(p) == TOK_DOLLAR) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        expr_t *e = expr_new(EX_FIELD);
        if (!e) {
            return NULL;
        }
        e->a = parse_unary(p);
        if (!e->a) {
            expr_free(e);
            return NULL;
        }
        while (peek(p) == TOK_INC || peek(p) == TOK_DEC) {
            token_t inc = lex_next(&p->lx);
            expr_t *u = expr_new(EX_INCDEC);
            if (!u) {
                expr_free(e);
                lex_free_tok(&inc);
                return NULL;
            }
            u->op = inc.kind;
            u->prefix = 0;
            u->a = e;
            lex_free_tok(&inc);
            e = u;
        }
        return e;
    }
    return parse_postfix(p);
}

static expr_t *parse_pow(parser_t *p) {
    expr_t *left = parse_field(p);
    if (!left) {
        return NULL;
    }
    if (peek(p) == TOK_CARET) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        expr_t *right = parse_unary(p);
        if (!right) {
            expr_free(left);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        b->op = TOK_CARET;
        b->a = left;
        b->b = right;
        return b;
    }
    return left;
}

static expr_t *parse_unary(parser_t *p) {
    tok_kind_t k = peek(p);
    if (k == TOK_PLUS || k == TOK_MINUS || k == TOK_NOT) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        expr_t *e = expr_new(EX_UNARY);
        if (!e) {
            return NULL;
        }
        e->op = k;
        e->a = parse_unary(p);
        if (!e->a) {
            expr_free(e);
            return NULL;
        }
        return e;
    }
    if (k == TOK_INC || k == TOK_DEC) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        expr_t *e = expr_new(EX_INCDEC);
        if (!e) {
            return NULL;
        }
        e->op = k;
        e->prefix = 1;
        e->a = parse_field(p);
        if (!e->a) {
            expr_free(e);
            return NULL;
        }
        return e;
    }
    return parse_pow(p);
}

static expr_t *parse_mul(parser_t *p) {
    expr_t *left = parse_unary(p);
    if (!left) {
        return NULL;
    }
    while (peek(p) == TOK_STAR || peek(p) == TOK_SLASH ||
           peek(p) == TOK_PERCENT) {
        token_t t = lex_next(&p->lx);
        expr_t *right = parse_unary(p);
        if (!right) {
            expr_free(left);
            lex_free_tok(&t);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            lex_free_tok(&t);
            return NULL;
        }
        b->op = t.kind;
        b->a = left;
        b->b = right;
        lex_free_tok(&t);
        left = b;
    }
    return left;
}

static expr_t *parse_add(parser_t *p) {
    expr_t *left = parse_mul(p);
    if (!left) {
        return NULL;
    }
    while (peek(p) == TOK_PLUS || peek(p) == TOK_MINUS) {
        token_t t = lex_next(&p->lx);
        expr_t *right = parse_mul(p);
        if (!right) {
            expr_free(left);
            lex_free_tok(&t);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            lex_free_tok(&t);
            return NULL;
        }
        b->op = t.kind;
        b->a = left;
        b->b = right;
        lex_free_tok(&t);
        left = b;
    }
    return left;
}

static expr_t *parse_concat(parser_t *p) {
    expr_t *left = parse_add(p);
    if (!left) {
        return NULL;
    }
    while (can_start_concat(peek(p))) {
        expr_t *right = parse_add(p);
        if (!right) {
            expr_free(left);
            return NULL;
        }
        expr_t *c = expr_new(EX_CONCAT);
        if (!c) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        c->a = left;
        c->b = right;
        left = c;
    }
    return left;
}

static expr_t *parse_rel(parser_t *p) {
    expr_t *left = parse_concat(p);
    if (!left) {
        return NULL;
    }
    tok_kind_t k = peek(p);
    if (k == TOK_PIPE && !p->in_print) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        if (peek(p) == TOK_GETLINE) {
            expr_t *gl = parse_primary(p);
            if (!gl) {
                expr_free(left);
                return NULL;
            }
            gl->op = TOK_PIPE;
            gl->b = left;
            return gl;
        }
        expr_free(left);
        p->err = 1;
        return NULL;
    }
    while (k == TOK_LT || k == TOK_LE || k == TOK_EQ || k == TOK_NE ||
           k == TOK_GE || (k == TOK_GT && !p->in_print)) {
        token_t t = lex_next(&p->lx);
        expr_t *right = parse_concat(p);
        if (!right) {
            expr_free(left);
            lex_free_tok(&t);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            lex_free_tok(&t);
            return NULL;
        }
        b->op = t.kind;
        b->a = left;
        b->b = right;
        lex_free_tok(&t);
        left = b;
        k = peek(p);
    }
    return left;
}

static expr_t *parse_match(parser_t *p) {
    expr_t *left = parse_rel(p);
    if (!left) {
        return NULL;
    }
    while (peek(p) == TOK_MATCH || peek(p) == TOK_NOMATCH) {
        token_t t = lex_next(&p->lx);
        expr_t *right = parse_rel(p);
        if (!right) {
            expr_free(left);
            lex_free_tok(&t);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            lex_free_tok(&t);
            return NULL;
        }
        b->op = t.kind;
        b->a = left;
        b->b = right;
        lex_free_tok(&t);
        left = b;
    }
    return left;
}

static expr_t *parse_in(parser_t *p) {
    expr_t *left = parse_match(p);
    if (!left) {
        return NULL;
    }
    if (peek(p) == TOK_IN) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        if (peek(p) != TOK_IDENT) {
            expr_free(left);
            p->err = 1;
            return NULL;
        }
        token_t arr = lex_next(&p->lx);
        expr_t *in = expr_new(EX_IN);
        if (!in) {
            expr_free(left);
            lex_free_tok(&arr);
            return NULL;
        }
        in->str = arr.text;
        arr.text = NULL;
        lex_free_tok(&arr);
        in->args = calloc(1, sizeof(*in->args));
        if (!in->args) {
            expr_free(left);
            expr_free(in);
            return NULL;
        }
        in->args[0] = left;
        in->nargs = 1;
        return in;
    }
    return left;
}

static expr_t *parse_and(parser_t *p) {
    expr_t *left = parse_in(p);
    if (!left) {
        return NULL;
    }
    while (peek(p) == TOK_AND) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        skip_nl(p);
        expr_t *right = parse_in(p);
        if (!right) {
            expr_free(left);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        b->op = TOK_AND;
        b->a = left;
        b->b = right;
        left = b;
    }
    return left;
}

static expr_t *parse_or(parser_t *p) {
    expr_t *left = parse_and(p);
    if (!left) {
        return NULL;
    }
    while (peek(p) == TOK_OR) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        skip_nl(p);
        expr_t *right = parse_and(p);
        if (!right) {
            expr_free(left);
            return NULL;
        }
        expr_t *b = expr_new(EX_BINARY);
        if (!b) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        b->op = TOK_OR;
        b->a = left;
        b->b = right;
        left = b;
    }
    return left;
}

static expr_t *parse_ternary(parser_t *p) {
    expr_t *cond = parse_or(p);
    if (!cond) {
        return NULL;
    }
    if (peek(p) == TOK_QUEST) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        skip_nl(p);
        expr_t *yes = parse_expr(p);
        skip_nl(p);
        if (!yes || expect(p, TOK_COLON) != 0) {
            expr_free(cond);
            expr_free(yes);
            return NULL;
        }
        skip_nl(p);
        expr_t *no = parse_ternary(p);
        if (!no) {
            expr_free(cond);
            expr_free(yes);
            return NULL;
        }
        expr_t *q = expr_new(EX_TERNARY);
        if (!q) {
            expr_free(cond);
            expr_free(yes);
            expr_free(no);
            return NULL;
        }
        q->a = cond;
        q->b = yes;
        q->c = no;
        return q;
    }
    return cond;
}

static expr_t *parse_expr(parser_t *p) {
    expr_t *left = parse_ternary(p);
    if (!left) {
        return NULL;
    }
    if (is_assign_op(peek(p))) {
        token_t t = lex_next(&p->lx);
        skip_nl(p);
        expr_t *right = parse_expr(p);
        if (!right) {
            expr_free(left);
            lex_free_tok(&t);
            return NULL;
        }
        expr_t *a = expr_new(EX_ASSIGN);
        if (!a) {
            expr_free(left);
            expr_free(right);
            lex_free_tok(&t);
            return NULL;
        }
        a->op = t.kind;
        a->a = left;
        a->b = right;
        lex_free_tok(&t);
        return a;
    }
    return left;
}

static int is_stmt_end(tok_kind_t k) {
    return k == TOK_EOF || k == TOK_NL || k == TOK_SEMI || k == TOK_RBRACE;
}

static stmt_t *parse_block(parser_t *p) {
    if (expect(p, TOK_LBRACE) != 0) {
        return NULL;
    }
    stmt_t *blk = stmt_new(ST_BLOCK);
    if (!blk) {
        return NULL;
    }
    skip_seps(p);
    while (peek(p) != TOK_RBRACE && peek(p) != TOK_EOF) {
        stmt_t *s = parse_stmt(p);
        if (!s) {
            stmt_free(blk);
            return NULL;
        }
        stmt_t **nl = realloc(blk->list, (blk->nlist + 1) * sizeof(*nl));
        if (!nl) {
            stmt_free(s);
            stmt_free(blk);
            return NULL;
        }
        blk->list = nl;
        blk->list[blk->nlist++] = s;
        skip_seps(p);
    }
    if (expect(p, TOK_RBRACE) != 0) {
        stmt_free(blk);
        return NULL;
    }
    return blk;
}

static stmt_t *parse_print_stmt(parser_t *p, stmt_kind_t kind) {
    token_t kw = lex_next(&p->lx);
    lex_free_tok(&kw);
    stmt_t *st = stmt_new(kind);
    if (!st) {
        return NULL;
    }
    int saved = p->in_print;
    p->in_print = 1;
    if (!is_stmt_end(peek(p)) && peek(p) != TOK_GT && peek(p) != TOK_APPEND &&
        peek(p) != TOK_PIPE) {
        for (;;) {
            expr_t *arg = parse_expr(p);
            if (!arg) {
                p->in_print = saved;
                stmt_free(st);
                return NULL;
            }
            expr_t **na = realloc(st->args, (st->nargs + 1) * sizeof(*na));
            if (!na) {
                expr_free(arg);
                p->in_print = saved;
                stmt_free(st);
                return NULL;
            }
            st->args = na;
            st->args[st->nargs++] = arg;
            if (peek(p) != TOK_COMMA) {
                break;
            }
            token_t cm = lex_next(&p->lx);
            lex_free_tok(&cm);
            skip_nl(p);
        }
    }
    p->in_print = saved;
    if (peek(p) == TOK_GT || peek(p) == TOK_APPEND || peek(p) == TOK_PIPE) {
        token_t rd = lex_next(&p->lx);
        st->redir_op = rd.kind;
        lex_free_tok(&rd);
        st->redir_expr = parse_expr(p);
        if (!st->redir_expr) {
            stmt_free(st);
            return NULL;
        }
    }
    return st;
}

static stmt_t *parse_stmt(parser_t *p) {
    skip_nl(p);
    tok_kind_t k = peek(p);
    if (k == TOK_SEMI) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        return stmt_new(ST_EMPTY);
    }
    if (k == TOK_LBRACE) {
        return parse_block(p);
    }
    if (k == TOK_IF) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        stmt_t *st = stmt_new(ST_IF);
        if (!st || expect(p, TOK_LPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        st->e1 = parse_expr(p);
        if (!st->e1 || expect(p, TOK_RPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        skip_nl(p);
        st->s1 = parse_stmt(p);
        if (!st->s1) {
            stmt_free(st);
            return NULL;
        }
        while (peek(p) == TOK_SEMI || peek(p) == TOK_NL) {
            /* Peek ahead to see if else follows */
            lexer_t snap = p->lx;
            if (snap.has_cur && snap.cur.text) {
                snap.cur.text = strdup(snap.cur.text);
            }
            skip_seps(p);
            if (peek(p) == TOK_ELSE) {
                lex_free_tok(&snap.cur);
                break;
            }
            lex_free_tok(&p->lx.cur);
            p->lx = snap;
            break;
        }
        if (peek(p) == TOK_ELSE) {
            token_t el = lex_next(&p->lx);
            lex_free_tok(&el);
            skip_nl(p);
            st->s2 = parse_stmt(p);
            if (!st->s2) {
                stmt_free(st);
                return NULL;
            }
        }
        return st;
    }
    if (k == TOK_WHILE) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        stmt_t *st = stmt_new(ST_WHILE);
        if (!st || expect(p, TOK_LPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        st->e1 = parse_expr(p);
        if (!st->e1 || expect(p, TOK_RPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        skip_nl(p);
        st->s1 = parse_stmt(p);
        if (!st->s1) {
            stmt_free(st);
            return NULL;
        }
        return st;
    }
    if (k == TOK_DO) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        stmt_t *st = stmt_new(ST_DO);
        if (!st) {
            return NULL;
        }
        skip_nl(p);
        st->s1 = parse_stmt(p);
        skip_seps(p);
        if (!st->s1 || expect(p, TOK_WHILE) != 0 ||
            expect(p, TOK_LPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        st->e1 = parse_expr(p);
        if (!st->e1 || expect(p, TOK_RPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        return st;
    }
    if (k == TOK_FOR) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        if (expect(p, TOK_LPAREN) != 0) {
            return NULL;
        }
        skip_nl(p);
        /* Check for-in: IDENT in IDENT ) */
        if (peek(p) == TOK_IDENT) {
            lexer_t snap = p->lx;
            if (snap.has_cur && snap.cur.text) {
                snap.cur.text = strdup(snap.cur.text);
            }
            token_t id1 = lex_next(&p->lx);
            if (peek(p) == TOK_IN) {
                lex_free_tok(&snap.cur);
                token_t kw_in = lex_next(&p->lx);
                lex_free_tok(&kw_in);
                if (peek(p) != TOK_IDENT) {
                    lex_free_tok(&id1);
                    p->err = 1;
                    return NULL;
                }
                token_t id2 = lex_next(&p->lx);
                stmt_t *st = stmt_new(ST_FOR_IN);
                if (!st) {
                    lex_free_tok(&id1);
                    lex_free_tok(&id2);
                    return NULL;
                }
                st->var_name = id1.text;
                st->arr_name = id2.text;
                if (expect(p, TOK_RPAREN) != 0) {
                    stmt_free(st);
                    return NULL;
                }
                skip_nl(p);
                st->s1 = parse_stmt(p);
                if (!st->s1) {
                    stmt_free(st);
                    return NULL;
                }
                return st;
            }
            lex_free_tok(&id1);
            lex_free_tok(&p->lx.cur);
            p->lx = snap;
        }
        stmt_t *st = stmt_new(ST_FOR);
        if (!st) {
            return NULL;
        }
        if (peek(p) != TOK_SEMI) {
            st->e1 = parse_expr(p);
            if (!st->e1) {
                stmt_free(st);
                return NULL;
            }
        }
        if (expect(p, TOK_SEMI) != 0) {
            stmt_free(st);
            return NULL;
        }
        skip_nl(p);
        if (peek(p) != TOK_SEMI) {
            st->e2 = parse_expr(p);
            if (!st->e2) {
                stmt_free(st);
                return NULL;
            }
        }
        if (expect(p, TOK_SEMI) != 0) {
            stmt_free(st);
            return NULL;
        }
        skip_nl(p);
        if (peek(p) != TOK_RPAREN) {
            st->e3 = parse_expr(p);
            if (!st->e3) {
                stmt_free(st);
                return NULL;
            }
        }
        if (expect(p, TOK_RPAREN) != 0) {
            stmt_free(st);
            return NULL;
        }
        skip_nl(p);
        st->s1 = parse_stmt(p);
        if (!st->s1) {
            stmt_free(st);
            return NULL;
        }
        return st;
    }
    if (k == TOK_BREAK || k == TOK_CONTINUE || k == TOK_NEXT) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        stmt_kind_t sk = (k == TOK_BREAK)      ? ST_BREAK
                         : (k == TOK_CONTINUE) ? ST_CONTINUE
                                               : ST_NEXT;
        return stmt_new(sk);
    }
    if (k == TOK_EXIT || k == TOK_RETURN) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        stmt_t *st = stmt_new((k == TOK_EXIT) ? ST_EXIT : ST_RETURN);
        if (!st) {
            return NULL;
        }
        if (!is_stmt_end(peek(p))) {
            st->e1 = parse_expr(p);
            if (!st->e1) {
                stmt_free(st);
                return NULL;
            }
        }
        return st;
    }
    if (k == TOK_DELETE) {
        token_t t = lex_next(&p->lx);
        lex_free_tok(&t);
        if (peek(p) != TOK_IDENT) {
            p->err = 1;
            return NULL;
        }
        token_t arr = lex_next(&p->lx);
        stmt_t *st = stmt_new(ST_DELETE);
        if (!st) {
            lex_free_tok(&arr);
            return NULL;
        }
        st->arr_name = arr.text;
        if (peek(p) == TOK_LBRACK) {
            token_t lb = lex_next(&p->lx);
            lex_free_tok(&lb);
            expr_t *ix = expr_new(EX_INDEX);
            if (!ix) {
                stmt_free(st);
                return NULL;
            }
            if (!parse_arg_list_into(p, ix, TOK_RBRACK)) {
                expr_free(ix);
                stmt_free(st);
                return NULL;
            }
            st->e1 = ix;
        }
        return st;
    }
    if (k == TOK_PRINT || k == TOK_PRINTF) {
        return parse_print_stmt(p, (k == TOK_PRINT) ? ST_PRINT : ST_PRINTF);
    }

    stmt_t *st = stmt_new(ST_EXPR);
    if (!st) {
        return NULL;
    }
    st->e1 = parse_expr(p);
    if (!st->e1) {
        stmt_free(st);
        return NULL;
    }
    return st;
}

static int parse_func_def(parser_t *p, prog_t *prog) {
    token_t kw = lex_next(&p->lx);
    lex_free_tok(&kw);
    if (peek(p) != TOK_IDENT) {
        p->err = 1;
        return -1;
    }
    token_t nm = lex_next(&p->lx);
    if (expect(p, TOK_LPAREN) != 0) {
        lex_free_tok(&nm);
        return -1;
    }
    func_t fn = {.name = nm.text};
    skip_nl(p);
    if (peek(p) != TOK_RPAREN) {
        for (;;) {
            if (peek(p) != TOK_IDENT) {
                p->err = 1;
                break;
            }
            token_t prm = lex_next(&p->lx);
            char **np = realloc(fn.params, (fn.nparams + 1) * sizeof(*np));
            if (!np) {
                lex_free_tok(&prm);
                p->err = 1;
                break;
            }
            fn.params = np;
            fn.params[fn.nparams++] = prm.text;
            if (peek(p) != TOK_COMMA) {
                break;
            }
            token_t cm = lex_next(&p->lx);
            lex_free_tok(&cm);
            skip_nl(p);
        }
    }
    if (p->err || expect(p, TOK_RPAREN) != 0) {
        free(fn.name);
        for (size_t i = 0; i < fn.nparams; i++) {
            free(fn.params[i]);
        }
        free(fn.params);
        return -1;
    }
    skip_nl(p);
    fn.body = parse_block(p);
    if (!fn.body) {
        free(fn.name);
        for (size_t i = 0; i < fn.nparams; i++) {
            free(fn.params[i]);
        }
        free(fn.params);
        return -1;
    }
    if (prog->nfuncs == prog->cap_funcs) {
        size_t nc = (prog->cap_funcs == 0) ? 8 : prog->cap_funcs * 2;
        func_t *nf = realloc(prog->funcs, nc * sizeof(*nf));
        if (!nf) {
            free(fn.name);
            for (size_t i = 0; i < fn.nparams; i++) {
                free(fn.params[i]);
            }
            free(fn.params);
            stmt_free(fn.body);
            return -1;
        }
        prog->funcs = nf;
        prog->cap_funcs = nc;
    }
    prog->funcs[prog->nfuncs++] = fn;
    return 0;
}

int parse_program(const char *src, prog_t *prog) {
    (void)memset(prog, 0, sizeof(*prog));
    parser_t p = {0};
    lex_init(&p.lx, src);

    skip_seps(&p);
    while (peek(&p) != TOK_EOF && !p.err && !p.lx.err) {
        if (peek(&p) == TOK_FUNCTION) {
            if (parse_func_def(&p, prog) != 0) {
                break;
            }
            skip_seps(&p);
            continue;
        }
        rule_t r = {0};
        if (peek(&p) == TOK_BEGIN) {
            token_t t = lex_next(&p.lx);
            lex_free_tok(&t);
            r.kind = RULE_BEGIN;
            skip_nl(&p);
            r.action = parse_block(&p);
        } else if (peek(&p) == TOK_END) {
            token_t t = lex_next(&p.lx);
            lex_free_tok(&t);
            r.kind = RULE_END;
            skip_nl(&p);
            r.action = parse_block(&p);
        } else {
            r.kind = RULE_PATTERN;
            if (peek(&p) != TOK_LBRACE) {
                r.pat1 = parse_expr(&p);
                if (!r.pat1) {
                    break;
                }
                if (peek(&p) == TOK_COMMA) {
                    token_t cm = lex_next(&p.lx);
                    lex_free_tok(&cm);
                    skip_nl(&p);
                    r.pat2 = parse_expr(&p);
                    if (!r.pat2) {
                        expr_free(r.pat1);
                        break;
                    }
                }
            }
            if (peek(&p) == TOK_LBRACE) {
                r.action = parse_block(&p);
                if (!r.action) {
                    expr_free(r.pat1);
                    expr_free(r.pat2);
                    break;
                }
            }
        }
        if ((r.kind == RULE_BEGIN || r.kind == RULE_END) && !r.action) {
            p.err = 1;
            break;
        }
        if (prog->nrules == prog->cap_rules) {
            size_t nc = (prog->cap_rules == 0) ? 8 : prog->cap_rules * 2;
            rule_t *nr = realloc(prog->rules, nc * sizeof(*nr));
            if (!nr) {
                expr_free(r.pat1);
                expr_free(r.pat2);
                stmt_free(r.action);
                p.err = 1;
                break;
            }
            prog->rules = nr;
            prog->cap_rules = nc;
        }
        prog->rules[prog->nrules++] = r;
        skip_seps(&p);
    }

    lex_free_tok(&p.lx.cur);
    if (p.err || p.lx.err) {
        (void)fprintf(stderr, "awk: syntax error at line %d\n", p.lx.line);
        prog_free(prog);
        return -1;
    }
    return 0;
}
