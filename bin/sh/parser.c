/**
 * @file     parser.c
 * @brief    POSIX.1-2024 recursive-descent shell grammar parser and AST
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sh.h"

/* need:: COMP_LIBC_0017 */

struct parser {
    const sh_tokvec_t *tv;
    size_t pos;
    int err;
};

static const sh_tok_t *peek(const struct parser *p) {
    if (p->pos < p->tv->len) {
        return &p->tv->items[p->pos];
    }
    return &p->tv->items[p->tv->len - 1u];
}

static const sh_tok_t *peek_at(const struct parser *p, size_t off) {
    if (p->pos + off < p->tv->len) {
        return &p->tv->items[p->pos + off];
    }
    return &p->tv->items[p->tv->len - 1u];
}

static const sh_tok_t *advance(struct parser *p) {
    const sh_tok_t *t = peek(p);
    if (t->kind != T_EOF) {
        p->pos++;
    }
    return t;
}

static int is_word(const sh_tok_t *t, const char *kw) {
    return t->kind == T_WORD && t->text && strcmp(t->text, kw) == 0;
}

static int is_redir_kind(sh_tok_kind_t k) {
    return k == T_REDIR_IN || k == T_REDIR_OUT || k == T_REDIR_APP ||
           k == T_REDIR_CLOBBER || k == T_REDIR_RDWR || k == T_DUP_IN ||
           k == T_DUP_OUT || k == T_HEREDOC || k == T_HEREDOC_STRIP;
}

static void skip_newlines(struct parser *p) {
    while (peek(p)->kind == T_NL) {
        (void)advance(p);
    }
}

static sh_node_t *new_node(sh_node_kind_t kind) {
    sh_node_t *n = (sh_node_t *)calloc(1u, sizeof(*n));
    if (n) {
        n->kind = kind;
    }
    return n;
}

static int add_redir(sh_node_t *n, sh_redir_t r) {
    sh_redir_t *nr = (sh_redir_t *)reallocarray(n->redirs, n->nredirs + 1u,
                                                sizeof(sh_redir_t));
    if (!nr) {
        free(r.target);
        return -1;
    }
    n->redirs = nr;
    n->redirs[n->nredirs++] = r;
    return 0;
}

static int parse_redir(struct parser *p, sh_node_t *n) {
    const sh_tok_t *op = advance(p);
    const sh_tok_t *arg = peek(p);
    if (arg->kind != T_WORD) {
        fprintf(stderr, "sh: syntax error: missing redirection target\n");
        p->err = 1;
        return -1;
    }
    (void)advance(p);
    sh_redir_t r = {0};
    r.kind = op->kind;
    if (op->io_fd >= 0) {
        r.fd = op->io_fd;
    } else if (op->kind == T_REDIR_IN || op->kind == T_DUP_IN ||
               op->kind == T_REDIR_RDWR || op->kind == T_HEREDOC ||
               op->kind == T_HEREDOC_STRIP) {
        r.fd = 0;
    } else {
        r.fd = 1;
    }
    if (op->kind == T_HEREDOC || op->kind == T_HEREDOC_STRIP) {
        r.target = strdup(op->heredoc ? op->heredoc : "");
        r.quoted = op->heredoc_quoted;
    } else {
        r.target = strdup(arg->text ? arg->text : "");
    }
    if (!r.target) {
        p->err = 1;
        return -1;
    }
    return add_redir(n, r);
}

static void parse_trailing_redirs(struct parser *p, sh_node_t *n) {
    while (!p->err && is_redir_kind(peek(p)->kind)) {
        (void)parse_redir(p, n);
    }
}

static sh_node_t *parse_list_until(struct parser *p,
                                   const char *const *stop_kws, int stop_rparen,
                                   int stop_dsemi);
static sh_node_t *parse_command(struct parser *p);

