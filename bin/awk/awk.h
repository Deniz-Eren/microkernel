/**
 * @file     awk.h
 * @brief    Internal types and interfaces for Ring 3 POSIX.1-2024 XCU awk
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef AWK_H
#define AWK_H

#include <regex.h>
#include <stddef.h>
#include <stdio.h>

/* need:: COMP_LIBC_0024 */

typedef enum {
    TOK_EOF = 0,
    TOK_NL,
    TOK_SEMI,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACK,
    TOK_RBRACK,
    TOK_COMMA,
    TOK_QUEST,
    TOK_COLON,
    TOK_DOLLAR,
    TOK_NUM,
    TOK_STR,
    TOK_REGEX,
    TOK_IDENT,
    /* Keywords */
    TOK_BEGIN,
    TOK_END,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_DO,
    TOK_FOR,
    TOK_IN,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_NEXT,
    TOK_EXIT,
    TOK_RETURN,
    TOK_DELETE,
    TOK_PRINT,
    TOK_PRINTF,
    TOK_GETLINE,
    TOK_FUNCTION,
    /* Operators */
    TOK_ASSIGN,
    TOK_ADD_ASSIGN,
    TOK_SUB_ASSIGN,
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_POW_ASSIGN,
    TOK_OR,
    TOK_AND,
    TOK_MATCH,
    TOK_NOMATCH,
    TOK_LT,
    TOK_LE,
    TOK_EQ,
    TOK_NE,
    TOK_GT,
    TOK_GE,
    TOK_APPEND,
    TOK_PIPE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_CARET,
    TOK_NOT,
    TOK_INC,
    TOK_DEC
} tok_kind_t;

typedef struct {
    tok_kind_t kind;
    char *text;
    double num;
    int line;
} token_t;

typedef struct {
    const char *src;
    size_t pos;
    int line;
    tok_kind_t prev_kind;
    token_t cur;
    int has_cur;
    int err;
} lexer_t;

typedef enum {
    EX_NUM = 0,
    EX_STR,
    EX_REGEX,
    EX_VAR,
    EX_INDEX,
    EX_FIELD,
    EX_UNARY,
    EX_BINARY,
    EX_CONCAT,
    EX_ASSIGN,
    EX_INCDEC,
    EX_TERNARY,
    EX_IN,
    EX_CALL,
    EX_GETLINE
} expr_kind_t;

typedef struct expr expr_t;

struct expr {
    expr_kind_t kind;
    int op;
    int prefix;
    double num;
    char *str;
    expr_t *a;
    expr_t *b;
    expr_t *c;
    expr_t **args;
    size_t nargs;
};

typedef enum {
    ST_EMPTY = 0,
    ST_BLOCK,
    ST_EXPR,
    ST_IF,
    ST_WHILE,
    ST_DO,
    ST_FOR,
    ST_FOR_IN,
    ST_BREAK,
    ST_CONTINUE,
    ST_NEXT,
    ST_EXIT,
    ST_RETURN,
    ST_DELETE,
    ST_PRINT,
    ST_PRINTF
} stmt_kind_t;

typedef struct stmt stmt_t;

struct stmt {
    stmt_kind_t kind;
    int redir_op;
    char *var_name;
    char *arr_name;
    expr_t *e1;
    expr_t *e2;
    expr_t *e3;
    expr_t *redir_expr;
    expr_t **args;
    size_t nargs;
    stmt_t *s1;
    stmt_t *s2;
    stmt_t **list;
    size_t nlist;
};

typedef enum { RULE_BEGIN = 0, RULE_END, RULE_PATTERN } rule_kind_t;

typedef struct {
    rule_kind_t kind;
    expr_t *pat1;
    expr_t *pat2;
    int in_range;
    stmt_t *action;
} rule_t;

typedef struct {
    char *name;
    char **params;
    size_t nparams;
    stmt_t *body;
} func_t;

typedef struct {
    rule_t *rules;
    size_t nrules;
    size_t cap_rules;
    func_t *funcs;
    size_t nfuncs;
    size_t cap_funcs;
} prog_t;

typedef enum { VAL_UNINIT = 0, VAL_NUM, VAL_STR, VAL_NUMSTR } val_kind_t;

typedef struct {
    val_kind_t kind;
    double num;
    char *str;
} awk_val_t;

typedef struct {
    char *key;
    awk_val_t val;
} arr_entry_t;

typedef struct {
    arr_entry_t *items;
    size_t len;
    size_t cap;
} awk_arr_t;

typedef struct {
    char *name;
    awk_val_t val;
    awk_arr_t arr;
    int is_arr;
} awk_var_t;

typedef struct {
    char *name;
    FILE *fp;
    int is_pipe;
    int is_write;
} io_entry_t;

typedef enum {
    FLOW_NONE = 0,
    FLOW_BREAK,
    FLOW_CONTINUE,
    FLOW_NEXT,
    FLOW_RETURN,
    FLOW_EXIT
} flow_t;

typedef struct frame frame_t;

struct frame {
    char **names;
    awk_var_t *locals;
    awk_var_t **arr_refs;
    size_t nlocals;
    frame_t *parent;
};

typedef struct {
    prog_t *prog;
    awk_var_t *vars;
    size_t nvars;
    size_t cap_vars;
    char *record;
    char **fields;
    size_t nf;
    size_t cap_fields;
    io_entry_t *ios;
    size_t nios;
    size_t cap_ios;
    flow_t flow;
    awk_val_t ret_val;
    int exit_code;
    int exiting;
    unsigned int rng_state;
    FILE *cur_in;
    frame_t *frame;
} awk_vm_t;

/* Lexer (lex.c) */
void lex_init(lexer_t *lx, const char *src);
void lex_free_tok(token_t *t);
token_t *lex_peek(lexer_t *lx);
token_t lex_next(lexer_t *lx);

/* Parser (parse.c) */
int parse_program(const char *src, prog_t *prog);
void prog_free(prog_t *prog);

/* Evaluator / VM (eval.c) */
void vm_init(awk_vm_t *vm, prog_t *prog);
void vm_free(awk_vm_t *vm);
awk_val_t val_num(double n);
awk_val_t val_str(const char *s);
awk_val_t val_numstr(const char *s);
awk_val_t val_copy(const awk_val_t *v);
void val_free(awk_val_t *v);
double val_to_num(const awk_val_t *v);
char *val_to_str(awk_vm_t *vm, const awk_val_t *v);
awk_var_t *vm_get_var(awk_vm_t *vm, const char *name);
void vm_set_str(awk_vm_t *vm, const char *name, const char *s);
void vm_set_num(awk_vm_t *vm, const char *name, double n);
void vm_arr_set(awk_var_t *var, const char *key, awk_val_t v);
void vm_set_record(awk_vm_t *vm, const char *rec);
int vm_read_record(awk_vm_t *vm, FILE *fp, char **out);
void vm_run_begin(awk_vm_t *vm);
void vm_run_record(awk_vm_t *vm, const char *rec);
void vm_run_end(awk_vm_t *vm);
char *awk_unescape(const char *s);

#endif /* AWK_H */
