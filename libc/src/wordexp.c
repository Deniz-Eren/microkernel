/**
 * @file     wordexp.c
 * @brief    POSIX.1-2024 shell word expansion (wordexp and wordfree)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <glob.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

/* need:: COMP_LIBC_0014 */

/* Internal marker byte separating quoted vs unquoted expansion pieces. */
#define WE_QUOTED_MARK '\x01'

struct we_buf {
    char *data;
    size_t len;
    size_t cap;
};

static int buf_push(struct we_buf *b, char c) {
    if (b->len + 2u > b->cap) {
        size_t ncap = b->cap ? b->cap * 2u : 64u;
        char *nd = (char *)realloc(b->data, ncap);
        if (!nd) {
            return WRDE_NOSPACE;
        }
        memset(nd + b->cap, 0, ncap - b->cap);
        b->data = nd;
        b->cap = ncap;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return 0;
}

static int buf_puts(struct we_buf *b, const char *s) {
    for (size_t i = 0u; s[i] != '\0'; i++) {
        int rc = buf_push(b, s[i]);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

struct we_vec {
    char **items;
    size_t len;
    size_t cap;
};

static int we_vec_push(struct we_vec *v, char *s) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2u : 16u;
        char **ni = (char **)reallocarray(v->items, ncap, sizeof(char *));
        if (!ni) {
            return WRDE_NOSPACE;
        }
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->len++] = s;
    return 0;
}

static void we_vec_free(struct we_vec *v) {
    for (size_t i = 0u; i < v->len; i++) {
        free(v->items[i]);
    }
    free(v->items);
    v->items = NULL;
    v->len = 0u;
    v->cap = 0u;
}

/* Arithmetic evaluator for $((...)). */
struct arith_ctx {
    const char *p;
    int err;
};

static long long arith_or(struct arith_ctx *c);

static void arith_ws(struct arith_ctx *c) {
    while (*c->p && isspace((unsigned char)*c->p)) {
        c->p++;
    }
}

static long long arith_primary(struct arith_ctx *c) {
    arith_ws(c);
    if (*c->p == '(') {
        c->p++;
        long long v = arith_or(c);
        arith_ws(c);
        if (*c->p != ')') {
            c->err = WRDE_SYNTAX;
            return 0;
        }
        c->p++;
        return v;
    }
    if (isdigit((unsigned char)*c->p)) {
        char *ep = NULL;
        long long v = strtoll(c->p, &ep, 0);
        c->p = ep;
        return v;
    }
    if (isalpha((unsigned char)*c->p) || *c->p == '_') {
        const char *st = c->p;
        while (isalnum((unsigned char)*c->p) || *c->p == '_') {
            c->p++;
        }
        size_t len = (size_t)(c->p - st);
        char name[128];
        if (len >= sizeof(name)) {
            len = sizeof(name) - 1u;
        }
        memcpy(name, st, len);
        name[len] = '\0';
        const char *val = getenv(name);
        return (val && *val) ? strtoll(val, NULL, 0) : 0LL;
    }
    c->err = WRDE_SYNTAX;
    return 0;
}

static long long arith_unary(struct arith_ctx *c) {
    arith_ws(c);
    if (*c->p == '+') {
        c->p++;
        return arith_unary(c);
    }
    if (*c->p == '-') {
        c->p++;
        return -arith_unary(c);
    }
    if (*c->p == '!') {
        c->p++;
        return !arith_unary(c);
    }
    if (*c->p == '~') {
        c->p++;
        return ~arith_unary(c);
    }
    return arith_primary(c);
}

static long long arith_mul(struct arith_ctx *c) {
    long long v = arith_unary(c);
    for (;;) {
        arith_ws(c);
        char op = *c->p;
        if (op != '*' && op != '/' && op != '%') {
            break;
        }
        c->p++;
        long long r = arith_unary(c);
        if (op == '*') {
            v *= r;
        } else if (r == 0) {
            c->err = WRDE_SYNTAX;
            return 0;
        } else if (op == '/') {
            v /= r;
        } else {
            v %= r;
        }
    }
    return v;
}

static long long arith_add(struct arith_ctx *c) {
    long long v = arith_mul(c);
    for (;;) {
        arith_ws(c);
        char op = *c->p;
        if (op != '+' && op != '-') {
            break;
        }
        c->p++;
        long long r = arith_mul(c);
        v = (op == '+') ? (v + r) : (v - r);
    }
    return v;
}

static long long arith_rel(struct arith_ctx *c) {
    long long v = arith_add(c);
    for (;;) {
        arith_ws(c);
        if (c->p[0] == '<' && c->p[1] == '=') {
            c->p += 2;
            v = (v <= arith_add(c));
        } else if (c->p[0] == '>' && c->p[1] == '=') {
            c->p += 2;
            v = (v >= arith_add(c));
        } else if (c->p[0] == '<' && c->p[1] != '<') {
            c->p++;
            v = (v < arith_add(c));
        } else if (c->p[0] == '>' && c->p[1] != '>') {
            c->p++;
            v = (v > arith_add(c));
        } else if (c->p[0] == '=' && c->p[1] == '=') {
            c->p += 2;
            v = (v == arith_add(c));
        } else if (c->p[0] == '!' && c->p[1] == '=') {
            c->p += 2;
            v = (v != arith_add(c));
        } else {
            break;
        }
    }
    return v;
}

static long long arith_and(struct arith_ctx *c) {
    long long v = arith_rel(c);
    for (;;) {
        arith_ws(c);
        if (c->p[0] == '&' && c->p[1] == '&') {
            c->p += 2;
            long long r = arith_rel(c);
            v = (v && r);
        } else {
            break;
        }
    }
    return v;
}

static long long arith_or(struct arith_ctx *c) {
    long long v = arith_and(c);
    for (;;) {
        arith_ws(c);
        if (c->p[0] == '|' && c->p[1] == '|') {
            c->p += 2;
            long long r = arith_and(c);
            v = (v || r);
        } else {
            break;
        }
    }
    return v;
}

static int run_cmdsub(const char *cmd, int flags, struct we_buf *out) {
    if (flags & WRDE_NOCMD) {
        return WRDE_CMDSUB;
    }
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return WRDE_NOSPACE;
    }
    size_t start = out->len;
    char tmp[128];
    size_t n;
    while ((n = fread(tmp, 1u, sizeof(tmp), fp)) > 0u) {
        for (size_t i = 0u; i < n; i++) {
            int rc = buf_push(out, tmp[i]);
            if (rc != 0) {
                (void)pclose(fp);
                return rc;
            }
        }
    }
    (void)pclose(fp);
    while (out->len > start && out->data[out->len - 1u] == '\n') {
        out->data[--out->len] = '\0';
    }
    return 0;
}

