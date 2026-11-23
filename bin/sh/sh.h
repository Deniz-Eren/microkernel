/**
 * @file     sh.h
 * @brief    Internal data structures and module prototypes for Ring 3 sh
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SH_INTERNAL_H
#define _SH_INTERNAL_H

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define SH_MAX_REDIRS 16
#define SH_MAX_STAGES 16
#define SH_MAX_TRAPS 32
#define SH_QUOTED_MARK '\x01'
#define SH_AT_SPLIT '\x02'

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sh_buf_t;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} sh_vec_t;

typedef enum {
    T_EOF = 0,
    T_WORD,
    T_NL,
    T_SEMI,
    T_DSEMI,
    T_AMP,
    T_PIPE,
    T_AND,
    T_OR,
    T_LPAREN,
    T_RPAREN,
    T_REDIR_IN,
    T_REDIR_OUT,
    T_REDIR_APP,
    T_REDIR_CLOBBER,
    T_REDIR_RDWR,
    T_DUP_IN,
    T_DUP_OUT,
    T_HEREDOC,
    T_HEREDOC_STRIP,
} sh_tok_kind_t;

typedef struct {
    sh_tok_kind_t kind;
    int io_fd; /* -1 if unspecified */
    char *text;
    char *heredoc; /* body for T_HEREDOC / T_HEREDOC_STRIP */
    int heredoc_quoted;
} sh_tok_t;

typedef struct {
    sh_tok_t *items;
    size_t len;
    size_t cap;
} sh_tokvec_t;

typedef struct {
    sh_tok_kind_t kind;
    int fd;
    char *target; /* raw word or heredoc body */
    int quoted;   /* for heredoc expansion */
} sh_redir_t;

typedef enum {
    N_SIMPLE = 0,
    N_PIPELINE,
    N_AND_OR,
    N_LIST,
    N_SUBSHELL,
    N_BRACE,
    N_IF,
    N_FOR,
    N_WHILE,
    N_UNTIL,
    N_CASE,
    N_FUNCDEF,
} sh_node_kind_t;

typedef struct sh_node sh_node_t;

typedef struct {
    sh_vec_t patterns;
    sh_node_t *body;
} sh_case_item_t;

struct sh_node {
    sh_node_kind_t kind;
    sh_redir_t *redirs;
    size_t nredirs;
    union {
        struct {
            sh_vec_t assigns;
            sh_vec_t words;
        } simple;
        struct {
            sh_node_t **stages;
            size_t nstages;
            int negated;
        } pipe;
        struct {
            sh_node_t *left;
            sh_node_t *right;
            int is_or; /* 0 = &&, 1 = || */
        } and_or;
        struct {
            sh_node_t **items;
            int *bg;
            size_t nitems;
        } list;
        struct {
            sh_node_t *body;
        } group;
        struct {
            sh_node_t **conds;
            sh_node_t **bodies;
            size_t nbranches;
            sh_node_t *else_body;
        } if_cmd;
        struct {
            char *var_name;
            sh_vec_t words;
            int has_in;
            sh_node_t *body;
        } for_cmd;
        struct {
            sh_node_t *cond;
            sh_node_t *body;
        } loop;
        struct {
            char *word;
            sh_case_item_t *items;
            size_t nitems;
        } case_cmd;
        struct {
            char *name;
            sh_node_t *body;
        } funcdef;
    } u;
};

typedef struct sh_var {
    char *name;
    char *val;
    int exported;
    int readonly;
    struct sh_var *next;
} sh_var_t;

typedef struct sh_alias {
    char *name;
    char *val;
    struct sh_alias *next;
} sh_alias_t;

typedef struct sh_func {
    char *name;
    sh_node_t *body;
    struct sh_func *next;
} sh_func_t;

typedef struct sh_hash {
    char *name;
    char *path;
    unsigned hits;
    struct sh_hash *next;
} sh_hash_t;

typedef struct {
    sh_var_t *vars;
    sh_alias_t *aliases;
    sh_func_t *funcs;
    sh_hash_t *hashes;
    char *traps[SH_MAX_TRAPS]; /* index 0 = EXIT, others = signo */
    char *arg0;
    char **pos_args;
    int pos_argc;
    int last_status;
    pid_t last_bg_pid;
    int opt_pipefail;
    int opt_errexit;
    int opt_nounset;
    int opt_noglob;
    int opt_allexport;
    int interactive;
    pid_t shell_pgid;
    int should_exit;
    int exit_code;
    int in_exit_trap;
    int loop_depth;
    int break_count;
    int continue_count;
    int func_depth;
    int dot_depth;
    int returning;
    int return_code;
    int getopts_charoff;
} sh_state_t;

extern sh_state_t g_sh;
extern volatile sig_atomic_t g_sh_sig_pending[SH_MAX_TRAPS];

/* Buffer and vector helpers (lexer.c) */
int sh_buf_push(sh_buf_t *b, char c);
int sh_buf_puts(sh_buf_t *b, const char *s);
int sh_buf_putn(sh_buf_t *b, const char *s, size_t n);
void sh_buf_free(sh_buf_t *b);
int sh_vec_push(sh_vec_t *v, char *s);
void sh_vec_free(sh_vec_t *v);

/* Variable, alias, hash, function, trap helpers (builtins.c) */
const char *sh_var_get(const char *name);
int sh_var_set(const char *name, const char *val, int do_export,
               int do_readonly);
int sh_var_unset(const char *name);
int sh_is_name_start(char c);
int sh_is_name_char(char c);
int sh_is_assignment(const char *w);
const char *sh_alias_get(const char *name);
void sh_alias_set(const char *name, const char *val);
int sh_alias_unset(const char *name);
void sh_alias_clear(void);
const char *sh_hash_lookup(const char *name);
void sh_hash_store(const char *name, const char *path);
void sh_hash_clear(void);
char *sh_find_in_path(const char *cmd, int use_default_path);
sh_func_t *sh_func_find(const char *name);
void sh_func_set(const char *name, sh_node_t *body);
void sh_func_unset(const char *name);
void sh_check_traps(void);
void sh_run_exit_trap(void);
int sh_parse_sig(const char *spec);
const char *sh_sig_name(int sig);
void sh_set_positional(int argc, char **argv);

/* Lexer (lexer.c) */
int sh_lex(const char *src, sh_tokvec_t *out);
void sh_tokvec_free(sh_tokvec_t *tv);

/* Parser (parser.c) */
sh_node_t *sh_parse(const sh_tokvec_t *tv, int *err_out);
sh_node_t *sh_node_clone(const sh_node_t *n);
void sh_node_free(sh_node_t *n);

/* Expander (expand.c) */
int sh_expand_words(const sh_vec_t *raw, sh_vec_t *out);
char *sh_expand_single(const char *raw, int *err_out);
char *sh_expand_heredoc(const char *raw, int *err_out);
int64_t sh_eval_arith(const char *expr, int *err_out);

/* Executor (exec.c) */
int sh_eval_string(const char *src);
int sh_exec_node(const sh_node_t *n);
int sh_apply_redirs(const sh_redir_t *redirs, size_t nredirs);
int sh_exec_simple_argv(int argc, char **argv, int bypass_funcs,
                        int default_path);

/* Built-ins (builtins.c) */
int sh_is_builtin(const char *cmd);
int sh_is_special_builtin(const char *cmd);
int sh_run_builtin(int argc, char **argv);

#endif /* _SH_INTERNAL_H */
