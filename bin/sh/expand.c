/**
 * @file     expand.c
 * @brief    POSIX.1-2024 shell word, parameter, arithmetic, and glob expansion
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sh.h"

/* need:: COMP_LIBC_0017 */

/* ---- Arithmetic evaluator for $((...)) ---- */

struct arith {
    const char *p;
    int err;
};

static int64_t ar_assign(struct arith *a);

static void ar_ws(struct arith *a) {
    while (*a->p != '\0' && isspace((unsigned char)*a->p)) {
        a->p++;
    }
}

static int64_t ar_primary(struct arith *a) {
    ar_ws(a);
    if (*a->p == '(') {
        a->p++;
        int64_t v = ar_assign(a);
        ar_ws(a);
        if (*a->p == ')') {
            a->p++;
        } else {
            a->err = 1;
        }
        return v;
    }
    if (isdigit((unsigned char)*a->p)) {
        char *end = NULL;
        long long v = strtoll(a->p, &end, 0);
        a->p = end;
        return (int64_t)v;
    }
    if (sh_is_name_start(*a->p)) {
        char name[64];
        size_t len = 0u;
        while (sh_is_name_char(*a->p)) {
            if (len + 1u < sizeof(name)) {
                name[len++] = *a->p;
            }
            a->p++;
        }
        name[len] = '\0';
        const char *val = sh_var_get(name);
        if (!val || val[0] == '\0') {
            return 0;
        }
        return (int64_t)strtoll(val, NULL, 0);
    }
    a->err = 1;
    return 0;
}

static int64_t ar_unary(struct arith *a) {
    ar_ws(a);
    if (*a->p == '+') {
        a->p++;
        return ar_unary(a);
    }
    if (*a->p == '-') {
        a->p++;
        return -ar_unary(a);
    }
    if (*a->p == '!') {
        a->p++;
        return !ar_unary(a);
    }
    if (*a->p == '~') {
        a->p++;
        return ~ar_unary(a);
    }
    return ar_primary(a);
}

static int64_t ar_mul(struct arith *a) {
    int64_t v = ar_unary(a);
    for (;;) {
        ar_ws(a);
        char op = *a->p;
        if ((op != '*' && op != '/' && op != '%') || a->p[1] == '=') {
            break;
        }
        a->p++;
        int64_t r = ar_unary(a);
        if (op == '*') {
            v *= r;
        } else if (r == 0) {
            a->err = 1;
            return 0;
        } else if (op == '/') {
            v /= r;
        } else {
            v %= r;
        }
    }
    return v;
}

static int64_t ar_add(struct arith *a) {
    int64_t v = ar_mul(a);
    for (;;) {
        ar_ws(a);
        char op = *a->p;
        if ((op != '+' && op != '-') || a->p[1] == '=') {
            break;
        }
        a->p++;
        int64_t r = ar_mul(a);
        v = (op == '+') ? (v + r) : (v - r);
    }
    return v;
}