static int is_bad_unquoted(char c) {
    return c == '\n' || c == '|' || c == '&' || c == ';' || c == '<' ||
           c == '>' || c == '(' || c == ')' || c == '{' || c == '}';
}

static int expand_dollar(const char **pp, int flags, int in_dquote,
                         struct we_buf *out) {
    const char *p = *pp + 1; /* skip '$' */
    if (p[0] == '(' && p[1] == '(') {
        p += 2;
        int depth = 1;
        struct we_buf expr = {0};
        while (*p != '\0' && depth > 0) {
            if (p[0] == '(' && p[1] == '(') {
                depth++;
                if (buf_puts(&expr, "((") != 0) {
                    free(expr.data);
                    return WRDE_NOSPACE;
                }
                p += 2;
            } else if (p[0] == ')' && p[1] == ')') {
                depth--;
                if (depth > 0 && buf_puts(&expr, "))") != 0) {
                    free(expr.data);
                    return WRDE_NOSPACE;
                }
                p += 2;
            } else if (p[0] == '$') {
                int rc = expand_dollar(&p, flags, 1, &expr);
                if (rc != 0) {
                    free(expr.data);
                    return rc;
                }
            } else {
                if (buf_push(&expr, *p++) != 0) {
                    free(expr.data);
                    return WRDE_NOSPACE;
                }
            }
        }
        if (depth != 0) {
            free(expr.data);
            return WRDE_SYNTAX;
        }
        struct arith_ctx ac = {.p = expr.data ? expr.data : "", .err = 0};
        long long val = arith_or(&ac);
        arith_ws(&ac);
        if (ac.err != 0 || *ac.p != '\0') {
            free(expr.data);
            return WRDE_SYNTAX;
        }
        free(expr.data);
        char num[32];
        (void)snprintf(num, sizeof(num), "%lld", val);
        *pp = p;
        return buf_puts(out, num);
    }

    if (p[0] == '(') {
        p++;
        int depth = 1;
        struct we_buf cmd = {0};
        while (*p != '\0' && depth > 0) {
            if (*p == '(') {
                depth++;
            } else if (*p == ')') {
                depth--;
                if (depth == 0) {
                    p++;
                    break;
                }
            }
            if (buf_push(&cmd, *p++) != 0) {
                free(cmd.data);
                return WRDE_NOSPACE;
            }
        }
        if (depth != 0) {
            free(cmd.data);
            return WRDE_SYNTAX;
        }
        int rc = run_cmdsub(cmd.data ? cmd.data : "", flags, out);
        free(cmd.data);
        *pp = p;
        return rc;
    }

    if (*p == '{') {
        p++;
        int want_len = 0;
        if (*p == '#') {
            want_len = 1;
            p++;
        }
        const char *ns = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
            p++;
        }
        size_t nlen = (size_t)(p - ns);
        if (nlen == 0u) {
            return WRDE_SYNTAX;
        }
        char name[128];
        if (nlen >= sizeof(name)) {
            nlen = sizeof(name) - 1u;
        }
        memcpy(name, ns, nlen);
        name[nlen] = '\0';

        int colon = 0;
        char op = '\0';
        struct we_buf arg = {0};
        if (*p == ':') {
            colon = 1;
            p++;
        }
        if (*p == '-' || *p == '+' || *p == '?') {
            op = *p++;
            int bdepth = 1;
            while (*p != '\0' && bdepth > 0) {
                if (*p == '{') {
                    bdepth++;
                } else if (*p == '}') {
                    bdepth--;
                    if (bdepth == 0) {
                        break;
                    }
                }
                if (buf_push(&arg, *p++) != 0) {
                    free(arg.data);
                    return WRDE_NOSPACE;
                }
            }
        }
        if (*p != '}') {
            free(arg.data);
            return WRDE_SYNTAX;
        }
        p++;
        *pp = p;

        const char *val = getenv(name);
        if (want_len) {
            free(arg.data);
            if (!val && (flags & WRDE_UNDEF)) {
                return WRDE_BADVAL;
            }
            char num[32];
            (void)snprintf(num, sizeof(num), "%zu", val ? strlen(val) : 0u);
            return buf_puts(out, num);
        }
        int is_set = (val != NULL && (!colon || val[0] != '\0'));
        int rc = 0;
        if (op == '-') {
            rc = buf_puts(out, is_set ? val : (arg.data ? arg.data : ""));
        } else if (op == '+') {
            rc = is_set ? buf_puts(out, arg.data ? arg.data : "") : 0;
        } else {
            if (!val && (flags & WRDE_UNDEF)) {
                free(arg.data);
                return WRDE_BADVAL;
            }
            rc = val ? buf_puts(out, val) : 0;
        }
        free(arg.data);
        return rc;
    }

    if (isalpha((unsigned char)*p) || *p == '_') {
        const char *ns = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
            p++;
        }
        size_t nlen = (size_t)(p - ns);
        char name[128];
        if (nlen >= sizeof(name)) {
            nlen = sizeof(name) - 1u;
        }
        memcpy(name, ns, nlen);
        name[nlen] = '\0';
        *pp = p;
        const char *val = getenv(name);
        if (!val) {
            if (flags & WRDE_UNDEF) {
                return WRDE_BADVAL;
            }
            return 0;
        }
        return buf_puts(out, val);
    }

    (void)in_dquote;
    *pp = p;
    return buf_push(out, '$');
}

