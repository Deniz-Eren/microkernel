/**
 * @file     m4.c
 * @brief    Ring 3 POSIX.1-2024 XCU m4 macro processor
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0025 */

#define M4_NUM_DIV 10

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sbuf_t;

typedef struct def_node {
    char *body;
    int builtin;
    struct def_node *next;
} def_node_t;

typedef struct {
    char *name;
    def_node_t *top;
    int traced;
} macro_t;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} pback_t;

typedef struct {
    macro_t *macros;
    size_t nmacros;
    size_t cap_macros;
    pback_t pb;
    FILE *in;
    sbuf_t divs[M4_NUM_DIV];
    int cur_div;
    char lquote[32];
    char rquote[32];
    char bcom[32];
    char ecom[32];
    int trace_all;
    int last_sysval;
    int exiting;
    int exit_code;
    char **wraps;
    size_t nwraps;
} m4_t;

enum {
    BI_NONE = 0,
    BI_DEFINE,
    BI_UNDEFINE,
    BI_PUSHDEF,
    BI_POPDEF,
    BI_DEFN,
    BI_IFDEF,
    BI_IFELSE,
    BI_DNL,
    BI_CHANGEQUOTE,
    BI_CHANGECOM,
    BI_DIVERT,
    BI_UNDIVERT,
    BI_DIVNUM,
    BI_EVAL,
    BI_INCR,
    BI_DECR,
    BI_LEN,
    BI_INDEX,
    BI_SUBSTR,
    BI_TRANSLIT,
    BI_SHIFT,
    BI_INCLUDE,
    BI_SINCLUDE,
    BI_SYSCMD,
    BI_SYSVAL,
    BI_MAKETEMP,
    BI_MKSTEMP,
    BI_ERRPRINT,
    BI_DUMPDEF,
    BI_TRACEON,
    BI_TRACEOFF,
    BI_M4WRAP,
    BI_M4EXIT
};