static int is_stop_word(const sh_tok_t *t, const char *const *stop_kws) {
    if (!stop_kws || t->kind != T_WORD || !t->text) {
        return 0;
    }
    for (size_t i = 0u; stop_kws[i] != NULL; i++) {
        if (strcmp(t->text, stop_kws[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static sh_node_t *parse_if(struct parser *p) {
    (void)advance(p); /* 'if' */
    sh_node_t *n = new_node(N_IF);
    if (!n) {
        p->err = 1;
        return NULL;
    }
    static const char *const k_then[] = {"then", NULL};
    static const char *const k_body_end[] = {"elif", "else", "fi", NULL};
    static const char *const k_fi[] = {"fi", NULL};

    for (;;) {
        sh_node_t *cond = parse_list_until(p, k_then, 0, 0);
        if (p->err || !is_word(peek(p), "then")) {
            fprintf(stderr, "sh: syntax error: expected 'then'\n");
            p->err = 1;
            sh_node_free(cond);
            sh_node_free(n);
            return NULL;
        }
        (void)advance(p);
        sh_node_t *body = parse_list_until(p, k_body_end, 0, 0);
        if (p->err) {
            sh_node_free(cond);
            sh_node_free(body);
            sh_node_free(n);
            return NULL;
        }
        size_t nb = n->u.if_cmd.nbranches;
        sh_node_t **nc = (sh_node_t **)reallocarray(n->u.if_cmd.conds, nb + 1u,
                                                    sizeof(sh_node_t *));
        sh_node_t **ny = (sh_node_t **)reallocarray(n->u.if_cmd.bodies, nb + 1u,
                                                    sizeof(sh_node_t *));
        if (!nc || !ny) {
            free(nc);
            free(ny);
            sh_node_free(cond);
            sh_node_free(body);
            sh_node_free(n);
            p->err = 1;
            return NULL;
        }
        n->u.if_cmd.conds = nc;
        n->u.if_cmd.bodies = ny;
        n->u.if_cmd.conds[nb] = cond;
        n->u.if_cmd.bodies[nb] = body;
        n->u.if_cmd.nbranches = nb + 1u;

        if (is_word(peek(p), "elif")) {
            (void)advance(p);
            continue;
        }
        break;
    }
    if (is_word(peek(p), "else")) {
        (void)advance(p);
        n->u.if_cmd.else_body = parse_list_until(p, k_fi, 0, 0);
    }
    if (p->err || !is_word(peek(p), "fi")) {
        fprintf(stderr, "sh: syntax error: expected 'fi'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    parse_trailing_redirs(p, n);
    return n;
}

static sh_node_t *parse_for(struct parser *p) {
    (void)advance(p); /* 'for' */
    if (peek(p)->kind != T_WORD) {
        fprintf(stderr, "sh: syntax error: expected variable name after for\n");
        p->err = 1;
        return NULL;
    }
    sh_node_t *n = new_node(N_FOR);
    if (!n) {
        p->err = 1;
        return NULL;
    }
    n->u.for_cmd.var_name = strdup(advance(p)->text);
    skip_newlines(p);
    if (is_word(peek(p), "in")) {
        (void)advance(p);
        n->u.for_cmd.has_in = 1;
        while (peek(p)->kind == T_WORD) {
            if (sh_vec_push(&n->u.for_cmd.words, strdup(advance(p)->text)) !=
                0) {
                p->err = 1;
                sh_node_free(n);
                return NULL;
            }
        }
        if (peek(p)->kind == T_SEMI || peek(p)->kind == T_NL) {
            (void)advance(p);
        }
    } else if (peek(p)->kind == T_SEMI) {
        (void)advance(p);
    }
    skip_newlines(p);
    if (!is_word(peek(p), "do")) {
        fprintf(stderr, "sh: syntax error: expected 'do'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    static const char *const k_done[] = {"done", NULL};
    n->u.for_cmd.body = parse_list_until(p, k_done, 0, 0);
    if (p->err || !is_word(peek(p), "done")) {
        fprintf(stderr, "sh: syntax error: expected 'done'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    parse_trailing_redirs(p, n);
    return n;
}

static sh_node_t *parse_loop(struct parser *p, sh_node_kind_t kind) {
    (void)advance(p); /* 'while' or 'until' */
    sh_node_t *n = new_node(kind);
    if (!n) {
        p->err = 1;
        return NULL;
    }
    static const char *const k_do[] = {"do", NULL};
    static const char *const k_done[] = {"done", NULL};
    n->u.loop.cond = parse_list_until(p, k_do, 0, 0);
    if (p->err || !is_word(peek(p), "do")) {
        fprintf(stderr, "sh: syntax error: expected 'do'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    n->u.loop.body = parse_list_until(p, k_done, 0, 0);
    if (p->err || !is_word(peek(p), "done")) {
        fprintf(stderr, "sh: syntax error: expected 'done'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    parse_trailing_redirs(p, n);
    return n;
}

static sh_node_t *parse_case(struct parser *p) {
    (void)advance(p); /* 'case' */
    if (peek(p)->kind != T_WORD) {
        fprintf(stderr, "sh: syntax error: expected word after case\n");
        p->err = 1;
        return NULL;
    }
    sh_node_t *n = new_node(N_CASE);
    if (!n) {
        p->err = 1;
        return NULL;
    }
    n->u.case_cmd.word = strdup(advance(p)->text);
    skip_newlines(p);
    if (!is_word(peek(p), "in")) {
        fprintf(stderr, "sh: syntax error: expected 'in' after case word\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    skip_newlines(p);
    static const char *const k_esac[] = {"esac", NULL};

    while (!p->err && !is_word(peek(p), "esac") && peek(p)->kind != T_EOF) {
        if (peek(p)->kind == T_LPAREN) {
            (void)advance(p);
        }
        sh_case_item_t item = {0};
        for (;;) {
            if (peek(p)->kind != T_WORD) {
                fprintf(stderr, "sh: syntax error in case pattern\n");
                p->err = 1;
                sh_vec_free(&item.patterns);
                sh_node_free(n);
                return NULL;
            }
            (void)sh_vec_push(&item.patterns, strdup(advance(p)->text));
            if (peek(p)->kind == T_PIPE) {
                (void)advance(p);
                continue;
            }
            break;
        }
        if (peek(p)->kind != T_RPAREN) {
            fprintf(stderr, "sh: syntax error: expected ')' in case\n");
            p->err = 1;
            sh_vec_free(&item.patterns);
            sh_node_free(n);
            return NULL;
        }
        (void)advance(p);
        skip_newlines(p);
        item.body = parse_list_until(p, k_esac, 0, 1);
        if (peek(p)->kind == T_DSEMI) {
            (void)advance(p);
        }
        skip_newlines(p);
        sh_case_item_t *ni = (sh_case_item_t *)reallocarray(
            n->u.case_cmd.items, n->u.case_cmd.nitems + 1u,
            sizeof(sh_case_item_t));
        if (!ni) {
            sh_vec_free(&item.patterns);
            sh_node_free(item.body);
            sh_node_free(n);
            p->err = 1;
            return NULL;
        }
        n->u.case_cmd.items = ni;
        n->u.case_cmd.items[n->u.case_cmd.nitems++] = item;
    }
    if (p->err || !is_word(peek(p), "esac")) {
        fprintf(stderr, "sh: syntax error: expected 'esac'\n");
        p->err = 1;
        sh_node_free(n);
        return NULL;
    }
    (void)advance(p);
    parse_trailing_redirs(p, n);
    return n;
}

static sh_node_t *parse_command(struct parser *p) {
    const sh_tok_t *t = peek(p);
    if (t->kind == T_LPAREN) {
        (void)advance(p);
        sh_node_t *n = new_node(N_SUBSHELL);
        if (!n) {
            p->err = 1;
            return NULL;
        }
        n->u.group.body = parse_list_until(p, NULL, 1, 0);
        if (p->err || peek(p)->kind != T_RPAREN) {
            fprintf(stderr, "sh: syntax error: expected ')'\n");
            p->err = 1;
            sh_node_free(n);
            return NULL;
        }
        (void)advance(p);
        parse_trailing_redirs(p, n);
        return n;
    }
    if (is_word(t, "{")) {
        (void)advance(p);
        sh_node_t *n = new_node(N_BRACE);
        if (!n) {
            p->err = 1;
            return NULL;
        }
        static const char *const k_rbrace[] = {"}", NULL};
        n->u.group.body = parse_list_until(p, k_rbrace, 0, 0);
        if (p->err || !is_word(peek(p), "}")) {
            fprintf(stderr, "sh: syntax error: expected '}'\n");
            p->err = 1;
            sh_node_free(n);
            return NULL;
        }
        (void)advance(p);
        parse_trailing_redirs(p, n);
        return n;
    }
    if (is_word(t, "if")) {
        return parse_if(p);
    }
    if (is_word(t, "for")) {
        return parse_for(p);
    }
    if (is_word(t, "while")) {
        return parse_loop(p, N_WHILE);
    }
    if (is_word(t, "until")) {
        return parse_loop(p, N_UNTIL);
    }
    if (is_word(t, "case")) {
        return parse_case(p);
    }

    /* Function definition: name ( ) compound_command */
    if (t->kind == T_WORD && peek_at(p, 1u)->kind == T_LPAREN &&
        peek_at(p, 2u)->kind == T_RPAREN) {
        char *fname = strdup(advance(p)->text);
        (void)advance(p); /* '(' */
        (void)advance(p); /* ')' */
        skip_newlines(p);
        sh_node_t *body = parse_command(p);
        if (p->err || !body) {
            free(fname);
            sh_node_free(body);
            return NULL;
        }
        sh_node_t *fn = new_node(N_FUNCDEF);
        if (!fn) {
            free(fname);
            sh_node_free(body);
            p->err = 1;
            return NULL;
        }
        fn->u.funcdef.name = fname;
        fn->u.funcdef.body = body;
        return fn;
    }

    /* Simple command: assignments, words, and interleaved redirections. */
    sh_node_t *n = new_node(N_SIMPLE);
    if (!n) {
        p->err = 1;
        return NULL;
    }
    int seen_word = 0;
    while (!p->err) {
        const sh_tok_t *cur = peek(p);
        if (is_redir_kind(cur->kind)) {
            (void)parse_redir(p, n);
        } else if (cur->kind == T_WORD) {
            (void)advance(p);
            if (!seen_word && sh_is_assignment(cur->text)) {
                (void)sh_vec_push(&n->u.simple.assigns, strdup(cur->text));
            } else {
                seen_word = 1;
                (void)sh_vec_push(&n->u.simple.words, strdup(cur->text));
            }
        } else {
            break;
        }
    }
    if (n->u.simple.assigns.len == 0u && n->u.simple.words.len == 0u &&
        n->nredirs == 0u) {
        sh_node_free(n);
        return NULL;
    }
    return n;
}

static sh_node_t *parse_pipeline(struct parser *p) {
    int negated = 0;
    while (is_word(peek(p), "!")) {
        negated = !negated;
        (void)advance(p);
    }
    sh_node_t *first = parse_command(p);
    if (p->err || !first) {
        return first;
    }
    if (!negated && peek(p)->kind != T_PIPE) {
        return first;
    }
    sh_node_t *pl = new_node(N_PIPELINE);
    if (!pl) {
        sh_node_free(first);
        p->err = 1;
        return NULL;
    }
    pl->u.pipe.negated = negated;
    pl->u.pipe.stages = (sh_node_t **)malloc(sizeof(sh_node_t *));
    if (!pl->u.pipe.stages) {
        sh_node_free(first);
        sh_node_free(pl);
        p->err = 1;
        return NULL;
    }
    pl->u.pipe.stages[0] = first;
    pl->u.pipe.nstages = 1u;

    while (peek(p)->kind == T_PIPE) {
        (void)advance(p);
        skip_newlines(p);
        sh_node_t *st = parse_command(p);
        if (p->err || !st) {
            fprintf(stderr, "sh: syntax error after '|'\n");
            p->err = 1;
            sh_node_free(st);
            sh_node_free(pl);
            return NULL;
        }
        sh_node_t **ns = (sh_node_t **)reallocarray(
            pl->u.pipe.stages, pl->u.pipe.nstages + 1u, sizeof(sh_node_t *));
        if (!ns) {
            sh_node_free(st);
            sh_node_free(pl);
            p->err = 1;
            return NULL;
        }
        pl->u.pipe.stages = ns;
        pl->u.pipe.stages[pl->u.pipe.nstages++] = st;
    }
    return pl;
}

static sh_node_t *parse_and_or(struct parser *p) {
    sh_node_t *left = parse_pipeline(p);
    if (p->err || !left) {
        return left;
    }
    while (peek(p)->kind == T_AND || peek(p)->kind == T_OR) {
        int is_or = (advance(p)->kind == T_OR);
        skip_newlines(p);
        sh_node_t *right = parse_pipeline(p);
        if (p->err || !right) {
            fprintf(stderr, "sh: syntax error after %s\n", is_or ? "||" : "&&");
            p->err = 1;
            sh_node_free(left);
            sh_node_free(right);
            return NULL;
        }
        sh_node_t *ao = new_node(N_AND_OR);
        if (!ao) {
            sh_node_free(left);
            sh_node_free(right);
            p->err = 1;
            return NULL;
        }
        ao->u.and_or.left = left;
        ao->u.and_or.right = right;
        ao->u.and_or.is_or = is_or;
        left = ao;
    }
    return left;
}

static sh_node_t *parse_list_until(struct parser *p,
                                   const char *const *stop_kws, int stop_rparen,
                                   int stop_dsemi) {
    sh_node_t *list = new_node(N_LIST);
    if (!list) {
        p->err = 1;
        return NULL;
    }
    for (;;) {
        while (peek(p)->kind == T_NL || peek(p)->kind == T_SEMI) {
            (void)advance(p);
        }
        const sh_tok_t *t = peek(p);
        if (t->kind == T_EOF || (stop_rparen && t->kind == T_RPAREN) ||
            (stop_dsemi && t->kind == T_DSEMI) || is_stop_word(t, stop_kws)) {
            break;
        }
        sh_node_t *item = parse_and_or(p);
        if (p->err) {
            sh_node_free(item);
            sh_node_free(list);
            return NULL;
        }
        if (!item) {
            fprintf(stderr, "sh: unexpected token\n");
            p->err = 1;
            sh_node_free(list);
            return NULL;
        }
        int bg = 0;
        if (peek(p)->kind == T_AMP) {
            bg = 1;
            (void)advance(p);
        } else if (peek(p)->kind == T_SEMI || peek(p)->kind == T_NL) {
            (void)advance(p);
        }
        size_t ni = list->u.list.nitems;
        sh_node_t **arr = (sh_node_t **)reallocarray(
            list->u.list.items, ni + 1u, sizeof(sh_node_t *));
        int *bgs = (int *)reallocarray(list->u.list.bg, ni + 1u, sizeof(int));
        if (!arr || !bgs) {
            free(arr);
            free(bgs);
            sh_node_free(item);
            sh_node_free(list);
            p->err = 1;
            return NULL;
        }
        list->u.list.items = arr;
        list->u.list.bg = bgs;
        list->u.list.items[ni] = item;
        list->u.list.bg[ni] = bg;
        list->u.list.nitems = ni + 1u;
    }
    return list;
}

sh_node_t *sh_parse(const sh_tokvec_t *tv, int *err_out) {
    struct parser p = {.tv = tv, .pos = 0u, .err = 0};
    sh_node_t *root = parse_list_until(&p, NULL, 0, 0);
    if (!p.err && peek(&p)->kind != T_EOF) {
        fprintf(stderr, "sh: syntax error: unexpected trailing token\n");
        p.err = 1;
        sh_node_free(root);
        root = NULL;
    }
    if (err_out) {
        *err_out = p.err;
    }
    return root;
}

static sh_vec_t clone_vec(const sh_vec_t *src) {
    sh_vec_t dst = {0};
    for (size_t i = 0u; i < src->len; i++) {
        (void)sh_vec_push(&dst, strdup(src->items[i]));
    }
    return dst;
}

sh_node_t *sh_node_clone(const sh_node_t *n) {
    if (!n) {
        return NULL;
    }
    sh_node_t *c = new_node(n->kind);
    if (!c) {
        return NULL;
    }
    if (n->nredirs > 0u) {
        c->redirs = (sh_redir_t *)calloc(n->nredirs, sizeof(sh_redir_t));
        c->nredirs = n->nredirs;
        for (size_t i = 0u; i < n->nredirs; i++) {
            c->redirs[i] = n->redirs[i];
            c->redirs[i].target =
                n->redirs[i].target ? strdup(n->redirs[i].target) : NULL;
        }
    }
    switch (n->kind) {
    case N_SIMPLE:
        c->u.simple.assigns = clone_vec(&n->u.simple.assigns);
        c->u.simple.words = clone_vec(&n->u.simple.words);
        break;
    case N_PIPELINE:
        c->u.pipe.negated = n->u.pipe.negated;
        c->u.pipe.nstages = n->u.pipe.nstages;
        c->u.pipe.stages =
            (sh_node_t **)calloc(n->u.pipe.nstages, sizeof(sh_node_t *));
        for (size_t i = 0u; i < n->u.pipe.nstages; i++) {
            c->u.pipe.stages[i] = sh_node_clone(n->u.pipe.stages[i]);
        }
        break;
    case N_AND_OR:
        c->u.and_or.left = sh_node_clone(n->u.and_or.left);
        c->u.and_or.right = sh_node_clone(n->u.and_or.right);
        c->u.and_or.is_or = n->u.and_or.is_or;
        break;
    case N_LIST:
        c->u.list.nitems = n->u.list.nitems;
        if (n->u.list.nitems > 0u) {
            c->u.list.items =
                (sh_node_t **)calloc(n->u.list.nitems, sizeof(sh_node_t *));
            c->u.list.bg = (int *)calloc(n->u.list.nitems, sizeof(int));
            for (size_t i = 0u; i < n->u.list.nitems; i++) {
                c->u.list.items[i] = sh_node_clone(n->u.list.items[i]);
                c->u.list.bg[i] = n->u.list.bg[i];
            }
        }
        break;
    case N_SUBSHELL:
    case N_BRACE:
        c->u.group.body = sh_node_clone(n->u.group.body);
        break;
    case N_IF:
        c->u.if_cmd.nbranches = n->u.if_cmd.nbranches;
        c->u.if_cmd.conds =
            (sh_node_t **)calloc(n->u.if_cmd.nbranches, sizeof(sh_node_t *));
        c->u.if_cmd.bodies =
            (sh_node_t **)calloc(n->u.if_cmd.nbranches, sizeof(sh_node_t *));
        for (size_t i = 0u; i < n->u.if_cmd.nbranches; i++) {
            c->u.if_cmd.conds[i] = sh_node_clone(n->u.if_cmd.conds[i]);
            c->u.if_cmd.bodies[i] = sh_node_clone(n->u.if_cmd.bodies[i]);
        }
        c->u.if_cmd.else_body = sh_node_clone(n->u.if_cmd.else_body);
        break;
    case N_FOR:
        c->u.for_cmd.var_name = strdup(n->u.for_cmd.var_name);
        c->u.for_cmd.words = clone_vec(&n->u.for_cmd.words);
        c->u.for_cmd.has_in = n->u.for_cmd.has_in;
        c->u.for_cmd.body = sh_node_clone(n->u.for_cmd.body);
        break;
    case N_WHILE:
    case N_UNTIL:
        c->u.loop.cond = sh_node_clone(n->u.loop.cond);
        c->u.loop.body = sh_node_clone(n->u.loop.body);
        break;
    case N_CASE:
        c->u.case_cmd.word = strdup(n->u.case_cmd.word);
        c->u.case_cmd.nitems = n->u.case_cmd.nitems;
        if (n->u.case_cmd.nitems > 0u) {
            c->u.case_cmd.items = (sh_case_item_t *)calloc(
                n->u.case_cmd.nitems, sizeof(sh_case_item_t));
            for (size_t i = 0u; i < n->u.case_cmd.nitems; i++) {
                c->u.case_cmd.items[i].patterns =
                    clone_vec(&n->u.case_cmd.items[i].patterns);
                c->u.case_cmd.items[i].body =
                    sh_node_clone(n->u.case_cmd.items[i].body);
            }
        }
        break;
    case N_FUNCDEF:
        c->u.funcdef.name = strdup(n->u.funcdef.name);
        c->u.funcdef.body = sh_node_clone(n->u.funcdef.body);
        break;
    }
    return c;
}

void sh_node_free(sh_node_t *n) {
    if (!n) {
        return;
    }
    for (size_t i = 0u; i < n->nredirs; i++) {
        free(n->redirs[i].target);
    }
    free(n->redirs);
    switch (n->kind) {
    case N_SIMPLE:
        sh_vec_free(&n->u.simple.assigns);
        sh_vec_free(&n->u.simple.words);
        break;
    case N_PIPELINE:
        for (size_t i = 0u; i < n->u.pipe.nstages; i++) {
            sh_node_free(n->u.pipe.stages[i]);
        }
        free(n->u.pipe.stages);
        break;
    case N_AND_OR:
        sh_node_free(n->u.and_or.left);
        sh_node_free(n->u.and_or.right);
        break;
    case N_LIST:
        for (size_t i = 0u; i < n->u.list.nitems; i++) {
            sh_node_free(n->u.list.items[i]);
        }
        free(n->u.list.items);
        free(n->u.list.bg);
        break;
    case N_SUBSHELL:
    case N_BRACE:
        sh_node_free(n->u.group.body);
        break;
    case N_IF:
        for (size_t i = 0u; i < n->u.if_cmd.nbranches; i++) {
            sh_node_free(n->u.if_cmd.conds[i]);
            sh_node_free(n->u.if_cmd.bodies[i]);
        }
        free(n->u.if_cmd.conds);
        free(n->u.if_cmd.bodies);
        sh_node_free(n->u.if_cmd.else_body);
        break;
    case N_FOR:
        free(n->u.for_cmd.var_name);
        sh_vec_free(&n->u.for_cmd.words);
        sh_node_free(n->u.for_cmd.body);
        break;
    case N_WHILE:
    case N_UNTIL:
        sh_node_free(n->u.loop.cond);
        sh_node_free(n->u.loop.body);
        break;
    case N_CASE:
        free(n->u.case_cmd.word);
        for (size_t i = 0u; i < n->u.case_cmd.nitems; i++) {
            sh_vec_free(&n->u.case_cmd.items[i].patterns);
            sh_node_free(n->u.case_cmd.items[i].body);
        }
        free(n->u.case_cmd.items);
        break;
    case N_FUNCDEF:
        free(n->u.funcdef.name);
        sh_node_free(n->u.funcdef.body);
        break;
    }
    free(n);
}