static int word_has_unquoted_glob(const char *w) {
    int quoted = 0;
    for (size_t i = 0u; w[i] != '\0'; i++) {
        if (w[i] == WE_QUOTED_MARK) {
            quoted = !quoted;
            continue;
        }
        if (!quoted && (w[i] == '*' || w[i] == '?' || w[i] == '[')) {
            return 1;
        }
    }
    return 0;
}

static char *strip_marks(const char *w) {
    size_t len = strlen(w);
    char *out = (char *)malloc(len + 1u);
    if (!out) {
        return NULL;
    }
    size_t j = 0u;
    for (size_t i = 0u; i < len && w[i] != '\0'; i++) {
        if (w[i] != WE_QUOTED_MARK) {
            out[j++] = w[i];
        }
    }
    out[j] = '\0';
    return out;
}

static char *build_glob_pattern(const char *w) {
    size_t len = strlen(w);
    char *out = (char *)malloc(len * 2u + 1u);
    if (!out) {
        return NULL;
    }
    int quoted = 0;
    size_t j = 0u;
    for (size_t i = 0u; i < len && w[i] != '\0'; i++) {
        if (w[i] == WE_QUOTED_MARK) {
            quoted = !quoted;
            continue;
        }
        if (quoted &&
            (w[i] == '*' || w[i] == '?' || w[i] == '[' || w[i] == '\\')) {
            out[j++] = '\\';
        }
        out[j++] = w[i];
    }
    out[j] = '\0';
    return out;
}

