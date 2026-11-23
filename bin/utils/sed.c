/**
 * @file     sed.c
 * @brief    Ring 3 sed stream editor targeting POSIX.1-2024 XCU over <regex.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0018 */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sbuf_t;

static int sb_push(sbuf_t *b, char c) {
    if (b->len + 2u > b->cap) {
        size_t ncap = b->cap ? b->cap * 2u : 64u;
        char *nd = (char *)realloc(b->data, ncap);
        if (!nd) {
            return -1;
        }
        memset(nd + b->cap, 0, ncap - b->cap);
        b->data = nd;
        b->cap = ncap;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return 0;
}

static int sb_putn(sbuf_t *b, const char *s, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (sb_push(b, s[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int sb_puts(sbuf_t *b, const char *s) {
    return s ? sb_putn(b, s, strlen(s)) : 0;
}

static void sb_set(sbuf_t *b, const char *s) {
    b->len = 0u;
    if (b->data) {
        b->data[0] = '\0';
    }
    (void)sb_puts(b, s);
}

enum addr_kind {
    ADDR_NONE = 0,
    ADDR_LINE,
    ADDR_LAST,
    ADDR_REGEX,
};

struct sed_addr {
    enum addr_kind kind;
    unsigned long line;
    regex_t re;
};

struct sed_cmd {
    struct sed_addr a1;
    struct sed_addr a2;
    int in_range;
    int negated;
    char op;
    regex_t sub_re;
    int has_sub_re;
    char *repl;
    int sub_global;
    int sub_nth;
    int sub_print;
    char *text; /* for a/i/c/b/t/:/y-src */
    char *dst;  /* for y-dst */
    size_t block_end;
};

struct sed_prog {
    struct sed_cmd *cmds;
    size_t len;
    size_t cap;
    int ere;
    int suppress;
};

static int prog_push(struct sed_prog *pr, struct sed_cmd c) {
    if (pr->len == pr->cap) {
        size_t ncap = pr->cap ? pr->cap * 2u : 32u;
        struct sed_cmd *nc =
            (struct sed_cmd *)reallocarray(pr->cmds, ncap, sizeof(*nc));
        if (!nc) {
            return -1;
        }
        pr->cmds = nc;
        pr->cap = ncap;
    }
    pr->cmds[pr->len++] = c;
    return 0;
}

static int parse_delimited(const char **pp, char delim, sbuf_t *out,
                           int is_regex) {
    const char *p = *pp;
    while (*p != '\0' && *p != delim) {
        if (*p == '\\' && p[1] != '\0') {
            if (p[1] == delim) {
                (void)sb_push(out, delim);
                p += 2;
                continue;
            }
            if (p[1] == 'n') {
                (void)sb_push(out, '\n');
                p += 2;
                continue;
            }
            if (is_regex) {
                (void)sb_push(out, '\\');
                (void)sb_push(out, p[1]);
                p += 2;
                continue;
            }
            (void)sb_push(out, '\\');
            (void)sb_push(out, p[1]);
            p += 2;
            continue;
        }
        (void)sb_push(out, *p++);
    }
    if (*p != delim) {
        return -1;
    }
    *pp = p + 1;
    return 0;
}

static int parse_addr(const char **pp, struct sed_addr *a, int ere) {
    const char *p = *pp;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (isdigit((unsigned char)*p)) {
        a->kind = ADDR_LINE;
        char *end = NULL;
        a->line = strtoul(p, &end, 10);
        *pp = end;
        return 1;
    }
    if (*p == '$') {
        a->kind = ADDR_LAST;
        *pp = p + 1;
        return 1;
    }
    if (*p == '/' || (*p == '\\' && p[1] != '\0')) {
        char delim = (*p == '/') ? *p++ : (p++, *p++);
        sbuf_t pat = {0};
        if (parse_delimited(&p, delim, &pat, 1) != 0) {
            free(pat.data);
            return -1;
        }
        int rc =
            regcomp(&a->re, pat.data ? pat.data : "", ere ? REG_EXTENDED : 0);
        free(pat.data);
        if (rc != 0) {
            return -1;
        }
        a->kind = ADDR_REGEX;
        *pp = p;
        return 1;
    }
    *pp = p;
    return 0;
}

static int parse_script(struct sed_prog *pr, const char *script) {
    const char *p = script;
    size_t block_stack[16];
    size_t bdepth = 0u;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == ';' || *p == '\n' ||
               *p == '\r') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == '#') {
            if (p == script && p[1] == 'n') {
                pr->suppress = 1;
            }
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }
        if (*p == '}') {
            p++;
            if (bdepth == 0u) {
                fprintf(stderr, "sed: unmatched '}'\n");
                return -1;
            }
            pr->cmds[block_stack[--bdepth]].block_end = pr->len;
            continue;
        }

        struct sed_cmd cmd;
        memset(&cmd, 0, sizeof(cmd));
        int r1 = parse_addr(&p, &cmd.a1, pr->ere);
        if (r1 < 0) {
            fprintf(stderr, "sed: invalid address\n");
            return -1;
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (r1 > 0 && *p == ',') {
            p++;
            if (parse_addr(&p, &cmd.a2, pr->ere) <= 0) {
                fprintf(stderr, "sed: invalid second address\n");
                return -1;
            }
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '!') {
            cmd.negated = 1;
            p++;
            while (*p == ' ' || *p == '\t') {
                p++;
            }
        }
        if (*p == '\0') {
            break;
        }
        cmd.op = *p++;

        if (cmd.op == '{') {
            if (bdepth >= 16u || prog_push(pr, cmd) != 0) {
                return -1;
            }
            block_stack[bdepth++] = pr->len - 1u;
            continue;
        }
        if (cmd.op == 's') {
            char delim = *p++;
            if (delim == '\0' || delim == '\n') {
                return -1;
            }
            sbuf_t pat = {0};
            sbuf_t rep = {0};
            if (parse_delimited(&p, delim, &pat, 1) != 0 ||
                parse_delimited(&p, delim, &rep, 0) != 0) {
                free(pat.data);
                free(rep.data);
                fprintf(stderr, "sed: unterminated s command\n");
                return -1;
            }
            if (regcomp(&cmd.sub_re, pat.data ? pat.data : "",
                        pr->ere ? REG_EXTENDED : 0) != 0) {
                free(pat.data);
                free(rep.data);
                fprintf(stderr, "sed: invalid regex in s command\n");
                return -1;
            }
            free(pat.data);
            cmd.has_sub_re = 1;
            cmd.repl = rep.data ? rep.data : strdup("");
            while (*p != '\0' && *p != ';' && *p != '\n' && *p != '}') {
                if (*p == 'g') {
                    cmd.sub_global = 1;
                } else if (*p == 'p') {
                    cmd.sub_print = 1;
                } else if (isdigit((unsigned char)*p)) {
                    cmd.sub_nth = cmd.sub_nth * 10 + (*p - '0');
                }
                p++;
            }
        } else if (cmd.op == 'y') {
            char delim = *p++;
            if (delim == '\0' || delim == '\n') {
                return -1;
            }
            sbuf_t s1 = {0};
            sbuf_t s2 = {0};
            if (parse_delimited(&p, delim, &s1, 0) != 0 ||
                parse_delimited(&p, delim, &s2, 0) != 0 ||
                strlen(s1.data ? s1.data : "") !=
                    strlen(s2.data ? s2.data : "")) {
                free(s1.data);
                free(s2.data);
                fprintf(stderr, "sed: invalid y command\n");
                return -1;
            }
            cmd.text = s1.data ? s1.data : strdup("");
            cmd.dst = s2.data ? s2.data : strdup("");
        } else if (cmd.op == 'a' || cmd.op == 'i' || cmd.op == 'c') {
            if (*p == '\\') {
                p++;
            }
            if (*p == '\n') {
                p++;
            }
            sbuf_t txt = {0};
            while (*p != '\0' && *p != '\n') {
                if (*p == '\\' && p[1] == '\n') {
                    (void)sb_push(&txt, '\n');
                    p += 2;
                    continue;
                }
                (void)sb_push(&txt, *p++);
            }
            cmd.text = txt.data ? txt.data : strdup("");
        } else if (cmd.op == ':' || cmd.op == 'b' || cmd.op == 't') {
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            const char *start = p;
            while (*p != '\0' && *p != ';' && *p != '\n' && *p != '}' &&
                   !isspace((unsigned char)*p)) {
                p++;
            }
            cmd.text = strndup(start, (size_t)(p - start));
        }

        if (prog_push(pr, cmd) != 0) {
            if (cmd.a1.kind == ADDR_REGEX) {
                regfree(&cmd.a1.re);
            }
            if (cmd.a2.kind == ADDR_REGEX) {
                regfree(&cmd.a2.re);
            }
            if (cmd.has_sub_re) {
                regfree(&cmd.sub_re);
            }
            free(cmd.repl);
            free(cmd.text);
            free(cmd.dst);
            return -1;
        }
    }
    if (bdepth != 0u) {
        fprintf(stderr, "sed: unclosed '{'\n");
        return -1;
    }
    return 0;
}

static int addr_match(const struct sed_addr *a, unsigned long lno, int is_last,
                      const char *pat) {
    if (a->kind == ADDR_LINE) {
        return lno == a->line;
    }
    if (a->kind == ADDR_LAST) {
        return is_last;
    }
    if (a->kind == ADDR_REGEX) {
        return regexec(&a->re, pat, 0u, NULL, 0) == 0;
    }
    return 1;
}

static int cmd_applies(struct sed_cmd *c, unsigned long lno, int is_last,
                       const char *pat) {
    int m = 1;
    if (c->a1.kind == ADDR_NONE) {
        m = 1;
    } else if (c->a2.kind == ADDR_NONE) {
        m = addr_match(&c->a1, lno, is_last, pat);
    } else {
        if (!c->in_range) {
            if (addr_match(&c->a1, lno, is_last, pat)) {
                m = 1;
                if (!(c->a2.kind == ADDR_LINE && lno >= c->a2.line)) {
                    c->in_range = 1;
                }
            } else {
                m = 0;
            }
        } else {
            m = 1;
            if (addr_match(&c->a2, lno, is_last, pat) ||
                (c->a2.kind == ADDR_LINE && lno >= c->a2.line)) {
                c->in_range = 0;
            }
        }
    }
    return c->negated ? !m : m;
}

static void append_repl(sbuf_t *out, const char *repl, const char *src,
                        const regmatch_t pm[10]) {
    for (const char *r = repl; *r != '\0'; r++) {
        if (*r == '&') {
            if (pm[0].rm_so >= 0) {
                (void)sb_putn(out, src + pm[0].rm_so,
                              (size_t)(pm[0].rm_eo - pm[0].rm_so));
            }
        } else if (*r == '\\' && r[1] != '\0') {
            r++;
            if (*r >= '1' && *r <= '9') {
                int idx = *r - '0';
                if (pm[idx].rm_so >= 0) {
                    (void)sb_putn(out, src + pm[idx].rm_so,
                                  (size_t)(pm[idx].rm_eo - pm[idx].rm_so));
                }
            } else if (*r == 'n') {
                (void)sb_push(out, '\n');
            } else {
                (void)sb_push(out, *r);
            }
        } else {
            (void)sb_push(out, *r);
        }
    }
}

static int do_substitute(const struct sed_cmd *c, sbuf_t *ps) {
    const char *src = ps->data ? ps->data : "";
    size_t slen = strlen(src);
    sbuf_t out = {0};
    size_t pos = 0u;
    int count = 0;
    int did = 0;
    int target_nth = (c->sub_nth > 0) ? c->sub_nth : 1;

    while (pos <= slen) {
        regmatch_t pm[10];
        for (int k = 0; k < 10; k++) {
            pm[k].rm_so = -1;
            pm[k].rm_eo = -1;
        }
        int eflags = (pos > 0u) ? REG_NOTBOL : 0;
        if (regexec(&c->sub_re, src + pos, 10u, pm, eflags) != 0) {
            (void)sb_putn(&out, src + pos, slen - pos);
            break;
        }
        size_t so = (size_t)pm[0].rm_so;
        size_t eo = (size_t)pm[0].rm_eo;
        count++;
        (void)sb_putn(&out, src + pos, so);
        if (c->sub_global || count == target_nth) {
            regmatch_t adj[10];
            for (int k = 0; k < 10; k++) {
                adj[k].rm_so = (pm[k].rm_so >= 0)
                                   ? (regoff_t)(pos + (size_t)pm[k].rm_so)
                                   : -1;
                adj[k].rm_eo = (pm[k].rm_eo >= 0)
                                   ? (regoff_t)(pos + (size_t)pm[k].rm_eo)
                                   : -1;
            }
            append_repl(&out, c->repl, src, adj);
            did = 1;
            if (!c->sub_global) {
                (void)sb_putn(&out, src + pos + eo, slen - (pos + eo));
                break;
            }
        } else {
            (void)sb_putn(&out, src + pos + so, eo - so);
        }
        if (eo == so) {
            if (pos + eo < slen) {
                (void)sb_push(&out, src[pos + eo]);
                pos += eo + 1u;
            } else {
                break;
            }
        } else {
            pos += eo;
        }
    }
    if (did) {
        sb_set(ps, out.data ? out.data : "");
    }
    free(out.data);
    return did;
}

static size_t find_label(const struct sed_prog *pr, const char *lbl) {
    if (!lbl || lbl[0] == '\0') {
        return pr->len;
    }
    for (size_t i = 0u; i < pr->len; i++) {
        if (pr->cmds[i].op == ':' && pr->cmds[i].text &&
            strcmp(pr->cmds[i].text, lbl) == 0) {
            return i + 1u;
        }
    }
    return pr->len;
}

/* Line reader spanning multiple input files with 1-line lookahead for '$'. */
struct line_src {
    char **files;
    int nfiles;
    int cur_idx;
    FILE *fp;
    char *next_line;
    int has_next;
};

static char *read_one_raw(struct line_src *ls) {
    for (;;) {
        if (!ls->fp) {
            if (ls->nfiles == 0) {
                if (ls->cur_idx > 0) {
                    return NULL;
                }
                ls->cur_idx = 1;
                ls->fp = stdin;
            } else {
                if (ls->cur_idx >= ls->nfiles) {
                    return NULL;
                }
                const char *fn = ls->files[ls->cur_idx++];
                if (strcmp(fn, "-") == 0) {
                    ls->fp = stdin;
                } else {
                    ls->fp = fopen(fn, "r");
                    if (!ls->fp) {
                        perror(fn);
                        continue;
                    }
                }
            }
        }
        char *buf = NULL;
        size_t cap = 0u;
        ssize_t n = getline(&buf, &cap, ls->fp);
        if (n >= 0) {
            if (n > 0 && buf[n - 1] == '\n') {
                buf[n - 1] = '\0';
            }
            return buf;
        }
        free(buf);
        if (ls->fp != stdin) {
            (void)fclose(ls->fp);
        }
        ls->fp = NULL;
    }
}

static int next_input_line(struct line_src *ls, char **out_line,
                           int *is_last_out) {
    if (!ls->has_next) {
        ls->next_line = read_one_raw(ls);
        ls->has_next = (ls->next_line != NULL);
    }
    if (!ls->has_next) {
        return 0;
    }
    *out_line = ls->next_line;
    ls->next_line = read_one_raw(ls);
    ls->has_next = (ls->next_line != NULL);
    *is_last_out = !ls->has_next;
    return 1;
}

int main(int argc, char **argv) {
    struct sed_prog pr;
    memset(&pr, 0, sizeof(pr));
    sbuf_t combined = {0};
    int had_script = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-n") == 0) {
            pr.suppress = 1;
            idx++;
        } else if (strcmp(argv[idx], "-E") == 0 ||
                   strcmp(argv[idx], "-r") == 0) {
            pr.ere = 1;
            idx++;
        } else if (strcmp(argv[idx], "-e") == 0 && idx + 1 < argc) {
            had_script = 1;
            (void)sb_puts(&combined, argv[idx + 1]);
            (void)sb_push(&combined, '\n');
            idx += 2;
        } else if (strcmp(argv[idx], "-f") == 0 && idx + 1 < argc) {
            had_script = 1;
            FILE *fp = fopen(argv[idx + 1], "r");
            if (!fp) {
                perror(argv[idx + 1]);
                free(combined.data);
                return 1;
            }
            char chunk[256];
            size_t n;
            while ((n = fread(chunk, 1u, sizeof(chunk), fp)) > 0u) {
                (void)sb_putn(&combined, chunk, n);
            }
            (void)sb_push(&combined, '\n');
            (void)fclose(fp);
            idx += 2;
        } else {
            fprintf(stderr, "sed: unknown option '%s'\n", argv[idx]);
            free(combined.data);
            return 1;
        }
    }

    if (!had_script) {
        if (idx >= argc) {
            fprintf(stderr, "usage: sed [-nE] [-e script] [-f file] "
                            "[script] [file...]\n");
            free(combined.data);
            return 1;
        }
        (void)sb_puts(&combined, argv[idx++]);
        (void)sb_push(&combined, '\n');
    }

    if (parse_script(&pr, combined.data ? combined.data : "") != 0) {
        free(combined.data);
        return 1;
    }
    free(combined.data);

    struct line_src ls = {
        .files = &argv[idx],
        .nfiles = argc - idx,
        .cur_idx = 0,
        .fp = NULL,
        .next_line = NULL,
        .has_next = 0,
    };

    sbuf_t ps = {0};
    sbuf_t hs = {0};
    sbuf_t append_q = {0};
    unsigned long lno = 0ul;
    int quit = 0;

    char *line = NULL;
    int is_last = 0;
    while (!quit && next_input_line(&ls, &line, &is_last)) {
        lno++;
        sb_set(&ps, line);
        free(line);

    restart_cycle:;
        int deleted = 0;
        int did_sub = 0;
        append_q.len = 0u;
        if (append_q.data) {
            append_q.data[0] = '\0';
        }

        for (size_t pc = 0u; pc < pr.len; pc++) {
            struct sed_cmd *c = &pr.cmds[pc];
            if (!cmd_applies(c, lno, is_last, ps.data ? ps.data : "")) {
                if (c->op == '{') {
                    pc = c->block_end - 1u;
                }
                continue;
            }
            switch (c->op) {
            case '{':
            case ':':
                break;
            case 's':
                if (do_substitute(c, &ps)) {
                    did_sub = 1;
                    if (c->sub_print) {
                        puts(ps.data ? ps.data : "");
                    }
                }
                break;
            case 'p':
                puts(ps.data ? ps.data : "");
                break;
            case 'P': {
                const char *s = ps.data ? ps.data : "";
                const char *nl = strchr(s, '\n');
                if (nl) {
                    (void)fwrite(s, 1u, (size_t)(nl - s), stdout);
                    putchar('\n');
                } else {
                    puts(s);
                }
                break;
            }
            case '=':
                printf("%lu\n", lno);
                break;
            case 'd':
                deleted = 1;
                pc = pr.len;
                break;
            case 'D': {
                const char *s = ps.data ? ps.data : "";
                const char *nl = strchr(s, '\n');
                if (nl) {
                    char *rest = strdup(nl + 1);
                    sb_set(&ps, rest ? rest : "");
                    free(rest);
                    goto restart_cycle;
                }
                deleted = 1;
                pc = pr.len;
                break;
            }
            case 'q':
                if (!pr.suppress) {
                    puts(ps.data ? ps.data : "");
                }
                deleted = 1;
                quit = 1;
                pc = pr.len;
                break;
            case 'n':
                if (!pr.suppress) {
                    puts(ps.data ? ps.data : "");
                }
                if (next_input_line(&ls, &line, &is_last)) {
                    lno++;
                    sb_set(&ps, line);
                    free(line);
                } else {
                    deleted = 1;
                    quit = 1;
                    pc = pr.len;
                }
                break;
            case 'N':
                if (next_input_line(&ls, &line, &is_last)) {
                    lno++;
                    (void)sb_push(&ps, '\n');
                    (void)sb_puts(&ps, line);
                    free(line);
                } else {
                    if (!pr.suppress) {
                        puts(ps.data ? ps.data : "");
                    }
                    deleted = 1;
                    quit = 1;
                    pc = pr.len;
                }
                break;
            case 'h':
                sb_set(&hs, ps.data ? ps.data : "");
                break;
            case 'H':
                (void)sb_push(&hs, '\n');
                (void)sb_puts(&hs, ps.data ? ps.data : "");
                break;
            case 'g':
                sb_set(&ps, hs.data ? hs.data : "");
                break;
            case 'G':
                (void)sb_push(&ps, '\n');
                (void)sb_puts(&ps, hs.data ? hs.data : "");
                break;
            case 'x': {
                sbuf_t tmp = ps;
                ps = hs;
                hs = tmp;
                break;
            }
            case 'y':
                if (ps.data && c->text && c->dst) {
                    for (size_t k = 0u; k < ps.len; k++) {
                        const char *f = strchr(c->text, ps.data[k]);
                        if (f) {
                            ps.data[k] = c->dst[f - c->text];
                        }
                    }
                }
                break;
            case 'i':
                puts(c->text ? c->text : "");
                break;
            case 'a':
                (void)sb_puts(&append_q, c->text ? c->text : "");
                (void)sb_push(&append_q, '\n');
                break;
            case 'c':
                if (!c->in_range) {
                    puts(c->text ? c->text : "");
                }
                deleted = 1;
                pc = pr.len;
                break;
            case 'b': {
                size_t tgt = find_label(&pr, c->text);
                pc = (tgt > 0u) ? (tgt - 1u) : pr.len;
                break;
            }
            case 't':
                if (did_sub) {
                    did_sub = 0;
                    size_t tgt = find_label(&pr, c->text);
                    pc = (tgt > 0u) ? (tgt - 1u) : pr.len;
                }
                break;
            default:
                break;
            }
        }

        if (!deleted && !pr.suppress) {
            puts(ps.data ? ps.data : "");
        }
        if (append_q.len > 0u) {
            fputs(append_q.data, stdout);
        }
    }

    free(ls.next_line);
    if (ls.fp && ls.fp != stdin) {
        (void)fclose(ls.fp);
    }
    free(ps.data);
    free(hs.data);
    free(append_q.data);
    for (size_t i = 0u; i < pr.len; i++) {
        if (pr.cmds[i].a1.kind == ADDR_REGEX) {
            regfree(&pr.cmds[i].a1.re);
        }
        if (pr.cmds[i].a2.kind == ADDR_REGEX) {
            regfree(&pr.cmds[i].a2.re);
        }
        if (pr.cmds[i].has_sub_re) {
            regfree(&pr.cmds[i].sub_re);
        }
        free(pr.cmds[i].repl);
        free(pr.cmds[i].text);
        free(pr.cmds[i].dst);
    }
    free(pr.cmds);
    return 0;
}