static int64_t ar_shift(struct arith *a) {
    int64_t v = ar_add(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '<' && a->p[1] == '<') {
            a->p += 2;
            v <<= (ar_add(a) & 63);
        } else if (a->p[0] == '>' && a->p[1] == '>') {
            a->p += 2;
            v >>= (ar_add(a) & 63);
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_rel(struct arith *a) {
    int64_t v = ar_shift(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '<' && a->p[1] == '=') {
            a->p += 2;
            v = (v <= ar_shift(a));
        } else if (a->p[0] == '>' && a->p[1] == '=') {
            a->p += 2;
            v = (v >= ar_shift(a));
        } else if (a->p[0] == '<') {
            a->p++;
            v = (v < ar_shift(a));
        } else if (a->p[0] == '>') {
            a->p++;
            v = (v > ar_shift(a));
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_eq(struct arith *a) {
    int64_t v = ar_rel(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '=' && a->p[1] == '=') {
            a->p += 2;
            v = (v == ar_rel(a));
        } else if (a->p[0] == '!' && a->p[1] == '=') {
            a->p += 2;
            v = (v != ar_rel(a));
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_bitand(struct arith *a) {
    int64_t v = ar_eq(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '&' && a->p[1] != '&') {
            a->p++;
            v &= ar_eq(a);
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_bitxor(struct arith *a) {
    int64_t v = ar_bitand(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '^') {
            a->p++;
            v ^= ar_bitand(a);
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_bitor(struct arith *a) {
    int64_t v = ar_bitxor(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '|' && a->p[1] != '|') {
            a->p++;
            v |= ar_bitxor(a);
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_logand(struct arith *a) {
    int64_t v = ar_bitor(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '&' && a->p[1] == '&') {
            a->p += 2;
            int64_t r = ar_bitor(a);
            v = (v && r) ? 1 : 0;
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_logor(struct arith *a) {
    int64_t v = ar_logand(a);
    for (;;) {
        ar_ws(a);
        if (a->p[0] == '|' && a->p[1] == '|') {
            a->p += 2;
            int64_t r = ar_logand(a);
            v = (v || r) ? 1 : 0;
        } else {
            break;
        }
    }
    return v;
}

static int64_t ar_ternary(struct arith *a) {
    int64_t cond = ar_logor(a);
    ar_ws(a);
    if (*a->p == '?') {
        a->p++;
        int64_t t = ar_assign(a);
        ar_ws(a);
        if (*a->p == ':') {
            a->p++;
        } else {
            a->err = 1;
        }
        int64_t f = ar_ternary(a);
        return cond ? t : f;
    }
    return cond;
}

static int64_t ar_assign(struct arith *a) {
    ar_ws(a);
    const char *save = a->p;
    if (sh_is_name_start(*a->p)) {
        char name[64];
        size_t len = 0u;
        const char *q = a->p;
        while (sh_is_name_char(*q)) {
            if (len + 1u < sizeof(name)) {
                name[len++] = *q;
            }
            q++;
        }
        name[len] = '\0';
        while (*q != '\0' && isspace((unsigned char)*q)) {
            q++;
        }
        char op = 0;
        if (q[0] == '=' && q[1] != '=') {
            op = '=';
            q++;
        } else if ((q[0] == '+' || q[0] == '-' || q[0] == '*' || q[0] == '/' ||
                    q[0] == '%') &&
                   q[1] == '=') {
            op = q[0];
            q += 2;
        }
        if (op != 0) {
            a->p = q;
            int64_t rhs = ar_assign(a);
            int64_t cur = 0;
            const char *cv = sh_var_get(name);
            if (cv && cv[0] != '\0') {
                cur = (int64_t)strtoll(cv, NULL, 0);
            }
            int64_t nv = rhs;
            if (op == '+') {
                nv = cur + rhs;
            } else if (op == '-') {
                nv = cur - rhs;
            } else if (op == '*') {
                nv = cur * rhs;
            } else if (op == '/' || op == '%') {
                if (rhs == 0) {
                    a->err = 1;
                    return 0;
                }
                nv = (op == '/') ? (cur / rhs) : (cur % rhs);
            }
            char nbuf[32];
            (void)snprintf(nbuf, sizeof(nbuf), "%lld", (long long)nv);
            (void)sh_var_set(name, nbuf, g_sh.opt_allexport, 0);
            return nv;
        }
    }
    a->p = save;
    return ar_ternary(a);
}

int64_t sh_eval_arith(const char *expr, int *err_out) {
    struct arith a = {.p = expr ? expr : "", .err = 0};
    int64_t v = ar_assign(&a);
    ar_ws(&a);
    if (*a.p != '\0') {
        a.err = 1;
    }
    if (err_out) {
        *err_out = a.err;
    }
    return v;
}

/* ---- ANSI-C $'...' quoting ---- */

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void push_utf8(sh_buf_t *out, unsigned int cp) {
    if (cp <= 0x7Fu) {
        (void)sh_buf_push(out, (char)cp);
    } else if (cp <= 0x7FFu) {
        (void)sh_buf_push(out, (char)(0xC0u | (cp >> 6)));
        (void)sh_buf_push(out, (char)(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        (void)sh_buf_push(out, (char)(0xE0u | (cp >> 12)));
        (void)sh_buf_push(out, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        (void)sh_buf_push(out, (char)(0x80u | (cp & 0x3Fu)));
    }
}

static const char *expand_ansi_c(const char *p, sh_buf_t *out) {
    while (*p != '\0' && *p != '\'') {
        if (*p != '\\') {
            (void)sh_buf_push(out, *p++);
            continue;
        }
        p++;
        if (*p == '\0') {
            break;
        }
        switch (*p) {
        case 'a':
            (void)sh_buf_push(out, '\a');
            p++;
            break;
        case 'b':
            (void)sh_buf_push(out, '\b');
            p++;
            break;
        case 'e':
        case 'E':
            (void)sh_buf_push(out, 0x1B);
            p++;
            break;
        case 'f':
            (void)sh_buf_push(out, '\f');
            p++;
            break;
        case 'n':
            (void)sh_buf_push(out, '\n');
            p++;
            break;
        case 'r':
            (void)sh_buf_push(out, '\r');
            p++;
            break;
        case 't':
            (void)sh_buf_push(out, '\t');
            p++;
            break;
        case 'v':
            (void)sh_buf_push(out, '\v');
            p++;
            break;
        case '\\':
        case '\'':
        case '"':
        case '?':
            (void)sh_buf_push(out, *p++);
            break;
        case 'x': {
            p++;
            unsigned val = 0u;
            int d = 0;
            while (d < 2 && hex_val(*p) >= 0) {
                val = (val << 4) | (unsigned)hex_val(*p++);
                d++;
            }
            if (d > 0) {
                (void)sh_buf_push(out, (char)val);
            }
            break;
        }
        case 'u': {
            p++;
            unsigned cp = 0u;
            int d = 0;
            while (d < 4 && hex_val(*p) >= 0) {
                cp = (cp << 4) | (unsigned)hex_val(*p++);
                d++;
            }
            if (d > 0) {
                push_utf8(out, cp);
            }
            break;
        }
        default:
            if (*p >= '0' && *p <= '7') {
                unsigned val = 0u;
                int d = 0;
                while (d < 3 && *p >= '0' && *p <= '7') {
                    val = (val << 3) | (unsigned)(*p++ - '0');
                    d++;
                }
                (void)sh_buf_push(out, (char)(val & 0xFFu));
            } else {
                (void)sh_buf_push(out, '\\');
                (void)sh_buf_push(out, *p++);
            }
            break;
        }
    }
    if (*p == '\'') {
        p++;
    }
    return p;
}

/* ---- Command substitution $(...) and `...` ---- */

static int run_cmdsub(const char *cmd, sh_buf_t *out) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        (void)dup2(pfd[1], STDOUT_FILENO);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        g_sh.interactive = 0;
        g_sh.traps[0] = NULL;
        int rc = sh_eval_string(cmd);
        (void)fflush(stdout);
        (void)fflush(stderr);
        _exit(rc);
    }
    (void)close(pfd[1]);
    sh_buf_t cap = {0};
    char chunk[256];
    for (;;) {
        ssize_t n = read(pfd[0], chunk, sizeof(chunk));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        (void)sh_buf_putn(&cap, chunk, (size_t)n);
    }
    (void)close(pfd[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(st)) {
        g_sh.last_status = WEXITSTATUS(st);
    }
    while (cap.len > 0u && cap.data[cap.len - 1u] == '\n') {
        cap.data[--cap.len] = '\0';
    }
    if (cap.data) {
        (void)sh_buf_putn(out, cap.data, cap.len);
    }
    sh_buf_free(&cap);
    return 0;
}

/* ---- Parameter & pattern stripping helpers ---- */

static char *get_param_value(const char *name, int *is_set) {
    char buf[64];
    *is_set = 1;
    if (strcmp(name, "?") == 0) {
        (void)snprintf(buf, sizeof(buf), "%d", g_sh.last_status);
        return strdup(buf);
    }
    if (strcmp(name, "$") == 0) {
        (void)snprintf(buf, sizeof(buf), "%ld", (long)getpid());
        return strdup(buf);
    }
    if (strcmp(name, "!") == 0) {
        if (g_sh.last_bg_pid <= 0) {
            *is_set = 0;
            return strdup("");
        }
        (void)snprintf(buf, sizeof(buf), "%ld", (long)g_sh.last_bg_pid);
        return strdup(buf);
    }
    if (strcmp(name, "#") == 0) {
        (void)snprintf(buf, sizeof(buf), "%d", g_sh.pos_argc);
        return strdup(buf);
    }
    if (strcmp(name, "0") == 0) {
        return strdup(g_sh.arg0 ? g_sh.arg0 : "sh");
    }
    if (strcmp(name, "-") == 0) {
        size_t pos = 0u;
        if (g_sh.opt_errexit) {
            buf[pos++] = 'e';
        }
        if (g_sh.opt_noglob) {
            buf[pos++] = 'f';
        }
        if (g_sh.opt_nounset) {
            buf[pos++] = 'u';
        }
        if (g_sh.opt_allexport) {
            buf[pos++] = 'a';
        }
        buf[pos] = '\0';
        return strdup(buf);
    }
    if (strcmp(name, "*") == 0 || strcmp(name, "@") == 0) {
        sh_buf_t b = {0};
        const char *ifs = sh_var_get("IFS");
        char sep = (ifs && ifs[0] != '\0') ? ifs[0] : (ifs ? '\0' : ' ');
        for (int i = 0; i < g_sh.pos_argc; i++) {
            if (i > 0 && sep != '\0') {
                (void)sh_buf_push(&b, sep);
            }
            (void)sh_buf_puts(&b, g_sh.pos_args[i]);
        }
        if (g_sh.pos_argc == 0) {
            *is_set = 0;
        }
        return b.data ? b.data : strdup("");
    }
    /* Positional numeric parameter $1..$N. */
    int all_dig = (name[0] != '\0');
    for (size_t i = 0u; name[i] != '\0'; i++) {
        if (!isdigit((unsigned char)name[i])) {
            all_dig = 0;
            break;
        }
    }
    if (all_dig) {
        long idx = strtol(name, NULL, 10);
        if (idx >= 1 && idx <= g_sh.pos_argc) {
            return strdup(g_sh.pos_args[idx - 1]);
        }
        *is_set = 0;
        return strdup("");
    }
    const char *v = sh_var_get(name);
    if (!v) {
        *is_set = 0;
        return strdup("");
    }
    return strdup(v);
}

static char *strip_pattern(const char *val, const char *pat, int from_end,
                           int longest) {
    size_t len = strlen(val);
    if (!from_end) {
        if (longest) {
            for (size_t i = len + 1u; i > 0u; i--) {
                size_t k = i - 1u;
                char *sub = strndup(val, k);
                int m = sub && fnmatch(pat, sub, 0) == 0;
                free(sub);
                if (m) {
                    return strdup(val + k);
                }
            }
        } else {
            for (size_t k = 0u; k <= len; k++) {
                char *sub = strndup(val, k);
                int m = sub && fnmatch(pat, sub, 0) == 0;
                free(sub);
                if (m) {
                    return strdup(val + k);
                }
            }
        }
        return strdup(val);
    }
    if (longest) {
        for (size_t k = 0u; k <= len; k++) {
            if (fnmatch(pat, val + k, 0) == 0) {
                return strndup(val, k);
            }
        }
    } else {
        for (size_t i = len + 1u; i > 0u; i--) {
            size_t k = i - 1u;
            if (fnmatch(pat, val + k, 0) == 0) {
                return strndup(val, k);
            }
        }
    }
    return strdup(val);
}

static int expand_raw_into(const char *raw, sh_buf_t *out, int keep_marks,
                           int heredoc_mode, int *err_out);

static const char *expand_dollar(const char *p, sh_buf_t *out, int in_dquote,
                                 int keep_marks, int *err_out) {
    /* p points past '$'. */
    if (*p == '\'' && !in_dquote) {
        if (keep_marks) {
            (void)sh_buf_push(out, SH_QUOTED_MARK);
        }
        p = expand_ansi_c(p + 1, out);
        if (keep_marks) {
            (void)sh_buf_push(out, SH_QUOTED_MARK);
        }
        return p;
    }
    if (p[0] == '(' && p[1] == '(') {
        p += 2;
        const char *start = p;
        int depth = 2;
        while (*p != '\0' && depth > 0) {
            if (*p == '(') {
                depth++;
            } else if (*p == ')') {
                depth--;
                if (depth == 0) {
                    break;
                }
            }
            p++;
        }
        size_t elen = (p > start) ? (size_t)(p - start - 1) : 0u;
        if (*p == ')') {
            p++;
        }
        char *raw_expr = strndup(start, elen);
        int e2 = 0;
        char *exp_expr = sh_expand_single(raw_expr ? raw_expr : "", &e2);
        free(raw_expr);
        int aerr = 0;
        int64_t val = sh_eval_arith(exp_expr ? exp_expr : "", &aerr);
        free(exp_expr);
        if (aerr) {
            fprintf(stderr, "sh: arithmetic syntax error\n");
            *err_out = 1;
            return p;
        }
        char nbuf[32];
        (void)snprintf(nbuf, sizeof(nbuf), "%lld", (long long)val);
        (void)sh_buf_puts(out, nbuf);
        return p;
    }
    if (p[0] == '(') {
        p++;
        const char *start = p;
        int depth = 1;
        while (*p != '\0' && depth > 0) {
            if (*p == '\'') {
                p++;
                while (*p != '\0' && *p != '\'') {
                    p++;
                }
                if (*p == '\'') {
                    p++;
                }
                continue;
            }
            if (*p == '"') {
                p++;
                while (*p != '\0' && *p != '"') {
                    if (*p == '\\' && p[1] != '\0') {
                        p += 2;
                    } else {
                        p++;
                    }
                }
                if (*p == '"') {
                    p++;
                }
                continue;
            }
            if (*p == '(') {
                depth++;
            } else if (*p == ')') {
                depth--;
                if (depth == 0) {
                    break;
                }
            }
            p++;
        }
        char *cmd = strndup(start, (size_t)(p - start));
        if (*p == ')') {
            p++;
        }
        if (cmd) {
            (void)run_cmdsub(cmd, out);
            free(cmd);
        }
        return p;
    }
    if (*p == '{') {
        p++;
        const char *start = p;
        int depth = 1;
        while (*p != '\0' && depth > 0) {
            if (*p == '\\' && p[1] != '\0') {
                p += 2;
                continue;
            }
            if (*p == '{') {
                depth++;
            } else if (*p == '}') {
                depth--;
                if (depth == 0) {
                    break;
                }
            }
            p++;
        }
        char *inner = strndup(start, (size_t)(p - start));
        if (*p == '}') {
            p++;
        }
        if (!inner) {
            return p;
        }
        if (inner[0] == '#' && inner[1] != '\0' &&
            strpbrk(inner + 1, ":-+=?#%") == NULL) {
            if (strcmp(inner + 1, "@") == 0 || strcmp(inner + 1, "*") == 0) {
                char nbuf[32];
                (void)snprintf(nbuf, sizeof(nbuf), "%d", g_sh.pos_argc);
                (void)sh_buf_puts(out, nbuf);
            } else {
                int is_set = 0;
                char *v = get_param_value(inner + 1, &is_set);
                char nbuf[32];
                (void)snprintf(nbuf, sizeof(nbuf), "%zu", v ? strlen(v) : 0u);
                (void)sh_buf_puts(out, nbuf);
                free(v);
            }
            free(inner);
            return p;
        }

        const char *q = inner;
        char name[64];
        size_t nl = 0u;
        if (*q == '?' || *q == '$' || *q == '!' || *q == '#' || *q == '*' ||
            *q == '@' || *q == '-') {
            name[nl++] = *q++;
        } else if (isdigit((unsigned char)*q)) {
            while (isdigit((unsigned char)*q) && nl + 1u < sizeof(name)) {
                name[nl++] = *q++;
            }
        } else {
            while (sh_is_name_char(*q) && nl + 1u < sizeof(name)) {
                name[nl++] = *q++;
            }
        }
        name[nl] = '\0';

        if (*q == '\0') {
            if (in_dquote && keep_marks && strcmp(name, "@") == 0) {
                for (int i = 0; i < g_sh.pos_argc; i++) {
                    if (i > 0) {
                        (void)sh_buf_push(out, SH_AT_SPLIT);
                    }
                    (void)sh_buf_puts(out, g_sh.pos_args[i]);
                }
            } else {
                int is_set = 0;
                char *v = get_param_value(name, &is_set);
                if (!is_set && g_sh.opt_nounset) {
                    fprintf(stderr, "sh: %s: parameter not set\n", name);
                    *err_out = 1;
                } else {
                    (void)sh_buf_puts(out, v);
                }
                free(v);
            }
            free(inner);
            return p;
        }

        int is_set = 0;
        char *val = get_param_value(name, &is_set);
        int is_empty = (!val || val[0] == '\0');
        int colon = 0;
        if (*q == ':') {
            colon = 1;
            q++;
        }
        char op = *q++;
        int dbl = 0;
        if ((op == '#' || op == '%') && *q == op) {
            dbl = 1;
            q++;
        }
        if (op == '-' || op == '=' || op == '+' || op == '?') {
            int unset_or_null = colon ? (!is_set || is_empty) : !is_set;
            if (op == '+') {
                if (!unset_or_null) {
                    int e2 = 0;
                    char *w = sh_expand_single(q, &e2);
                    (void)sh_buf_puts(out, w);
                    free(w);
                }
            } else if (!unset_or_null) {
                (void)sh_buf_puts(out, val);
            } else if (op == '-') {
                int e2 = 0;
                char *w = sh_expand_single(q, &e2);
                (void)sh_buf_puts(out, w);
                free(w);
            } else if (op == '=') {
                int e2 = 0;
                char *w = sh_expand_single(q, &e2);
                (void)sh_var_set(name, w ? w : "", g_sh.opt_allexport, 0);
                (void)sh_buf_puts(out, w);
                free(w);
            } else {
                int e2 = 0;
                char *w = sh_expand_single(q, &e2);
                fprintf(stderr, "sh: %s: %s\n", name,
                        (w && w[0] != '\0') ? w : "parameter null or not set");
                free(w);
                *err_out = 1;
            }
        } else if (op == '#' || op == '%') {
            int e2 = 0;
            char *pat = sh_expand_single(q, &e2);
            char *res =
                strip_pattern(val ? val : "", pat ? pat : "", op == '%', dbl);
            (void)sh_buf_puts(out, res);
            free(pat);
            free(res);
        }
        free(val);
        free(inner);
        return p;
    }

    if (*p == '@' && in_dquote && keep_marks) {
        for (int i = 0; i < g_sh.pos_argc; i++) {
            if (i > 0) {
                (void)sh_buf_push(out, SH_AT_SPLIT);
            }
            (void)sh_buf_puts(out, g_sh.pos_args[i]);
        }
        return p + 1;
    }
    if (*p == '?' || *p == '$' || *p == '!' || *p == '#' || *p == '*' ||
        *p == '@' || *p == '-' || isdigit((unsigned char)*p)) {
        char name[2] = {*p, '\0'};
        int is_set = 0;
        char *v = get_param_value(name, &is_set);
        (void)sh_buf_puts(out, v);
        free(v);
        return p + 1;
    }
    if (sh_is_name_start(*p)) {
        char name[64];
        size_t nl = 0u;
        while (sh_is_name_char(*p)) {
            if (nl + 1u < sizeof(name)) {
                name[nl++] = *p;
            }
            p++;
        }
        name[nl] = '\0';
        int is_set = 0;
        char *v = get_param_value(name, &is_set);
        if (!is_set && g_sh.opt_nounset) {
            fprintf(stderr, "sh: %s: parameter not set\n", name);
            *err_out = 1;
        } else {
            (void)sh_buf_puts(out, v);
        }
        free(v);
        return p;
    }
    (void)sh_buf_push(out, '$');
    return p;
}

static int expand_raw_into(const char *raw, sh_buf_t *out, int keep_marks,
                           int heredoc_mode, int *err_out) {
    const char *p = raw;
    /* Tilde expansion at start of unquoted word. */
    if (!heredoc_mode && p[0] == '~' && (p[1] == '\0' || p[1] == '/')) {
        const char *home = sh_var_get("HOME");
        if (home && home[0] != '\0') {
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
            (void)sh_buf_puts(out, home);
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
            p++;
        }
    }

    while (*p != '\0' && !*err_out) {
        if (heredoc_mode) {
            if (*p == '\\' && (p[1] == '$' || p[1] == '`' || p[1] == '\\')) {
                p++;
                (void)sh_buf_push(out, *p++);
            } else if (*p == '$') {
                p = expand_dollar(p + 1, out, 1, 0, err_out);
            } else if (*p == '`') {
                p++;
                sh_buf_t cmd = {0};
                while (*p != '\0' && *p != '`') {
                    if (*p == '\\' &&
                        (p[1] == '`' || p[1] == '\\' || p[1] == '$')) {
                        p++;
                    }
                    (void)sh_buf_push(&cmd, *p++);
                }
                if (*p == '`') {
                    p++;
                }
                (void)run_cmdsub(cmd.data ? cmd.data : "", out);
                sh_buf_free(&cmd);
            } else {
                (void)sh_buf_push(out, *p++);
            }
            continue;
        }

        if (*p == '\'') {
            p++;
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
            while (*p != '\0' && *p != '\'') {
                (void)sh_buf_push(out, *p++);
            }
            if (*p == '\'') {
                p++;
            }
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
        } else if (*p == '"') {
            p++;
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
            while (*p != '\0' && *p != '"' && !*err_out) {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$' ||
                                   p[1] == '`')) {
                    p++;
                    (void)sh_buf_push(out, *p++);
                } else if (*p == '$') {
                    p = expand_dollar(p + 1, out, 1, keep_marks, err_out);
                } else if (*p == '`') {
                    p++;
                    sh_buf_t cmd = {0};
                    while (*p != '\0' && *p != '`') {
                        if (*p == '\\' &&
                            (p[1] == '`' || p[1] == '\\' || p[1] == '$')) {
                            p++;
                        }
                        (void)sh_buf_push(&cmd, *p++);
                    }
                    if (*p == '`') {
                        p++;
                    }
                    (void)run_cmdsub(cmd.data ? cmd.data : "", out);
                    sh_buf_free(&cmd);
                } else {
                    (void)sh_buf_push(out, *p++);
                }
            }
            if (*p == '"') {
                p++;
            }
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            }
        } else if (*p == '\\' && p[1] != '\0') {
            p++;
            if (keep_marks) {
                (void)sh_buf_push(out, SH_QUOTED_MARK);
                (void)sh_buf_push(out, *p++);
                (void)sh_buf_push(out, SH_QUOTED_MARK);
            } else {
                (void)sh_buf_push(out, *p++);
            }
        } else if (*p == '$') {
            p = expand_dollar(p + 1, out, 0, keep_marks, err_out);
        } else if (*p == '`') {
            p++;
            sh_buf_t cmd = {0};
            while (*p != '\0' && *p != '`') {
                if (*p == '\\' &&
                    (p[1] == '`' || p[1] == '\\' || p[1] == '$')) {
                    p++;
                }
                (void)sh_buf_push(&cmd, *p++);
            }
            if (*p == '`') {
                p++;
            }
            (void)run_cmdsub(cmd.data ? cmd.data : "", out);
            sh_buf_free(&cmd);
        } else {
            (void)sh_buf_push(out, *p++);
        }
    }
    return *err_out ? -1 : 0;
}

static int has_unquoted_glob(const char *w) {
    int quoted = 0;
    for (size_t i = 0u; w[i] != '\0'; i++) {
        if (w[i] == SH_QUOTED_MARK) {
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
        if (w[i] != SH_QUOTED_MARK && w[i] != SH_AT_SPLIT) {
            out[j++] = w[i];
        }
    }
    out[j] = '\0';
    return out;
}

static char *build_glob_pat(const char *w) {
    size_t len = strlen(w);
    char *out = (char *)malloc(len * 2u + 1u);
    if (!out) {
        return NULL;
    }
    int quoted = 0;
    size_t j = 0u;
    for (size_t i = 0u; i < len && w[i] != '\0'; i++) {
        if (w[i] == SH_QUOTED_MARK) {
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

static void emit_expanded_piece(const char *piece, sh_vec_t *out) {
    if (!g_sh.opt_noglob && has_unquoted_glob(piece)) {
        char *pat = build_glob_pat(piece);
        if (pat) {
            glob_t g = {0};
            if (glob(pat, 0, NULL, &g) == 0 && g.gl_pathc > 0u) {
                for (size_t i = 0u; i < g.gl_pathc; i++) {
                    (void)sh_vec_push(out, strdup(g.gl_pathv[i]));
                }
                globfree(&g);
                free(pat);
                return;
            }
            globfree(&g);
            free(pat);
        }
    }
    char *clean = strip_marks(piece);
    if (clean) {
        (void)sh_vec_push(out, clean);
    }
}

static int is_pure_quoted_at(const char *raw) {
    return strcmp(raw, "\"$@\"") == 0 || strcmp(raw, "\"${@}\"") == 0;
}

int sh_expand_words(const sh_vec_t *raw, sh_vec_t *out) {
    memset(out, 0, sizeof(*out));
    const char *ifs = sh_var_get("IFS");
    if (!ifs) {
        ifs = " \t\n";
    }

    for (size_t i = 0u; i < raw->len; i++) {
        if (is_pure_quoted_at(raw->items[i])) {
            for (int k = 0; k < g_sh.pos_argc; k++) {
                (void)sh_vec_push(out, strdup(g_sh.pos_args[k]));
            }
            continue;
        }
        sh_buf_t b = {0};
        int err = 0;
        if (expand_raw_into(raw->items[i], &b, 1, 0, &err) != 0) {
            sh_buf_free(&b);
            sh_vec_free(out);
            return -1;
        }
        const char *s = b.data ? b.data : "";
        int has_quote = (strchr(s, SH_QUOTED_MARK) != NULL);
        if (s[0] == '\0') {
            sh_buf_free(&b);
            continue;
        }

        /* Split on SH_AT_SPLIT (inside quotes) and unquoted IFS characters. */
        sh_buf_t cur = {0};
        int quoted = 0;
        int has_tok = 0;
        for (size_t k = 0u; s[k] != '\0'; k++) {
            char c = s[k];
            if (c == SH_QUOTED_MARK) {
                quoted = !quoted;
                has_tok = 1;
                (void)sh_buf_push(&cur, c);
                continue;
            }
            if (c == SH_AT_SPLIT) {
                if (quoted) {
                    (void)sh_buf_push(&cur, SH_QUOTED_MARK);
                }
                emit_expanded_piece(cur.data ? cur.data : "", out);
                cur.len = 0u;
                if (cur.data) {
                    cur.data[0] = '\0';
                }
                if (quoted) {
                    (void)sh_buf_push(&cur, SH_QUOTED_MARK);
                }
                has_tok = 1;
                continue;
            }
            if (!quoted && ifs[0] != '\0' && strchr(ifs, c) != NULL) {
                if (has_tok) {
                    emit_expanded_piece(cur.data ? cur.data : "", out);
                    cur.len = 0u;
                    if (cur.data) {
                        cur.data[0] = '\0';
                    }
                    has_tok = 0;
                }
                continue;
            }
            has_tok = 1;
            (void)sh_buf_push(&cur, c);
        }
        if (has_tok || has_quote) {
            emit_expanded_piece(cur.data ? cur.data : "", out);
        }
        sh_buf_free(&cur);
        sh_buf_free(&b);
    }
    return 0;
}

char *sh_expand_single(const char *raw, int *err_out) {
    sh_buf_t b = {0};
    int err = 0;
    (void)expand_raw_into(raw ? raw : "", &b, 0, 0, &err);
    if (err_out) {
        *err_out = err;
    }
    if (err) {
        sh_buf_free(&b);
        return NULL;
    }
    return b.data ? b.data : strdup("");
}

char *sh_expand_heredoc(const char *raw, int *err_out) {
    sh_buf_t b = {0};
    int err = 0;
    (void)expand_raw_into(raw ? raw : "", &b, 0, 1, &err);
    if (err_out) {
        *err_out = err;
    }
    if (err) {
        sh_buf_free(&b);
        return NULL;
    }
    return b.data ? b.data : strdup("");
}