static int finalize_word(struct we_vec *out, const char *raw) {
    if (!word_has_unquoted_glob(raw)) {
        char *clean = strip_marks(raw);
        if (!clean) {
            return WRDE_NOSPACE;
        }
        int rc = we_vec_push(out, clean);
        if (rc != 0) {
            free(clean);
        }
        return rc;
    }
    char *gpat = build_glob_pattern(raw);
    if (!gpat) {
        return WRDE_NOSPACE;
    }
    glob_t g = {0};
    int gr = glob(gpat, 0, NULL, &g);
    free(gpat);
    if (gr == 0) {
        for (size_t i = 0u; i < g.gl_pathc; i++) {
            char *dup = strdup(g.gl_pathv[i]);
            if (!dup || we_vec_push(out, dup) != 0) {
                free(dup);
                globfree(&g);
                return WRDE_NOSPACE;
            }
        }
        globfree(&g);
        return 0;
    }
    globfree(&g);
    if (gr == GLOB_NOSPACE) {
        return WRDE_NOSPACE;
    }
    char *clean = strip_marks(raw);
    if (!clean) {
        return WRDE_NOSPACE;
    }
    int rc = we_vec_push(out, clean);
    if (rc != 0) {
        free(clean);
    }
    return rc;
}

int wordexp(const char *restrict words, wordexp_t *restrict pwordexp,
            int flags) {
    if (!words || !pwordexp) {
        return WRDE_SYNTAX;
    }
    if (flags & WRDE_REUSE) {
        wordfree(pwordexp);
    }
    size_t offs = (flags & WRDE_DOOFFS) ? pwordexp->we_offs : 0u;
    if (!(flags & WRDE_APPEND)) {
        pwordexp->we_wordc = 0u;
        pwordexp->we_wordv = NULL;
    }

    const char *ifs = getenv("IFS");
    if (!ifs) {
        ifs = " \t\n";
    }

    struct we_vec out = {0};
    struct we_buf cur = {0};
    int has_word = 0;
    const char *p = words;

    while (*p != '\0') {
        while (*p != '\0' && strchr(ifs, *p) != NULL &&
               isspace((unsigned char)*p)) {
            if (*p == '\n') {
                free(cur.data);
                we_vec_free(&out);
                return WRDE_BADCHAR;
            }
            p++;
        }
        if (*p == '\0') {
            break;
        }

        cur.len = 0u;
        if (cur.data) {
            cur.data[0] = '\0';
        }
        has_word = 0;

        /* Tilde expansion at start of unquoted word. */
        if (*p == '~' &&
            (p[1] == '\0' || p[1] == '/' || strchr(ifs, p[1]) != NULL)) {
            const char *home = getenv("HOME");
            if (!home) {
                struct passwd *pw = getpwuid(getuid());
                home = pw ? pw->pw_dir : "/";
            }
            int rc = buf_push(&cur, WE_QUOTED_MARK);
            if (rc == 0) {
                rc = buf_puts(&cur, home);
            }
            if (rc == 0) {
                rc = buf_push(&cur, WE_QUOTED_MARK);
            }
            if (rc != 0) {
                free(cur.data);
                we_vec_free(&out);
                return rc;
            }
            has_word = 1;
            p++;
        }

        while (*p != '\0' && strchr(ifs, *p) == NULL) {
            if (is_bad_unquoted(*p)) {
                free(cur.data);
                we_vec_free(&out);
                return WRDE_BADCHAR;
            }
            if (*p == '\'') {
                p++;
                int rc = buf_push(&cur, WE_QUOTED_MARK);
                has_word = 1;
                while (*p != '\0' && *p != '\'' && rc == 0) {
                    rc = buf_push(&cur, *p++);
                }
                if (rc != 0 || *p != '\'') {
                    free(cur.data);
                    we_vec_free(&out);
                    return rc != 0 ? rc : WRDE_SYNTAX;
                }
                p++;
                rc = buf_push(&cur, WE_QUOTED_MARK);
                if (rc != 0) {
                    free(cur.data);
                    we_vec_free(&out);
                    return rc;
                }
            } else if (*p == '"') {
                p++;
                int rc = buf_push(&cur, WE_QUOTED_MARK);
                has_word = 1;
                while (*p != '\0' && *p != '"' && rc == 0) {
                    if (*p == '\\' && (p[1] == '"' || p[1] == '\\' ||
                                       p[1] == '$' || p[1] == '`')) {
                        p++;
                        rc = buf_push(&cur, *p++);
                    } else if (*p == '$') {
                        rc = expand_dollar(&p, flags, 1, &cur);
                    } else if (*p == '`') {
                        p++;
                        struct we_buf cmd = {0};
                        while (*p != '\0' && *p != '`') {
                            if (*p == '\\' &&
                                (p[1] == '`' || p[1] == '\\' || p[1] == '$')) {
                                p++;
                            }
                            if (buf_push(&cmd, *p++) != 0) {
                                free(cmd.data);
                                rc = WRDE_NOSPACE;
                                break;
                            }
                        }
                        if (rc == 0) {
                            if (*p != '`') {
                                rc = WRDE_SYNTAX;
                            } else {
                                p++;
                                rc = run_cmdsub(cmd.data ? cmd.data : "", flags,
                                                &cur);
                            }
                        }
                        free(cmd.data);
                    } else {
                        rc = buf_push(&cur, *p++);
                    }
                }
                if (rc != 0 || *p != '"') {
                    free(cur.data);
                    we_vec_free(&out);
                    return rc != 0 ? rc : WRDE_SYNTAX;
                }
                p++;
                rc = buf_push(&cur, WE_QUOTED_MARK);
                if (rc != 0) {
                    free(cur.data);
                    we_vec_free(&out);
                    return rc;
                }
            } else if (*p == '\\') {
                p++;
                if (*p == '\0') {
                    free(cur.data);
                    we_vec_free(&out);
                    return WRDE_SYNTAX;
                }
                has_word = 1;
                if (buf_push(&cur, WE_QUOTED_MARK) != 0 ||
                    buf_push(&cur, *p++) != 0 ||
                    buf_push(&cur, WE_QUOTED_MARK) != 0) {
                    free(cur.data);
                    we_vec_free(&out);
                    return WRDE_NOSPACE;
                }
            } else if (*p == '$') {
                struct we_buf exp = {0};
                int rc = expand_dollar(&p, flags, 0, &exp);
                if (rc != 0) {
                    free(exp.data);
                    free(cur.data);
                    we_vec_free(&out);
                    return rc;
                }
                const char *ep = exp.data ? exp.data : "";
                while (*ep != '\0') {
                    if (strchr(ifs, *ep) != NULL) {
                        if (has_word) {
                            int frc =
                                finalize_word(&out, cur.data ? cur.data : "");
                            if (frc != 0) {
                                free(exp.data);
                                free(cur.data);
                                we_vec_free(&out);
                                return frc;
                            }
                            cur.len = 0u;
                            if (cur.data) {
                                cur.data[0] = '\0';
                            }
                            has_word = 0;
                        }
                        ep++;
                        continue;
                    }
                    has_word = 1;
                    if (buf_push(&cur, *ep++) != 0) {
                        free(exp.data);
                        free(cur.data);
                        we_vec_free(&out);
                        return WRDE_NOSPACE;
                    }
                }
                free(exp.data);
            } else if (*p == '`') {
                p++;
                struct we_buf cmd = {0};
                while (*p != '\0' && *p != '`') {
                    if (*p == '\\' && (p[1] == '`' || p[1] == '\\')) {
                        p++;
                    }
                    if (buf_push(&cmd, *p++) != 0) {
                        free(cmd.data);
                        free(cur.data);
                        we_vec_free(&out);
                        return WRDE_NOSPACE;
                    }
                }
                if (*p != '`') {
                    free(cmd.data);
                    free(cur.data);
                    we_vec_free(&out);
                    return WRDE_SYNTAX;
                }
                p++;
                struct we_buf sub = {0};
                int rc = run_cmdsub(cmd.data ? cmd.data : "", flags, &sub);
                free(cmd.data);
                if (rc != 0) {
                    free(sub.data);
                    free(cur.data);
                    we_vec_free(&out);
                    return rc;
                }
                const char *sp = sub.data ? sub.data : "";
                while (*sp != '\0') {
                    if (strchr(ifs, *sp) != NULL) {
                        if (has_word) {
                            int frc =
                                finalize_word(&out, cur.data ? cur.data : "");
                            if (frc != 0) {
                                free(sub.data);
                                free(cur.data);
                                we_vec_free(&out);
                                return frc;
                            }
                            cur.len = 0u;
                            if (cur.data) {
                                cur.data[0] = '\0';
                            }
                            has_word = 0;
                        }
                        sp++;
                        continue;
                    }
                    has_word = 1;
                    if (buf_push(&cur, *sp++) != 0) {
                        free(sub.data);
                        free(cur.data);
                        we_vec_free(&out);
                        return WRDE_NOSPACE;
                    }
                }
                free(sub.data);
            } else {
                has_word = 1;
                if (buf_push(&cur, *p++) != 0) {
                    free(cur.data);
                    we_vec_free(&out);
                    return WRDE_NOSPACE;
                }
            }
        }

        if (has_word) {
            int frc = finalize_word(&out, cur.data ? cur.data : "");
            if (frc != 0) {
                free(cur.data);
                we_vec_free(&out);
                return frc;
            }
        }
        if (*p != '\0' && strchr(ifs, *p) != NULL &&
            !isspace((unsigned char)*p)) {
            p++;
        }
    }
    free(cur.data);

    size_t old_count = (flags & WRDE_APPEND) ? pwordexp->we_wordc : 0u;
    size_t total = offs + old_count + out.len + 1u;
    char **nv =
        (char **)reallocarray((flags & WRDE_APPEND) ? pwordexp->we_wordv : NULL,
                              total, sizeof(char *));
    if (!nv) {
        we_vec_free(&out);
        return WRDE_NOSPACE;
    }
    if (!(flags & WRDE_APPEND)) {
        for (size_t i = 0u; i < offs; i++) {
            nv[i] = NULL;
        }
    }
    for (size_t i = 0u; i < out.len; i++) {
        nv[offs + old_count + i] = out.items[i];
    }
    nv[offs + old_count + out.len] = NULL;
    free(out.items);

    pwordexp->we_wordv = nv;
    pwordexp->we_wordc = old_count + out.len;
    return 0;
}

void wordfree(wordexp_t *pwordexp) {
    if (!pwordexp || !pwordexp->we_wordv) {
        return;
    }
    size_t idx = 0u;
    while (idx < pwordexp->we_offs && pwordexp->we_wordv[idx] == NULL) {
        idx++;
    }
    for (size_t i = 0u; i < pwordexp->we_wordc; i++) {
        free(pwordexp->we_wordv[idx + i]);
    }
    free(pwordexp->we_wordv);
    pwordexp->we_wordv = NULL;
    pwordexp->we_wordc = 0u;
}
