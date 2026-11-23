/**
 * @file     find.c
 * @brief    Ring 3 find utility targeting POSIX.1-2024 XCU over <fnmatch.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: COMP_LIBC_0018 */

enum expr_kind {
    EX_TRUE = 0,
    EX_AND,
    EX_OR,
    EX_NOT,
    EX_NAME,
    EX_PATH,
    EX_TYPE,
    EX_SIZE,
    EX_PERM,
    EX_NEWER,
    EX_PRINT,
    EX_PRINT0,
    EX_PRUNE,
    EX_EXEC,
};

struct fexpr {
    enum expr_kind kind;
    struct fexpr *left;
    struct fexpr *right;
    const char *str;
    char type_ch;
    long num;
    int cmp_sign; /* -1 = <, 0 = ==, +1 = > */
    int size_bytes;
    int perm_all;
    time_t ref_mtime;
    char **exec_argv;
    int exec_argc;
    int exec_batch;
    char **batch_paths;
    size_t batch_len;
    size_t batch_cap;
};

struct find_ctx {
    int follow_links;
    int depth_first;
    int has_action;
    int pruned;
    int rc;
};

static const char *base_of(const char *path) {
    const char *sl = strrchr(path, '/');
    return sl ? (sl + 1) : path;
}

static int run_exec_argv(char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("find: fork");
        return 0;
    }
    if (pid == 0) {
        (void)execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

static int flush_batch_exec(struct fexpr *e, struct find_ctx *ctx) {
    if (!e) {
        return 0;
    }
    if (e->kind == EX_EXEC && e->exec_batch && e->batch_len > 0u) {
        size_t total = (size_t)e->exec_argc + e->batch_len;
        char **av = (char **)calloc(total + 1u, sizeof(char *));
        if (av) {
            size_t k = 0u;
            for (int i = 0; i < e->exec_argc; i++) {
                if (strcmp(e->exec_argv[i], "{}") == 0) {
                    for (size_t j = 0u; j < e->batch_len; j++) {
                        av[k++] = e->batch_paths[j];
                    }
                } else {
                    av[k++] = e->exec_argv[i];
                }
            }
            av[k] = NULL;
            if (!run_exec_argv(av)) {
                ctx->rc = 1;
            }
            free(av);
        }
        for (size_t j = 0u; j < e->batch_len; j++) {
            free(e->batch_paths[j]);
        }
        free(e->batch_paths);
        e->batch_paths = NULL;
        e->batch_len = 0u;
        e->batch_cap = 0u;
    }
    (void)flush_batch_exec(e->left, ctx);
    (void)flush_batch_exec(e->right, ctx);
    return 0;
}

static int eval_expr(struct fexpr *e, const char *path, const struct stat *st,
                     struct find_ctx *ctx) {
    if (!e) {
        return 1;
    }
    switch (e->kind) {
    case EX_TRUE:
        return 1;
    case EX_AND:
        return eval_expr(e->left, path, st, ctx) &&
               eval_expr(e->right, path, st, ctx);
    case EX_OR:
        return eval_expr(e->left, path, st, ctx) ||
               eval_expr(e->right, path, st, ctx);
    case EX_NOT:
        return !eval_expr(e->left, path, st, ctx);
    case EX_NAME:
        return fnmatch(e->str, base_of(path), 0) == 0;
    case EX_PATH:
        return fnmatch(e->str, path, 0) == 0;
    case EX_TYPE:
        switch (e->type_ch) {
        case 'f':
            return S_ISREG(st->st_mode);
        case 'd':
            return S_ISDIR(st->st_mode);
        case 'l':
            return S_ISLNK(st->st_mode);
        case 'p':
            return S_ISFIFO(st->st_mode);
        case 's':
            return S_ISSOCK(st->st_mode);
        case 'c':
            return S_ISCHR(st->st_mode);
        case 'b':
            return S_ISBLK(st->st_mode);
        default:
            return 0;
        }
    case EX_SIZE: {
        long sz = e->size_bytes ? (long)st->st_size
                                : (long)((st->st_size + 511) / 512);
        if (e->cmp_sign < 0) {
            return sz < e->num;
        }
        if (e->cmp_sign > 0) {
            return sz > e->num;
        }
        return sz == e->num;
    }
    case EX_PERM: {
        mode_t m = st->st_mode & 07777u;
        mode_t want = (mode_t)e->num;
        return e->perm_all ? ((m & want) == want) : (m == want);
    }
    case EX_NEWER:
        return st->st_mtime > e->ref_mtime;
    case EX_PRINT:
        puts(path);
        return 1;
    case EX_PRINT0:
        fputs(path, stdout);
        putchar('\0');
        return 1;
    case EX_PRUNE:
        ctx->pruned = 1;
        return 1;
    case EX_EXEC:
        if (e->exec_batch) {
            if (e->batch_len == e->batch_cap) {
                size_t ncap = e->batch_cap ? e->batch_cap * 2u : 16u;
                char **np =
                    (char **)reallocarray(e->batch_paths, ncap, sizeof(char *));
                if (!np) {
                    return 0;
                }
                e->batch_paths = np;
                e->batch_cap = ncap;
            }
            e->batch_paths[e->batch_len++] = strdup(path);
            return 1;
        } else {
            char **av =
                (char **)calloc((size_t)e->exec_argc + 1u, sizeof(char *));
            if (!av) {
                return 0;
            }
            for (int i = 0; i < e->exec_argc; i++) {
                av[i] = (strcmp(e->exec_argv[i], "{}") == 0) ? (char *)path
                                                             : e->exec_argv[i];
            }
            av[e->exec_argc] = NULL;
            int ok = run_exec_argv(av);
            free(av);
            return ok;
        }
    }
    return 0;
}

static void walk_path(const char *path, struct fexpr *expr,
                      struct find_ctx *ctx) {
    struct stat st;
    int sr = ctx->follow_links ? stat(path, &st) : lstat(path, &st);
    if (sr != 0) {
        perror(path);
        ctx->rc = 1;
        return;
    }

    ctx->pruned = 0;
    if (!ctx->depth_first) {
        int matched = eval_expr(expr, path, &st, ctx);
        if (matched && !ctx->has_action) {
            puts(path);
        }
    }

    if (S_ISDIR(st.st_mode) && !ctx->pruned) {
        DIR *dp = opendir(path);
        if (!dp) {
            perror(path);
            ctx->rc = 1;
        } else {
            struct dirent *de;
            while ((de = readdir(dp)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 ||
                    strcmp(de->d_name, "..") == 0) {
                    continue;
                }
                char child[PATH_MAX];
                size_t plen = strlen(path);
                if (plen > 0u && path[plen - 1u] == '/') {
                    (void)snprintf(child, sizeof(child), "%s%s", path,
                                   de->d_name);
                } else {
                    (void)snprintf(child, sizeof(child), "%s/%s", path,
                                   de->d_name);
                }
                walk_path(child, expr, ctx);
            }
            (void)closedir(dp);
        }
    }

    if (ctx->depth_first) {
        int matched = eval_expr(expr, path, &st, ctx);
        if (matched && !ctx->has_action) {
            puts(path);
        }
    }
}

struct fparser {
    char **argv;
    int argc;
    int pos;
    struct find_ctx *ctx;
    int err;
};

static struct fexpr *new_fexpr(enum expr_kind k) {
    struct fexpr *e = (struct fexpr *)calloc(1u, sizeof(*e));
    if (e) {
        e->kind = k;
    }
    return e;
}

static struct fexpr *parse_or(struct fparser *fp);

static struct fexpr *parse_primary(struct fparser *fp) {
    if (fp->pos >= fp->argc) {
        return NULL;
    }
    const char *tok = fp->argv[fp->pos];
    if (strcmp(tok, "(") == 0) {
        fp->pos++;
        struct fexpr *sub = parse_or(fp);
        if (fp->pos >= fp->argc || strcmp(fp->argv[fp->pos], ")") != 0) {
            fprintf(stderr, "find: missing ')'\n");
            fp->err = 1;
            return sub;
        }
        fp->pos++;
        return sub;
    }
    if (strcmp(tok, "!") == 0 || strcmp(tok, "-not") == 0) {
        fp->pos++;
        struct fexpr *n = new_fexpr(EX_NOT);
        n->left = parse_primary(fp);
        return n;
    }
    if (strcmp(tok, "-name") == 0 || strcmp(tok, "-path") == 0) {
        int is_path = (tok[1] == 'p');
        fp->pos++;
        if (fp->pos >= fp->argc) {
            fp->err = 1;
            return NULL;
        }
        struct fexpr *e = new_fexpr(is_path ? EX_PATH : EX_NAME);
        e->str = fp->argv[fp->pos++];
        return e;
    }
    if (strcmp(tok, "-type") == 0) {
        fp->pos++;
        if (fp->pos >= fp->argc) {
            fp->err = 1;
            return NULL;
        }
        struct fexpr *e = new_fexpr(EX_TYPE);
        e->type_ch = fp->argv[fp->pos++][0];
        return e;
    }
    if (strcmp(tok, "-size") == 0) {
        fp->pos++;
        if (fp->pos >= fp->argc) {
            fp->err = 1;
            return NULL;
        }
        const char *s = fp->argv[fp->pos++];
        struct fexpr *e = new_fexpr(EX_SIZE);
        if (*s == '+') {
            e->cmp_sign = 1;
            s++;
        } else if (*s == '-') {
            e->cmp_sign = -1;
            s++;
        }
        char *end = NULL;
        e->num = strtol(s, &end, 10);
        if (end && *end == 'c') {
            e->size_bytes = 1;
        }
        return e;
    }
    if (strcmp(tok, "-perm") == 0) {
        fp->pos++;
        if (fp->pos >= fp->argc) {
            fp->err = 1;
            return NULL;
        }
        const char *s = fp->argv[fp->pos++];
        struct fexpr *e = new_fexpr(EX_PERM);
        if (*s == '-') {
            e->perm_all = 1;
            s++;
        }
        e->num = strtol(s, NULL, 8);
        return e;
    }
    if (strcmp(tok, "-newer") == 0) {
        fp->pos++;
        if (fp->pos >= fp->argc) {
            fp->err = 1;
            return NULL;
        }
        struct stat rst;
        if (stat(fp->argv[fp->pos++], &rst) != 0) {
            fp->err = 1;
            return NULL;
        }
        struct fexpr *e = new_fexpr(EX_NEWER);
        e->ref_mtime = rst.st_mtime;
        return e;
    }
    if (strcmp(tok, "-print") == 0) {
        fp->pos++;
        fp->ctx->has_action = 1;
        return new_fexpr(EX_PRINT);
    }
    if (strcmp(tok, "-print0") == 0) {
        fp->pos++;
        fp->ctx->has_action = 1;
        return new_fexpr(EX_PRINT0);
    }
    if (strcmp(tok, "-prune") == 0) {
        fp->pos++;
        return new_fexpr(EX_PRUNE);
    }
    if (strcmp(tok, "-depth") == 0) {
        fp->pos++;
        fp->ctx->depth_first = 1;
        return new_fexpr(EX_TRUE);
    }
    if (strcmp(tok, "-xdev") == 0) {
        fp->pos++;
        return new_fexpr(EX_TRUE);
    }
    if (strcmp(tok, "-exec") == 0) {
        fp->pos++;
        fp->ctx->has_action = 1;
        int start = fp->pos;
        while (fp->pos < fp->argc && strcmp(fp->argv[fp->pos], ";") != 0 &&
               strcmp(fp->argv[fp->pos], "+") != 0) {
            fp->pos++;
        }
        if (fp->pos >= fp->argc || fp->pos == start) {
            fprintf(stderr, "find: missing terminator for -exec\n");
            fp->err = 1;
            return NULL;
        }
        struct fexpr *e = new_fexpr(EX_EXEC);
        e->exec_argv = &fp->argv[start];
        e->exec_argc = fp->pos - start;
        e->exec_batch = (strcmp(fp->argv[fp->pos], "+") == 0);
        fp->pos++;
        return e;
    }
    fprintf(stderr, "find: unknown predicate '%s'\n", tok);
    fp->err = 1;
    return NULL;
}

static struct fexpr *parse_and(struct fparser *fp) {
    struct fexpr *left = parse_primary(fp);
    while (!fp->err && fp->pos < fp->argc) {
        const char *tok = fp->argv[fp->pos];
        if (strcmp(tok, "-o") == 0 || strcmp(tok, "-or") == 0 ||
            strcmp(tok, ")") == 0) {
            break;
        }
        if (strcmp(tok, "-a") == 0 || strcmp(tok, "-and") == 0) {
            fp->pos++;
        }
        struct fexpr *right = parse_primary(fp);
        struct fexpr *a = new_fexpr(EX_AND);
        a->left = left;
        a->right = right;
        left = a;
    }
    return left;
}

static struct fexpr *parse_or(struct fparser *fp) {
    struct fexpr *left = parse_and(fp);
    while (!fp->err && fp->pos < fp->argc &&
           (strcmp(fp->argv[fp->pos], "-o") == 0 ||
            strcmp(fp->argv[fp->pos], "-or") == 0)) {
        fp->pos++;
        struct fexpr *right = parse_and(fp);
        struct fexpr *o = new_fexpr(EX_OR);
        o->left = left;
        o->right = right;
        left = o;
    }
    return left;
}

static void free_fexpr(struct fexpr *e) {
    if (!e) {
        return;
    }
    free_fexpr(e->left);
    free_fexpr(e->right);
    for (size_t i = 0u; i < e->batch_len; i++) {
        free(e->batch_paths[i]);
    }
    free(e->batch_paths);
    free(e);
}

int main(int argc, char **argv) {
    struct find_ctx ctx = {0};
    int idx = 1;
    while (idx < argc &&
           (strcmp(argv[idx], "-H") == 0 || strcmp(argv[idx], "-L") == 0)) {
        ctx.follow_links = (argv[idx][1] == 'L');
        idx++;
    }
    int path_start = idx;
    while (idx < argc && argv[idx][0] != '-' && strcmp(argv[idx], "!") != 0 &&
           strcmp(argv[idx], "(") != 0) {
        idx++;
    }
    int npaths = idx - path_start;

    struct fexpr *expr = NULL;
    if (idx < argc) {
        struct fparser fp = {
            .argv = argv,
            .argc = argc,
            .pos = idx,
            .ctx = &ctx,
            .err = 0,
        };
        expr = parse_or(&fp);
        if (fp.err || fp.pos < argc) {
            free_fexpr(expr);
            return 1;
        }
    }

    if (npaths == 0) {
        walk_path(".", expr, &ctx);
    } else {
        for (int i = 0; i < npaths; i++) {
            walk_path(argv[path_start + i], expr, &ctx);
        }
    }
    (void)flush_batch_exec(expr, &ctx);
    free_fexpr(expr);
    return ctx.rc;
}