static void sb_append_n(sbuf_t *sb, const char *s, size_t n) {
    if (n == 0) {
        return;
    }
    if (sb->len + n + 1 > sb->cap) {
        size_t nc = (sb->cap == 0) ? 128 : sb->cap * 2;
        while (nc < sb->len + n + 1) {
            nc *= 2;
        }
        char *nb = realloc(sb->data, nc);
        if (!nb) {
            return;
        }
        sb->data = nb;
        sb->cap = nc;
    }
    (void)memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_append(sbuf_t *sb, const char *s) {
    if (s) {
        sb_append_n(sb, s, strlen(s));
    }
}

static void sb_putc(sbuf_t *sb, char c) {
    sb_append_n(sb, &c, 1);
}

static void sb_free(sbuf_t *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void pb_push_char(m4_t *m, char c) {
    if (m->pb.len + 1 > m->pb.cap) {
        size_t nc = (m->pb.cap == 0) ? 256 : m->pb.cap * 2;
        char *nb = realloc(m->pb.buf, nc);
        if (!nb) {
            return;
        }
        m->pb.buf = nb;
        m->pb.cap = nc;
    }
    m->pb.buf[m->pb.len++] = c;
}

static void pb_push_str(m4_t *m, const char *s) {
    if (!s) {
        return;
    }
    size_t n = strlen(s);
    for (size_t i = n; i > 0; i--) {
        pb_push_char(m, s[i - 1]);
    }
}

static int m4_getc(m4_t *m) {
    if (m->pb.len > 0) {
        return (unsigned char)m->pb.buf[--m->pb.len];
    }
    return m->in ? fgetc(m->in) : EOF;
}

static void m4_emit_c(m4_t *m, char c) {
    if (m->cur_div < 0) {
        return;
    }
    if (m->cur_div == 0) {
        (void)putchar((unsigned char)c);
    } else if (m->cur_div < M4_NUM_DIV) {
        sb_putc(&m->divs[m->cur_div], c);
    }
}

static void m4_emit_s(m4_t *m, const char *s) {
    if (!s || m->cur_div < 0) {
        return;
    }
    if (m->cur_div == 0) {
        (void)fputs(s, stdout);
    } else if (m->cur_div < M4_NUM_DIV) {
        sb_append(&m->divs[m->cur_div], s);
    }
}

static macro_t *find_macro(m4_t *m, const char *name, int create) {
    for (size_t i = 0; i < m->nmacros; i++) {
        if (strcmp(m->macros[i].name, name) == 0) {
            return &m->macros[i];
        }
    }
    if (!create) {
        return NULL;
    }
    if (m->nmacros == m->cap_macros) {
        size_t nc = (m->cap_macros == 0) ? 64 : m->cap_macros * 2;
        macro_t *nm = realloc(m->macros, nc * sizeof(*nm));
        if (!nm) {
            return NULL;
        }
        (void)memset(nm + m->cap_macros, 0, (nc - m->cap_macros) * sizeof(*nm));
        m->macros = nm;
        m->cap_macros = nc;
    }
    size_t idx = m->nmacros++;
    m->macros[idx].name = strdup(name);
    return &m->macros[idx];
}

static void def_push(m4_t *m, const char *name, const char *body, int bi) {
    macro_t *mc = find_macro(m, name, 1);
    if (!mc) {
        return;
    }
    def_node_t *nd = calloc(1, sizeof(*nd));
    if (!nd) {
        return;
    }
    nd->body = body ? strdup(body) : NULL;
    nd->builtin = bi;
    nd->next = mc->top;
    mc->top = nd;
}

static void def_replace(m4_t *m, const char *name, const char *body, int bi) {
    macro_t *mc = find_macro(m, name, 1);
    if (!mc) {
        return;
    }
    if (mc->top) {
        free(mc->top->body);
        mc->top->body = body ? strdup(body) : NULL;
        mc->top->builtin = bi;
    } else {
        def_push(m, name, body, bi);
    }
}

static void def_pop(m4_t *m, const char *name) {
    macro_t *mc = find_macro(m, name, 0);
    if (!mc || !mc->top) {
        return;
    }
    def_node_t *old = mc->top;
    mc->top = old->next;
    free(old->body);
    free(old);
}

static void def_undef(m4_t *m, const char *name) {
    macro_t *mc = find_macro(m, name, 0);
    if (!mc) {
        return;
    }
    while (mc->top) {
        def_pop(m, name);
    }
}

static int match_seq(m4_t *m, int first_ch, const char *seq) {
    size_t slen = strlen(seq);
    if (slen == 0 || first_ch != (unsigned char)seq[0]) {
        return 0;
    }
    if (slen == 1) {
        return 1;
    }
    char got[32];
    size_t n = 0;
    for (size_t i = 1; i < slen; i++) {
        int c = m4_getc(m);
        if (c == EOF) {
            for (size_t k = n; k > 0; k--) {
                pb_push_char(m, got[k - 1]);
            }
            return 0;
        }
        got[n++] = (char)c;
        if (c != (unsigned char)seq[i]) {
            for (size_t k = n; k > 0; k--) {
                pb_push_char(m, got[k - 1]);
            }
            return 0;
        }
    }
    return 1;
}

/* eval() integer expression parser */
typedef struct {
    const char *s;
    size_t pos;
    int err;
} ev_t;

static void ev_ws(ev_t *e) {
    while (e->s[e->pos] == ' ' || e->s[e->pos] == '\t' ||
           e->s[e->pos] == '\n' || e->s[e->pos] == '\r') {
        e->pos++;
    }
}

static int64_t ev_or(ev_t *e);

static int64_t ev_primary(ev_t *e) {
    ev_ws(e);
    if (e->s[e->pos] == '(') {
        e->pos++;
        int64_t v = ev_or(e);
        ev_ws(e);
        if (e->s[e->pos] == ')') {
            e->pos++;
        } else {
            e->err = 1;
        }
        return v;
    }
    char *end = NULL;
    int64_t v = strtoll(e->s + e->pos, &end, 0);
    if (end == e->s + e->pos) {
        e->err = 1;
        return 0;
    }
    e->pos = (size_t)(end - e->s);
    return v;
}

static int64_t ev_unary(ev_t *e) {
    ev_ws(e);
    char c = e->s[e->pos];
    if (c == '+') {
        e->pos++;
        return ev_unary(e);
    }
    if (c == '-') {
        e->pos++;
        return -ev_unary(e);
    }
    if (c == '~') {
        e->pos++;
        return ~ev_unary(e);
    }
    if (c == '!' && e->s[e->pos + 1] != '=') {
        e->pos++;
        return !ev_unary(e);
    }
    return ev_primary(e);
}

static int64_t ev_pow(ev_t *e) {
    int64_t a = ev_unary(e);
    ev_ws(e);
    if (e->s[e->pos] == '*' && e->s[e->pos + 1] == '*') {
        e->pos += 2;
        int64_t b = ev_pow(e);
        if (b < 0) {
            return 0;
        }
        int64_t r = 1;
        for (int64_t i = 0; i < b; i++) {
            r *= a;
        }
        return r;
    }
    return a;
}

static int64_t ev_mul(ev_t *e) {
    int64_t a = ev_pow(e);
    for (;;) {
        ev_ws(e);
        char c = e->s[e->pos];
        if ((c == '*' && e->s[e->pos + 1] != '*') || c == '/' || c == '%') {
            e->pos++;
            int64_t b = ev_pow(e);
            if (c == '*') {
                a *= b;
            } else if (b == 0) {
                e->err = 1;
                a = 0;
            } else if (c == '/') {
                a /= b;
            } else {
                a %= b;
            }
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_add(ev_t *e) {
    int64_t a = ev_mul(e);
    for (;;) {
        ev_ws(e);
        char c = e->s[e->pos];
        if (c == '+' || c == '-') {
            e->pos++;
            int64_t b = ev_mul(e);
            a = (c == '+') ? (a + b) : (a - b);
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_shift(ev_t *e) {
    int64_t a = ev_add(e);
    for (;;) {
        ev_ws(e);
        if (e->s[e->pos] == '<' && e->s[e->pos + 1] == '<') {
            e->pos += 2;
            int64_t b = ev_add(e);
            a = (int64_t)((uint64_t)a << (unsigned int)(b & 63));
        } else if (e->s[e->pos] == '>' && e->s[e->pos + 1] == '>') {
            e->pos += 2;
            int64_t b = ev_add(e);
            a >>= (b & 63);
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_rel(ev_t *e) {
    int64_t a = ev_shift(e);
    for (;;) {
        ev_ws(e);
        const char *p = e->s + e->pos;
        if (p[0] == '<' && p[1] == '=') {
            e->pos += 2;
            a = (a <= ev_shift(e));
        } else if (p[0] == '>' && p[1] == '=') {
            e->pos += 2;
            a = (a >= ev_shift(e));
        } else if (p[0] == '<') {
            e->pos++;
            a = (a < ev_shift(e));
        } else if (p[0] == '>') {
            e->pos++;
            a = (a > ev_shift(e));
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_eq(ev_t *e) {
    int64_t a = ev_rel(e);
    for (;;) {
        ev_ws(e);
        const char *p = e->s + e->pos;
        if (p[0] == '=' && p[1] == '=') {
            e->pos += 2;
            a = (a == ev_rel(e));
        } else if (p[0] == '!' && p[1] == '=') {
            e->pos += 2;
            a = (a != ev_rel(e));
        } else if (p[0] == '=') {
            e->pos++;
            a = (a == ev_rel(e));
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_band(ev_t *e) {
    int64_t a = ev_eq(e);
    while (e->s[e->pos] == '&' && e->s[e->pos + 1] != '&') {
        e->pos++;
        a &= ev_eq(e);
        ev_ws(e);
    }
    return a;
}

static int64_t ev_bxor(ev_t *e) {
    int64_t a = ev_band(e);
    for (;;) {
        ev_ws(e);
        if (e->s[e->pos] == '^') {
            e->pos++;
            a ^= ev_band(e);
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_bor(ev_t *e) {
    int64_t a = ev_bxor(e);
    for (;;) {
        ev_ws(e);
        if (e->s[e->pos] == '|' && e->s[e->pos + 1] != '|') {
            e->pos++;
            a |= ev_bxor(e);
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_land(ev_t *e) {
    int64_t a = ev_bor(e);
    for (;;) {
        ev_ws(e);
        if (e->s[e->pos] == '&' && e->s[e->pos + 1] == '&') {
            e->pos += 2;
            int64_t b = ev_bor(e);
            a = (a && b);
        } else {
            break;
        }
    }
    return a;
}

static int64_t ev_or(ev_t *e) {
    int64_t a = ev_land(e);
    for (;;) {
        ev_ws(e);
        if (e->s[e->pos] == '|' && e->s[e->pos + 1] == '|') {
            e->pos += 2;
            int64_t b = ev_land(e);
            a = (a || b);
        } else {
            break;
        }
    }
    return a;
}

static char *format_radix(int64_t v, int radix, int minw) {
    if (radix < 2 || radix > 36) {
        radix = 10;
    }
    int neg = (v < 0);
    uint64_t uv = neg ? (uint64_t)(-(v + 1)) + 1ull : (uint64_t)v;
    char tmp[96];
    size_t n = 0;
    static const char digs[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    do {
        tmp[n++] = digs[uv % (uint64_t)radix];
        uv /= (uint64_t)radix;
    } while (uv > 0 && n < sizeof(tmp) - 2);
    while ((int)n < minw && n < sizeof(tmp) - 2) {
        tmp[n++] = '0';
    }
    if (neg) {
        tmp[n++] = '-';
    }
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = tmp[n - 1 - i];
    }
    out[n] = '\0';
    return out;
}

static void invoke_macro(m4_t *m, macro_t *mc, char **args, size_t nargs);

static void expand_quoted(m4_t *m, sbuf_t *dst) {
    int depth = 1;
    int ch = 0;
    while ((ch = m4_getc(m)) != EOF) {
        if (m->rquote[0] != '\0' && match_seq(m, ch, m->rquote)) {
            depth--;
            if (depth == 0) {
                return;
            }
            if (dst) {
                sb_append(dst, m->rquote);
            } else {
                m4_emit_s(m, m->rquote);
            }
            continue;
        }
        if (m->lquote[0] != '\0' && match_seq(m, ch, m->lquote)) {
            depth++;
            if (dst) {
                sb_append(dst, m->lquote);
            } else {
                m4_emit_s(m, m->lquote);
            }
            continue;
        }
        if (dst) {
            sb_putc(dst, (char)ch);
        } else {
            m4_emit_c(m, (char)ch);
        }
    }
}

static void process_stream(m4_t *m, sbuf_t *arg_dst, int stop_paren);

static void collect_and_invoke(m4_t *m, macro_t *mc) {
    char **args = NULL;
    size_t nargs = 0;
    size_t cap = 0;

    int ch = m4_getc(m);
    if (ch == '(') {
        for (;;) {
            /* Strip leading unquoted whitespace per POSIX m4 */
            do {
                ch = m4_getc(m);
            } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
            if (ch != EOF) {
                pb_push_char(m, (char)ch);
            }
            sbuf_t cur = {0};
            process_stream(m, &cur, 1);
            if (nargs == cap) {
                size_t nc = (cap == 0) ? 8 : cap * 2;
                char **na = realloc(args, nc * sizeof(*na));
                if (!na) {
                    sb_free(&cur);
                    break;
                }
                args = na;
                cap = nc;
            }
            args[nargs++] = cur.data ? cur.data : strdup("");
            int sep = m4_getc(m);
            if (sep == ')') {
                break;
            }
            if (sep != ',') {
                break;
            }
        }
    } else if (ch != EOF) {
        pb_push_char(m, (char)ch);
    }

    invoke_macro(m, mc, args, nargs);
    for (size_t i = 0; i < nargs; i++) {
        free(args[i]);
    }
    free(args);
}

static void invoke_builtin(m4_t *m, int bi, char **args, size_t nargs) {
    const char *a0 = (nargs > 0) ? args[0] : "";
    const char *a1 = (nargs > 1) ? args[1] : "";
    const char *a2 = (nargs > 2) ? args[2] : "";

    switch (bi) {
    case BI_DEFINE:
        if (nargs >= 1) {
            def_replace(m, a0, a1, BI_NONE);
        }
        break;
    case BI_PUSHDEF:
        if (nargs >= 1) {
            def_push(m, a0, a1, BI_NONE);
        }
        break;
    case BI_UNDEFINE:
        for (size_t i = 0; i < nargs; i++) {
            def_undef(m, args[i]);
        }
        break;
    case BI_POPDEF:
        for (size_t i = 0; i < nargs; i++) {
            def_pop(m, args[i]);
        }
        break;
    case BI_DEFN: {
        sbuf_t out = {0};
        for (size_t i = 0; i < nargs; i++) {
            macro_t *t = find_macro(m, args[i], 0);
            if (t && t->top && t->top->builtin == BI_NONE && t->top->body) {
                sb_append(&out, m->lquote);
                sb_append(&out, t->top->body);
                sb_append(&out, m->rquote);
            }
        }
        pb_push_str(m, out.data);
        sb_free(&out);
        break;
    }
    case BI_IFDEF: {
        macro_t *t = find_macro(m, a0, 0);
        if (t && t->top) {
            pb_push_str(m, a1);
        } else if (nargs >= 3) {
            pb_push_str(m, a2);
        }
        break;
    }
    case BI_IFELSE:
        if (nargs < 3) {
            break;
        }
        for (size_t i = 0; i + 2 < nargs; i += 3) {
            if (strcmp(args[i], args[i + 1]) == 0) {
                pb_push_str(m, args[i + 2]);
                return;
            }
            if (i + 3 == nargs - 1) {
                pb_push_str(m, args[i + 3]);
                return;
            }
        }
        break;
    case BI_DNL: {
        int ch = 0;
        while ((ch = m4_getc(m)) != EOF && ch != '\n') {
        }
        break;
    }
    case BI_CHANGEQUOTE:
        if (nargs == 0) {
            (void)snprintf(m->lquote, sizeof(m->lquote), "`");
            (void)snprintf(m->rquote, sizeof(m->rquote), "'");
        } else if (nargs == 1) {
            (void)snprintf(m->lquote, sizeof(m->lquote), "%s", a0);
            (void)snprintf(m->rquote, sizeof(m->rquote), "'");
        } else {
            (void)snprintf(m->lquote, sizeof(m->lquote), "%s", a0);
            (void)snprintf(m->rquote, sizeof(m->rquote), "%s", a1);
        }
        break;
    case BI_CHANGECOM:
        if (nargs == 0) {
            m->bcom[0] = '\0';
            m->ecom[0] = '\0';
        } else if (nargs == 1) {
            (void)snprintf(m->bcom, sizeof(m->bcom), "%s", a0);
            (void)snprintf(m->ecom, sizeof(m->ecom), "\n");
        } else {
            (void)snprintf(m->bcom, sizeof(m->bcom), "%s", a0);
            (void)snprintf(m->ecom, sizeof(m->ecom), "%s", a1);
        }
        break;
    case BI_DIVERT:
        m->cur_div = (nargs >= 1 && a0[0] != '\0') ? atoi(a0) : 0;
        break;
    case BI_UNDIVERT:
        if (nargs == 0) {
            for (int d = 1; d < M4_NUM_DIV; d++) {
                if (d != m->cur_div && m->divs[d].len > 0) {
                    m4_emit_s(m, m->divs[d].data);
                    sb_free(&m->divs[d]);
                }
            }
        } else {
            for (size_t i = 0; i < nargs; i++) {
                char *end = NULL;
                long d = strtol(args[i], &end, 10);
                if (end && *end == '\0' && end != args[i]) {
                    if (d >= 1 && d < M4_NUM_DIV && d != m->cur_div &&
                        m->divs[d].len > 0) {
                        m4_emit_s(m, m->divs[d].data);
                        sb_free(&m->divs[d]);
                    }
                } else {
                    FILE *fp = fopen(args[i], "rb");
                    if (fp) {
                        int ch = 0;
                        while ((ch = fgetc(fp)) != EOF) {
                            m4_emit_c(m, (char)ch);
                        }
                        (void)fclose(fp);
                    }
                }
            }
        }
        break;
    case BI_DIVNUM: {
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%d", m->cur_div);
        pb_push_str(m, buf);
        break;
    }
    case BI_EVAL: {
        ev_t ev = {.s = a0, .pos = 0, .err = 0};
        int64_t val = ev_or(&ev);
        if (!ev.err) {
            int radix = (nargs >= 2 && a1[0] != '\0') ? atoi(a1) : 10;
            int minw = (nargs >= 3 && a2[0] != '\0') ? atoi(a2) : 1;
            char *s = format_radix(val, radix, minw);
            if (s) {
                pb_push_str(m, s);
                free(s);
            }
        }
        break;
    }
    case BI_INCR:
    case BI_DECR: {
        long long v = strtoll(a0, NULL, 10);
        v += (bi == BI_INCR) ? 1 : -1;
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%lld", v);
        pb_push_str(m, buf);
        break;
    }
    case BI_LEN: {
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%zu", strlen(a0));
        pb_push_str(m, buf);
        break;
    }
    case BI_INDEX: {
        const char *f = strstr(a0, a1);
        long pos = f ? (long)(f - a0) : -1;
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%ld", pos);
        pb_push_str(m, buf);
        break;
    }
    case BI_SUBSTR: {
        size_t slen = strlen(a0);
        long start = strtol(a1, NULL, 10);
        if (start < 0 || (size_t)start >= slen) {
            break;
        }
        size_t take = slen - (size_t)start;
        if (nargs >= 3 && a2[0] != '\0') {
            long n = strtol(a2, NULL, 10);
            if (n <= 0) {
                take = 0;
            } else if ((size_t)n < take) {
                take = (size_t)n;
            }
        }
        char *sub = malloc(take + 1);
        if (sub) {
            if (take > 0) {
                (void)memcpy(sub, a0 + start, take);
            }
            sub[take] = '\0';
            pb_push_str(m, sub);
            free(sub);
        }
        break;
    }
    case BI_TRANSLIT: {
        sbuf_t out = {0};
        size_t tolen = strlen(a2);
        for (const char *p = a0; *p != '\0'; p++) {
            const char *f = strchr(a1, *p);
            if (!f) {
                sb_putc(&out, *p);
            } else {
                size_t idx = (size_t)(f - a1);
                if (idx < tolen) {
                    sb_putc(&out, a2[idx]);
                }
            }
        }
        pb_push_str(m, out.data);
        sb_free(&out);
        break;
    }
    case BI_SHIFT: {
        sbuf_t out = {0};
        for (size_t i = 1; i < nargs; i++) {
            if (i > 1) {
                sb_putc(&out, ',');
            }
            sb_append(&out, m->lquote);
            sb_append(&out, args[i]);
            sb_append(&out, m->rquote);
        }
        pb_push_str(m, out.data);
        sb_free(&out);
        break;
    }
    case BI_INCLUDE:
    case BI_SINCLUDE: {
        FILE *fp = fopen(a0, "rb");
        if (!fp) {
            if (bi == BI_INCLUDE) {
                (void)fprintf(stderr, "m4: cannot open %s\n", a0);
                m->exit_code = 1;
            }
            break;
        }
        sbuf_t fbuf = {0};
        int ch = 0;
        while ((ch = fgetc(fp)) != EOF) {
            sb_putc(&fbuf, (char)ch);
        }
        (void)fclose(fp);
        pb_push_str(m, fbuf.data);
        sb_free(&fbuf);
        break;
    }
    case BI_SYSCMD:
        (void)fflush(stdout);
        m->last_sysval = system(a0);
        break;
    case BI_SYSVAL: {
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "%d", m->last_sysval);
        pb_push_str(m, buf);
        break;
    }
    case BI_MAKETEMP:
    case BI_MKSTEMP: {
        char *tpl = strdup(a0);
        if (tpl) {
            int fd = mkstemp(tpl);
            if (fd >= 0) {
                (void)close(fd);
            }
            pb_push_str(m, tpl);
            free(tpl);
        }
        break;
    }
    case BI_ERRPRINT:
        for (size_t i = 0; i < nargs; i++) {
            if (i > 0) {
                (void)fputc(' ', stderr);
            }
            (void)fputs(args[i], stderr);
        }
        break;
    case BI_DUMPDEF:
        if (nargs == 0) {
            for (size_t i = 0; i < m->nmacros; i++) {
                if (m->macros[i].top) {
                    (void)fprintf(stderr, "%s:\t%s\n", m->macros[i].name,
                                  m->macros[i].top->body
                                      ? m->macros[i].top->body
                                      : "<builtin>");
                }
            }
        } else {
            for (size_t i = 0; i < nargs; i++) {
                macro_t *t = find_macro(m, args[i], 0);
                if (t && t->top) {
                    (void)fprintf(stderr, "%s:\t%s\n", t->name,
                                  t->top->body ? t->top->body : "<builtin>");
                }
            }
        }
        break;
    case BI_TRACEON:
        if (nargs == 0) {
            m->trace_all = 1;
        } else {
            for (size_t i = 0; i < nargs; i++) {
                macro_t *t = find_macro(m, args[i], 1);
                if (t) {
                    t->traced = 1;
                }
            }
        }
        break;
    case BI_TRACEOFF:
        if (nargs == 0) {
            m->trace_all = 0;
            for (size_t i = 0; i < m->nmacros; i++) {
                m->macros[i].traced = 0;
            }
        } else {
            for (size_t i = 0; i < nargs; i++) {
                macro_t *t = find_macro(m, args[i], 0);
                if (t) {
                    t->traced = 0;
                }
            }
        }
        break;
    case BI_M4WRAP:
        if (nargs >= 1) {
            char **nw = realloc(m->wraps, (m->nwraps + 1) * sizeof(*nw));
            if (nw) {
                m->wraps = nw;
                m->wraps[m->nwraps++] = strdup(a0);
            }
        }
        break;
    case BI_M4EXIT:
        m->exit_code = (nargs >= 1 && a0[0] != '\0') ? atoi(a0) : 0;
        m->exiting = 1;
        break;
    default:
        break;
    }
}

static void invoke_macro(m4_t *m, macro_t *mc, char **args, size_t nargs) {
    if (!mc || !mc->top) {
        return;
    }
    if (mc->top->builtin != BI_NONE) {
        invoke_builtin(m, mc->top->builtin, args, nargs);
        return;
    }
    const char *body = mc->top->body ? mc->top->body : "";
    sbuf_t out = {0};
    for (const char *p = body; *p != '\0'; p++) {
        if (*p == '$' && p[1] != '\0') {
            char c = p[1];
            if (c == '0') {
                sb_append(&out, mc->name);
                p++;
                continue;
            }
            if (c >= '1' && c <= '9') {
                size_t idx = (size_t)(c - '1');
                if (idx < nargs) {
                    sb_append(&out, args[idx]);
                }
                p++;
                continue;
            }
            if (c == '#') {
                char nb[32];
                (void)snprintf(nb, sizeof(nb), "%zu", nargs);
                sb_append(&out, nb);
                p++;
                continue;
            }
            if (c == '*' || c == '@') {
                for (size_t i = 0; i < nargs; i++) {
                    if (i > 0) {
                        sb_putc(&out, ',');
                    }
                    if (c == '@') {
                        sb_append(&out, m->lquote);
                    }
                    sb_append(&out, args[i]);
                    if (c == '@') {
                        sb_append(&out, m->rquote);
                    }
                }
                p++;
                continue;
            }
        }
        sb_putc(&out, *p);
    }
    pb_push_str(m, out.data);
    sb_free(&out);
}

static void process_stream(m4_t *m, sbuf_t *arg_dst, int stop_paren) {
    int paren = 0;
    int ch = 0;
    while (!m->exiting && (ch = m4_getc(m)) != EOF) {
        if (stop_paren) {
            if (ch == '(') {
                paren++;
                sb_putc(arg_dst, '(');
                continue;
            }
            if (paren == 0 && (ch == ',' || ch == ')')) {
                pb_push_char(m, (char)ch);
                return;
            }
            if (ch == ')' && paren > 0) {
                paren--;
                sb_putc(arg_dst, ')');
                continue;
            }
        }
        if (m->lquote[0] != '\0' && match_seq(m, ch, m->lquote)) {
            expand_quoted(m, arg_dst);
            continue;
        }
        if (m->bcom[0] != '\0' && match_seq(m, ch, m->bcom)) {
            if (arg_dst) {
                sb_append(arg_dst, m->bcom);
            } else {
                m4_emit_s(m, m->bcom);
            }
            while ((ch = m4_getc(m)) != EOF) {
                if (m->ecom[0] != '\0' && match_seq(m, ch, m->ecom)) {
                    if (arg_dst) {
                        sb_append(arg_dst, m->ecom);
                    } else {
                        m4_emit_s(m, m->ecom);
                    }
                    break;
                }
                if (arg_dst) {
                    sb_putc(arg_dst, (char)ch);
                } else {
                    m4_emit_c(m, (char)ch);
                }
            }
            continue;
        }
        if (isalpha(ch) || ch == '_') {
            sbuf_t id = {0};
            sb_putc(&id, (char)ch);
            while ((ch = m4_getc(m)) != EOF && (isalnum(ch) || ch == '_')) {
                sb_putc(&id, (char)ch);
            }
            if (ch != EOF) {
                pb_push_char(m, (char)ch);
            }
            macro_t *mc = find_macro(m, id.data ? id.data : "", 0);
            if (mc && mc->top) {
                sb_free(&id);
                collect_and_invoke(m, mc);
            } else {
                if (arg_dst) {
                    sb_append(arg_dst, id.data);
                } else {
                    m4_emit_s(m, id.data);
                }
                sb_free(&id);
            }
            continue;
        }
        if (arg_dst) {
            sb_putc(arg_dst, (char)ch);
        } else {
            m4_emit_c(m, (char)ch);
        }
    }
}

static void m4_init_builtins(m4_t *m) {
    static const struct {
        const char *name;
        int bi;
    } table[] = {
        {"define", BI_DEFINE},
        {"undefine", BI_UNDEFINE},
        {"pushdef", BI_PUSHDEF},
        {"popdef", BI_POPDEF},
        {"defn", BI_DEFN},
        {"ifdef", BI_IFDEF},
        {"ifelse", BI_IFELSE},
        {"dnl", BI_DNL},
        {"changequote", BI_CHANGEQUOTE},
        {"changecom", BI_CHANGECOM},
        {"divert", BI_DIVERT},
        {"undivert", BI_UNDIVERT},
        {"divnum", BI_DIVNUM},
        {"eval", BI_EVAL},
        {"incr", BI_INCR},
        {"decr", BI_DECR},
        {"len", BI_LEN},
        {"index", BI_INDEX},
        {"substr", BI_SUBSTR},
        {"translit", BI_TRANSLIT},
        {"shift", BI_SHIFT},
        {"include", BI_INCLUDE},
        {"sinclude", BI_SINCLUDE},
        {"syscmd", BI_SYSCMD},
        {"sysval", BI_SYSVAL},
        {"maketemp", BI_MAKETEMP},
        {"mkstemp", BI_MKSTEMP},
        {"errprint", BI_ERRPRINT},
        {"dumpdef", BI_DUMPDEF},
        {"traceon", BI_TRACEON},
        {"traceoff", BI_TRACEOFF},
        {"m4wrap", BI_M4WRAP},
        {"m4exit", BI_M4EXIT},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        def_push(m, table[i].name, NULL, table[i].bi);
    }
}

static void m4_free(m4_t *m) {
    for (size_t i = 0; i < m->nmacros; i++) {
        while (m->macros[i].top) {
            def_pop(m, m->macros[i].name);
        }
        free(m->macros[i].name);
    }
    free(m->macros);
    free(m->pb.buf);
    for (int d = 0; d < M4_NUM_DIV; d++) {
        sb_free(&m->divs[d]);
    }
    for (size_t i = 0; i < m->nwraps; i++) {
        free(m->wraps[i]);
    }
    free(m->wraps);
}

int main(int argc, char **argv) {
    m4_t m = {0};
    (void)snprintf(m.lquote, sizeof(m.lquote), "`");
    (void)snprintf(m.rquote, sizeof(m.rquote), "'");
    (void)snprintf(m.bcom, sizeof(m.bcom), "#");
    (void)snprintf(m.ecom, sizeof(m.ecom), "\n");
    m4_init_builtins(&m);

    int opt = 0;
    while ((opt = getopt(argc, argv, "sD:U:")) != -1) {
        switch (opt) {
        case 's':
            break;
        case 'D': {
            const char *eq = strchr(optarg, '=');
            if (eq) {
                size_t nl = (size_t)(eq - optarg);
                char *nm = malloc(nl + 1);
                if (nm) {
                    (void)memcpy(nm, optarg, nl);
                    nm[nl] = '\0';
                    def_replace(&m, nm, eq + 1, BI_NONE);
                    free(nm);
                }
            } else {
                def_replace(&m, optarg, "", BI_NONE);
            }
            break;
        }
        case 'U':
            def_undef(&m, optarg);
            break;
        default:
            (void)fprintf(stderr, "usage: m4 [-s] [-D name[=val]] [-U name] "
                                  "[file...]\n");
            m4_free(&m);
            return 1;
        }
    }

    if (optind >= argc) {
        m.in = stdin;
        process_stream(&m, NULL, 0);
    } else {
        for (int i = optind; i < argc && !m.exiting; i++) {
            FILE *fp =
                (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "rb");
            if (!fp) {
                perror(argv[i]);
                m.exit_code = 1;
                continue;
            }
            m.in = fp;
            process_stream(&m, NULL, 0);
            if (fp != stdin) {
                (void)fclose(fp);
            }
        }
    }

    /* Run m4wrap text in LIFO order */
    m.in = NULL;
    while (!m.exiting && m.nwraps > 0) {
        char *w = m.wraps[--m.nwraps];
        pb_push_str(&m, w);
        free(w);
        process_stream(&m, NULL, 0);
    }

    /* Flush remaining diversions 1..9 */
    if (!m.exiting) {
        m.cur_div = 0;
        for (int d = 1; d < M4_NUM_DIV; d++) {
            if (m.divs[d].len > 0) {
                (void)fputs(m.divs[d].data, stdout);
            }
        }
    }

    int rc = m.exit_code;
    m4_free(&m);
    return rc;
}
