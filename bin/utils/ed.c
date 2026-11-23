/**
 * @file     ed.c
 * @brief    Ring 3 POSIX.1-2024 XCU line editor (ed)
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
#include <unistd.h>

/* need:: COMP_LIBC_0024 */

typedef struct {
    char **lines;
    size_t len;
    size_t cap;
    size_t cur;
} buf_t;

typedef struct {
    buf_t cur;
    buf_t undo;
    int has_undo;
    int dirty;
    int quit_warned;
    int silent;
    int show_prompt;
    int show_err;
    int had_err;
    char *prompt;
    char *def_file;
    char *last_re;
    char *last_repl;
    char *last_shell;
    size_t marks[26];
    char last_err[128];
} ed_t;

static void buf_free(buf_t *b) {
    for (size_t i = 0; i < b->len; i++) {
        free(b->lines[i]);
    }
    free(b->lines);
    b->lines = NULL;
    b->len = 0;
    b->cap = 0;
    b->cur = 0;
}

static int buf_clone(buf_t *dst, const buf_t *src) {
    buf_free(dst);
    if (src->len == 0) {
        dst->cur = 0;
        return 0;
    }
    dst->lines = calloc(src->len, sizeof(*dst->lines));
    if (!dst->lines) {
        return -1;
    }
    for (size_t i = 0; i < src->len; i++) {
        dst->lines[i] = strdup(src->lines[i]);
        if (!dst->lines[i]) {
            dst->len = i;
            buf_free(dst);
            return -1;
        }
    }
    dst->len = src->len;
    dst->cap = src->len;
    dst->cur = src->cur;
    return 0;
}

static void save_undo(ed_t *ed) {
    if (buf_clone(&ed->undo, &ed->cur) == 0) {
        ed->has_undo = 1;
    }
}

static void ed_err(ed_t *ed, const char *msg) {
    (void)snprintf(ed->last_err, sizeof(ed->last_err), "%s", msg);
    (void)puts("?");
    if (ed->show_err) {
        (void)puts(ed->last_err);
    }
    ed->had_err = 1;
}

static int buf_insert_at(buf_t *b, size_t after, char **new_lines, size_t n) {
    if (n == 0) {
        return 0;
    }
    if (b->len + n > b->cap) {
        size_t nc = (b->cap == 0) ? 16 : b->cap * 2;
        while (nc < b->len + n) {
            nc *= 2;
        }
        char **nl = realloc(b->lines, nc * sizeof(*nl));
        if (!nl) {
            return -1;
        }
        b->lines = nl;
        b->cap = nc;
    }
    if (after < b->len) {
        (void)memmove(&b->lines[after + n], &b->lines[after],
                      (b->len - after) * sizeof(*b->lines));
    }
    for (size_t i = 0; i < n; i++) {
        b->lines[after + i] = new_lines[i];
    }
    b->len += n;
    b->cur = after + n;
    return 0;
}

static void buf_delete_range(buf_t *b, size_t lo, size_t hi) {
    if (lo < 1 || hi > b->len || lo > hi) {
        return;
    }
    for (size_t i = lo - 1; i < hi; i++) {
        free(b->lines[i]);
    }
    size_t cnt = hi - lo + 1;
    if (hi < b->len) {
        (void)memmove(&b->lines[lo - 1], &b->lines[hi],
                      (b->len - hi) * sizeof(*b->lines));
    }
    b->len -= cnt;
    if (b->len == 0) {
        b->cur = 0;
    } else if (lo <= b->len) {
        b->cur = lo;
    } else {
        b->cur = b->len;
    }
}

static char *read_line(FILE *fp) {
    size_t cap = 128;
    size_t len = 0;
    int ch = fgetc(fp);
    if (ch == EOF) {
        return NULL;
    }
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    while (ch != EOF && ch != '\n') {
        if (len + 2 > cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        buf[len++] = (char)ch;
        ch = fgetc(fp);
    }
    buf[len] = '\0';
    return buf;
}

static int read_file_into(ed_t *ed, const char *path, size_t after,
                          int replace) {
    FILE *fp = NULL;
    int is_pipe = (path[0] == '!');
    if (is_pipe) {
        fp = popen(path + 1, "r");
    } else {
        fp = fopen(path, "rb");
    }
    if (!fp) {
        ed_err(ed, "cannot open input file");
        return -1;
    }
    char **tmp = NULL;
    size_t n = 0;
    size_t cap = 0;
    size_t bytes = 0;
    char *ln = NULL;
    while ((ln = read_line(fp)) != NULL) {
        bytes += strlen(ln) + 1;
        if (n == cap) {
            size_t nc = (cap == 0) ? 16 : cap * 2;
            char **nt = realloc(tmp, nc * sizeof(*nt));
            if (!nt) {
                free(ln);
                for (size_t i = 0; i < n; i++) {
                    free(tmp[i]);
                }
                free(tmp);
                if (is_pipe) {
                    (void)pclose(fp);
                } else {
                    (void)fclose(fp);
                }
                return -1;
            }
            tmp = nt;
            cap = nc;
        }
        tmp[n++] = ln;
    }
    if (is_pipe) {
        (void)pclose(fp);
    } else {
        (void)fclose(fp);
    }
    if (replace) {
        buf_free(&ed->cur);
        after = 0;
    }
    if (n > 0 && buf_insert_at(&ed->cur, after, tmp, n) != 0) {
        for (size_t i = 0; i < n; i++) {
            free(tmp[i]);
        }
        free(tmp);
        ed_err(ed, "out of memory");
        return -1;
    }
    free(tmp);
    if (!ed->silent) {
        (void)printf("%zu\n", bytes);
    }
    return 0;
}

static int write_range_to(ed_t *ed, size_t lo, size_t hi, const char *path) {
    int is_pipe = (path[0] == '!');
    FILE *fp = is_pipe ? popen(path + 1, "w") : fopen(path, "wb");
    if (!fp) {
        ed_err(ed, "cannot open output file");
        return -1;
    }
    size_t bytes = 0;
    if (ed->cur.len > 0 && lo >= 1 && hi <= ed->cur.len && lo <= hi) {
        for (size_t i = lo - 1; i < hi; i++) {
            size_t l = strlen(ed->cur.lines[i]);
            (void)fwrite(ed->cur.lines[i], 1, l, fp);
            (void)fputc('\n', fp);
            bytes += l + 1;
        }
    }
    if (is_pipe) {
        (void)pclose(fp);
    } else {
        (void)fclose(fp);
    }
    if (!ed->silent) {
        (void)printf("%zu\n", bytes);
    }
    if (!is_pipe && lo == 1 && hi == ed->cur.len) {
        ed->dirty = 0;
    }
    return 0;
}

static void print_line_fmt(const char *s, size_t num, char mode) {
    if (mode == 'n') {
        (void)printf("%zu\t%s\n", num, s);
        return;
    }
    if (mode == 'l') {
        for (const char *p = s; *p != '\0'; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '\\') {
                (void)fputs("\\\\", stdout);
            } else if (c == '\t') {
                (void)fputs("\\t", stdout);
            } else if (c == '\b') {
                (void)fputs("\\b", stdout);
            } else if (c == '$') {
                (void)fputs("\\$", stdout);
            } else if (c < 0x20u || c >= 0x7fu) {
                (void)printf("\\%03o", c);
            } else {
                (void)putchar(c);
            }
        }
        (void)puts("$");
        return;
    }
    (void)puts(s);
}

static const char *extract_delim(const char *p, char delim, char **out) {
    size_t cap = 64;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    while (*p != '\0' && *p != delim) {
        if (len + 3 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        if (*p == '\\' && p[1] != '\0') {
            if (p[1] == delim) {
                buf[len++] = delim;
                p += 2;
                continue;
            }
            buf[len++] = *p++;
            buf[len++] = *p++;
            continue;
        }
        buf[len++] = *p++;
    }
    buf[len] = '\0';
    if (*p == delim) {
        p++;
    }
    *out = buf;
    return p;
}

static int search_re(ed_t *ed, const char *pat, int backward, size_t *out) {
    const char *use = (pat && pat[0] != '\0') ? pat : ed->last_re;
    if (!use || use[0] == '\0') {
        ed_err(ed, "no previous pattern");
        return -1;
    }
    if (pat && pat[0] != '\0') {
        free(ed->last_re);
        ed->last_re = strdup(pat);
    }
    if (ed->cur.len == 0) {
        ed_err(ed, "no match");
        return -1;
    }
    regex_t re;
    if (regcomp(&re, use, 0) != 0) {
        ed_err(ed, "invalid pattern");
        return -1;
    }
    size_t n = ed->cur.len;
    size_t cur = (ed->cur.cur > 0) ? ed->cur.cur : 1;
    for (size_t step = 1; step <= n; step++) {
        size_t idx = backward ? (((cur - 1 + n - (step % n)) % n) + 1)
                              : (((cur - 1 + step) % n) + 1);
        if (regexec(&re, ed->cur.lines[idx - 1], 0, NULL, 0) == 0) {
            regfree(&re);
            *out = idx;
            return 0;
        }
    }
    regfree(&re);
    ed_err(ed, "no match");
    return -1;
}

static int parse_single_addr(ed_t *ed, const char **pp, long *out) {
    const char *p = *pp;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    long base = -1;
    if (*p == '.') {
        base = (long)ed->cur.cur;
        p++;
    } else if (*p == '$') {
        base = (long)ed->cur.len;
        p++;
    } else if (isdigit((unsigned char)*p)) {
        char *end = NULL;
        base = strtol(p, &end, 10);
        p = end;
    } else if (*p == '/' || *p == '?') {
        char delim = *p++;
        char *pat = NULL;
        p = extract_delim(p, delim, &pat);
        if (!p) {
            return -1;
        }
        size_t found = 0;
        int rc = search_re(ed, pat, (delim == '?'), &found);
        free(pat);
        if (rc != 0) {
            return -1;
        }
        base = (long)found;
    } else if (*p == '\'') {
        p++;
        if (*p < 'a' || *p > 'z') {
            ed_err(ed, "invalid mark");
            return -1;
        }
        size_t m = ed->marks[*p - 'a'];
        p++;
        if (m == 0 || m > ed->cur.len) {
            ed_err(ed, "invalid mark");
            return -1;
        }
        base = (long)m;
    } else if (*p == '+' || *p == '-') {
        base = (long)ed->cur.cur;
    } else {
        return 0; /* no address present */
    }

    while (*p == '+' || *p == '-' || *p == ' ' || *p == '\t') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p != '+' && *p != '-') {
            break;
        }
        int sign = (*p == '+') ? 1 : -1;
        p++;
        long delta = 1;
        if (isdigit((unsigned char)*p)) {
            char *end = NULL;
            delta = strtol(p, &end, 10);
            p = end;
        }
        base += sign * delta;
    }
    *pp = p;
    *out = base;
    return 1;
}

static int parse_addr_range(ed_t *ed, const char **pp, long *a1, long *a2,
                            int *cnt) {
    const char *p = *pp;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    *cnt = 0;
    *a1 = (long)ed->cur.cur;
    *a2 = (long)ed->cur.cur;

    if (*p == '%' || (*p == ',' && !isdigit((unsigned char)p[1]))) {
        *a1 = (ed->cur.len > 0) ? 1 : 0;
        *a2 = (long)ed->cur.len;
        *cnt = 2;
        *pp = p + 1;
        return 0;
    }
    if (*p == ';') {
        *a1 = (long)ed->cur.cur;
        *a2 = (long)ed->cur.len;
        *cnt = 2;
        *pp = p + 1;
        return 0;
    }

    long first = 0;
    int r1 = parse_single_addr(ed, &p, &first);
    if (r1 < 0) {
        return -1;
    }
    if (r1 == 0) {
        *pp = p;
        return 0;
    }
    *a1 = first;
    *a2 = first;
    *cnt = 1;

    while (*p == ',' || *p == ';') {
        char sep = *p++;
        if (sep == ';') {
            if (first < 0 || (size_t)first > ed->cur.len) {
                ed_err(ed, "invalid address");
                return -1;
            }
            ed->cur.cur = (size_t)first;
        }
        long second = 0;
        int r2 = parse_single_addr(ed, &p, &second);
        if (r2 < 0) {
            return -1;
        }
        if (r2 == 0) {
            second = (long)ed->cur.cur;
        }
        *a1 = *a2;
        *a2 = second;
        *cnt = 2;
        first = second;
    }
    *pp = p;
    return 0;
}

static int read_input_lines(FILE *in, char ***out_lines, size_t *out_n) {
    char **arr = NULL;
    size_t n = 0;
    size_t cap = 0;
    char *ln = NULL;
    while ((ln = read_line(in)) != NULL) {
        if (strcmp(ln, ".") == 0) {
            free(ln);
            break;
        }
        if (n == cap) {
            size_t nc = (cap == 0) ? 16 : cap * 2;
            char **na = realloc(arr, nc * sizeof(*na));
            if (!na) {
                free(ln);
                for (size_t i = 0; i < n; i++) {
                    free(arr[i]);
                }
                free(arr);
                return -1;
            }
            arr = na;
            cap = nc;
        }
        arr[n++] = ln;
    }
    *out_lines = arr;
    *out_n = n;
    return 0;
}

static char *sub_line(const char *line, const regex_t *re, const char *repl,
                      int global, int nth, int *matched_out) {
    size_t cap = strlen(line) + 64;
    size_t len = 0;
    char *out = malloc(cap);
    if (!out) {
        return NULL;
    }
    const char *p = line;
    int eflags = 0;
    int occ = 0;
    *matched_out = 0;

    for (;;) {
        regmatch_t pm[10];
        if (regexec(re, p, 10, pm, eflags) != 0) {
            size_t rem = strlen(p);
            if (len + rem + 1 > cap) {
                cap = len + rem + 1;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            (void)memcpy(out + len, p, rem);
            len += rem;
            break;
        }
        occ++;
        size_t pre = (size_t)pm[0].rm_so;
        size_t mlen = (size_t)(pm[0].rm_eo - pm[0].rm_so);
        if (len + pre + 1 > cap) {
            cap = (len + pre + 64) * 2;
            char *nb = realloc(out, cap);
            if (!nb) {
                free(out);
                return NULL;
            }
            out = nb;
        }
        if (pre > 0) {
            (void)memcpy(out + len, p, pre);
            len += pre;
        }
        if (!global && occ < nth) {
            if (len + mlen + 1 > cap) {
                cap = (len + mlen + 64) * 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            if (mlen > 0) {
                (void)memcpy(out + len, p + pm[0].rm_so, mlen);
                len += mlen;
            }
            p += pm[0].rm_eo;
            eflags = REG_NOTBOL;
            if (mlen == 0 && *p != '\0') {
                out[len++] = *p++;
            } else if (mlen == 0) {
                break;
            }
            continue;
        }
        *matched_out = 1;
        for (const char *r = repl; *r != '\0'; r++) {
            if (*r == '&') {
                if (len + mlen + 1 > cap) {
                    cap = (len + mlen + 64) * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        return NULL;
                    }
                    out = nb;
                }
                if (mlen > 0) {
                    (void)memcpy(out + len, p + pm[0].rm_so, mlen);
                    len += mlen;
                }
            } else if (*r == '\\' && r[1] >= '1' && r[1] <= '9') {
                int idx = r[1] - '0';
                r++;
                if (pm[idx].rm_so >= 0 && pm[idx].rm_eo >= pm[idx].rm_so) {
                    size_t gl = (size_t)(pm[idx].rm_eo - pm[idx].rm_so);
                    if (len + gl + 1 > cap) {
                        cap = (len + gl + 64) * 2;
                        char *nb = realloc(out, cap);
                        if (!nb) {
                            free(out);
                            return NULL;
                        }
                        out = nb;
                    }
                    if (gl > 0) {
                        (void)memcpy(out + len, p + pm[idx].rm_so, gl);
                        len += gl;
                    }
                }
            } else {
                if (*r == '\\' && r[1] != '\0') {
                    r++;
                }
                if (len + 2 > cap) {
                    cap *= 2;
                    char *nb = realloc(out, cap);
                    if (!nb) {
                        free(out);
                        return NULL;
                    }
                    out = nb;
                }
                out[len++] = *r;
            }
        }
        p += pm[0].rm_eo;
        eflags = REG_NOTBOL;
        if (!global) {
            size_t rem = strlen(p);
            if (len + rem + 1 > cap) {
                cap = len + rem + 1;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            (void)memcpy(out + len, p, rem);
            len += rem;
            break;
        }
        if (mlen == 0) {
            if (*p == '\0') {
                break;
            }
            if (len + 2 > cap) {
                cap *= 2;
                char *nb = realloc(out, cap);
                if (!nb) {
                    free(out);
                    return NULL;
                }
                out = nb;
            }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return out;
}

static int exec_cmd(ed_t *ed, const char *cmdline, FILE *in, int *quit);

static int cmd_global(ed_t *ed, long a1, long a2, int cnt, const char *p,
                      int invert, FILE *in, int *quit) {
    size_t lo = (cnt == 0) ? 1 : (size_t)a1;
    size_t hi = (cnt == 0) ? ed->cur.len : (size_t)a2;
    if (ed->cur.len == 0 || lo < 1 || hi > ed->cur.len || lo > hi) {
        ed_err(ed, "invalid address");
        return -1;
    }
    char delim = *p++;
    if (delim == '\0' || delim == '\n') {
        ed_err(ed, "invalid pattern delimiter");
        return -1;
    }
    char *pat = NULL;
    p = extract_delim(p, delim, &pat);
    if (!p) {
        return -1;
    }
    const char *use = (pat[0] != '\0') ? pat : ed->last_re;
    if (!use || use[0] == '\0') {
        free(pat);
        ed_err(ed, "no previous pattern");
        return -1;
    }
    if (pat[0] != '\0') {
        free(ed->last_re);
        ed->last_re = strdup(pat);
    }
    regex_t re;
    if (regcomp(&re, use, 0) != 0) {
        free(pat);
        ed_err(ed, "invalid pattern");
        return -1;
    }
    free(pat);

    const char *subcmd = (*p != '\0') ? p : "p";
    size_t n = ed->cur.len;
    char **marked = calloc(n, sizeof(*marked));
    if (!marked) {
        regfree(&re);
        return -1;
    }
    size_t nmarked = 0;
    for (size_t i = lo - 1; i < hi; i++) {
        int m = (regexec(&re, ed->cur.lines[i], 0, NULL, 0) == 0);
        if (invert ? !m : m) {
            marked[nmarked++] = ed->cur.lines[i];
        }
    }
    regfree(&re);

    save_undo(ed);
    for (size_t m = 0; m < nmarked && !*quit; m++) {
        size_t pos = 0;
        for (size_t i = 0; i < ed->cur.len; i++) {
            if (ed->cur.lines[i] == marked[m]) {
                pos = i + 1;
                break;
            }
        }
        if (pos == 0) {
            continue;
        }
        ed->cur.cur = pos;
        (void)exec_cmd(ed, subcmd, in, quit);
    }
    free(marked);
    return 0;
}

static int exec_cmd(ed_t *ed, const char *cmdline, FILE *in, int *quit) {
    const char *p = cmdline;
    long a1 = 0;
    long a2 = 0;
    int cnt = 0;
    if (parse_addr_range(ed, &p, &a1, &a2, &cnt) != 0) {
        return -1;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    char cmd = *p;
    if (cmd != 'q') {
        ed->quit_warned = 0;
    }

    if (cmd == '\0') {
        size_t target = (cnt > 0) ? (size_t)a2 : (ed->cur.cur + 1);
        if (target < 1 || target > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        ed->cur.cur = target;
        print_line_fmt(ed->cur.lines[target - 1], target, 'p');
        return 0;
    }
    p++;

    if (cmd == 'q' || cmd == 'Q') {
        if (cmd == 'q' && ed->dirty && !ed->quit_warned) {
            ed->quit_warned = 1;
            ed_err(ed, "warning: file modified");
            return -1;
        }
        *quit = 1;
        return 0;
    }
    if (cmd == 'h') {
        if (ed->last_err[0] != '\0') {
            (void)puts(ed->last_err);
        }
        return 0;
    }
    if (cmd == 'H') {
        ed->show_err = !ed->show_err;
        return 0;
    }
    if (cmd == 'P') {
        ed->show_prompt = !ed->show_prompt;
        return 0;
    }
    if (cmd == 'u') {
        if (!ed->has_undo) {
            ed_err(ed, "nothing to undo");
            return -1;
        }
        buf_t tmp = ed->cur;
        ed->cur = ed->undo;
        ed->undo = tmp;
        ed->dirty = 1;
        return 0;
    }
    if (cmd == '=') {
        size_t target = (cnt > 0) ? (size_t)a2 : ed->cur.len;
        (void)printf("%zu\n", target);
        return 0;
    }
    if (cmd == 'k') {
        if (*p < 'a' || *p > 'z' || a2 < 1 || (size_t)a2 > ed->cur.len) {
            ed_err(ed, "invalid mark");
            return -1;
        }
        ed->marks[*p - 'a'] = (size_t)a2;
        return 0;
    }
    if (cmd == '!') {
        const char *sc = (*p != '\0') ? p : ed->last_shell;
        if (!sc) {
            ed_err(ed, "no previous command");
            return -1;
        }
        if (*p != '\0') {
            free(ed->last_shell);
            ed->last_shell = strdup(p);
        }
        (void)system(sc);
        if (!ed->silent) {
            (void)puts("!");
        }
        return 0;
    }
    if (cmd == 'f') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p != '\0') {
            free(ed->def_file);
            ed->def_file = strdup(p);
        }
        if (!ed->def_file) {
            ed_err(ed, "no current filename");
            return -1;
        }
        (void)puts(ed->def_file);
        return 0;
    }
    if (cmd == 'e' || cmd == 'E') {
        if (cmd == 'e' && ed->dirty && !ed->quit_warned) {
            ed->quit_warned = 1;
            ed_err(ed, "warning: file modified");
            return -1;
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        const char *fn = (*p != '\0') ? p : ed->def_file;
        if (!fn) {
            ed_err(ed, "no current filename");
            return -1;
        }
        if (*p != '\0' && *p != '!') {
            free(ed->def_file);
            ed->def_file = strdup(p);
        }
        if (read_file_into(ed, fn, 0, 1) == 0) {
            ed->dirty = 0;
            ed->has_undo = 0;
            return 0;
        }
        return -1;
    }
    if (cmd == 'r') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        const char *fn = (*p != '\0') ? p : ed->def_file;
        if (!fn) {
            ed_err(ed, "no current filename");
            return -1;
        }
        if (!ed->def_file && *p != '!') {
            ed->def_file = strdup(fn);
        }
        size_t after = (cnt > 0) ? (size_t)a2 : ed->cur.len;
        if (after > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        save_undo(ed);
        if (read_file_into(ed, fn, after, 0) == 0) {
            ed->dirty = 1;
            return 0;
        }
        return -1;
    }
    if (cmd == 'w') {
        int do_quit = 0;
        if (*p == 'q') {
            do_quit = 1;
            p++;
        }
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        const char *fn = (*p != '\0') ? p : ed->def_file;
        if (!fn) {
            ed_err(ed, "no current filename");
            return -1;
        }
        if (!ed->def_file && *p != '!') {
            ed->def_file = strdup(fn);
        }
        size_t lo = (cnt > 0) ? (size_t)a1 : ((ed->cur.len > 0) ? 1 : 0);
        size_t hi = (cnt > 0) ? (size_t)a2 : ed->cur.len;
        if (ed->cur.len > 0 && (lo < 1 || hi > ed->cur.len || lo > hi)) {
            ed_err(ed, "invalid address");
            return -1;
        }
        if (write_range_to(ed, lo, hi, fn) != 0) {
            return -1;
        }
        if (do_quit) {
            *quit = 1;
        }
        return 0;
    }
    if (cmd == 'p' || cmd == 'n' || cmd == 'l') {
        if (ed->cur.len == 0 || a1 < 1 || a2 < a1 || (size_t)a2 > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        for (size_t i = (size_t)a1; i <= (size_t)a2; i++) {
            print_line_fmt(ed->cur.lines[i - 1], i, cmd);
        }
        ed->cur.cur = (size_t)a2;
        return 0;
    }
    if (cmd == 'a' || cmd == 'i' || cmd == 'c') {
        size_t pos = 0;
        if (cmd == 'a') {
            if (a2 < 0 || (size_t)a2 > ed->cur.len) {
                ed_err(ed, "invalid address");
                return -1;
            }
            pos = (size_t)a2;
        } else if (cmd == 'i') {
            if (ed->cur.len == 0 && a2 == 0) {
                pos = 0;
            } else if (a2 < 1 || (size_t)a2 > ed->cur.len) {
                ed_err(ed, "invalid address");
                return -1;
            } else {
                pos = (size_t)a2 - 1;
            }
        } else {
            if (a1 < 1 || a2 < a1 || (size_t)a2 > ed->cur.len) {
                ed_err(ed, "invalid address");
                return -1;
            }
            pos = (size_t)a1 - 1;
        }
        char **ins = NULL;
        size_t n = 0;
        if (read_input_lines(in, &ins, &n) != 0) {
            ed_err(ed, "out of memory");
            return -1;
        }
        save_undo(ed);
        if (cmd == 'c') {
            buf_delete_range(&ed->cur, (size_t)a1, (size_t)a2);
        }
        if (n > 0) {
            if (buf_insert_at(&ed->cur, pos, ins, n) != 0) {
                for (size_t i = 0; i < n; i++) {
                    free(ins[i]);
                }
                free(ins);
                ed_err(ed, "out of memory");
                return -1;
            }
        }
        free(ins);
        ed->dirty = 1;
        return 0;
    }
    if (cmd == 'd') {
        if (ed->cur.len == 0 || a1 < 1 || a2 < a1 || (size_t)a2 > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        save_undo(ed);
        buf_delete_range(&ed->cur, (size_t)a1, (size_t)a2);
        ed->dirty = 1;
        if (*p == 'p' || *p == 'n' || *p == 'l') {
            if (ed->cur.cur > 0) {
                print_line_fmt(ed->cur.lines[ed->cur.cur - 1], ed->cur.cur, *p);
            }
        }
        return 0;
    }
    if (cmd == 'j') {
        size_t lo = (cnt == 0) ? ed->cur.cur : (size_t)a1;
        size_t hi = (cnt == 0) ? (ed->cur.cur + 1) : (size_t)a2;
        if (cnt == 1) {
            return 0;
        }
        if (lo < 1 || hi > ed->cur.len || lo > hi) {
            ed_err(ed, "invalid address");
            return -1;
        }
        size_t total = 1;
        for (size_t i = lo - 1; i < hi; i++) {
            total += strlen(ed->cur.lines[i]);
        }
        char *joined = malloc(total);
        if (!joined) {
            ed_err(ed, "out of memory");
            return -1;
        }
        size_t off = 0;
        for (size_t i = lo - 1; i < hi; i++) {
            size_t l = strlen(ed->cur.lines[i]);
            (void)memcpy(joined + off, ed->cur.lines[i], l);
            off += l;
        }
        joined[off] = '\0';
        save_undo(ed);
        buf_delete_range(&ed->cur, lo, hi);
        (void)buf_insert_at(&ed->cur, lo - 1, &joined, 1);
        ed->dirty = 1;
        return 0;
    }
    if (cmd == 'm' || cmd == 't') {
        if (ed->cur.len == 0 || a1 < 1 || a2 < a1 || (size_t)a2 > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        long dst = (long)ed->cur.cur;
        int rd = parse_single_addr(ed, &p, &dst);
        if (rd < 0) {
            return -1;
        }
        if (dst < 0 || (size_t)dst > ed->cur.len) {
            ed_err(ed, "invalid destination");
            return -1;
        }
        if (cmd == 'm' && dst >= a1 && dst <= a2) {
            ed_err(ed, "invalid destination");
            return -1;
        }
        size_t n = (size_t)(a2 - a1 + 1);
        char **cp = calloc(n, sizeof(*cp));
        if (!cp) {
            ed_err(ed, "out of memory");
            return -1;
        }
        for (size_t i = 0; i < n; i++) {
            cp[i] = strdup(ed->cur.lines[(size_t)a1 - 1 + i]);
            if (!cp[i]) {
                for (size_t j = 0; j < i; j++) {
                    free(cp[j]);
                }
                free(cp);
                ed_err(ed, "out of memory");
                return -1;
            }
        }
        save_undo(ed);
        size_t ins_pos = (size_t)dst;
        if (cmd == 'm') {
            buf_delete_range(&ed->cur, (size_t)a1, (size_t)a2);
            if (dst > a2) {
                ins_pos -= n;
            }
        }
        (void)buf_insert_at(&ed->cur, ins_pos, cp, n);
        free(cp);
        ed->dirty = 1;
        return 0;
    }
    if (cmd == 's') {
        if (ed->cur.len == 0 || a1 < 1 || a2 < a1 || (size_t)a2 > ed->cur.len) {
            ed_err(ed, "invalid address");
            return -1;
        }
        char *pat = NULL;
        char *repl = NULL;
        int global = 0;
        int nth = 1;
        char pmode = '\0';
        if (*p == '\0') {
            if (!ed->last_re || !ed->last_repl) {
                ed_err(ed, "no previous substitution");
                return -1;
            }
            pat = strdup(ed->last_re);
            repl = strdup(ed->last_repl);
        } else {
            char delim = *p++;
            p = extract_delim(p, delim, &pat);
            if (!p) {
                return -1;
            }
            p = extract_delim(p, delim, &repl);
            if (!p) {
                free(pat);
                return -1;
            }
            while (*p != '\0') {
                if (*p == 'g') {
                    global = 1;
                    p++;
                } else if (*p == 'p' || *p == 'n' || *p == 'l') {
                    pmode = *p++;
                } else if (isdigit((unsigned char)*p)) {
                    char *end = NULL;
                    nth = (int)strtol(p, &end, 10);
                    p = end;
                } else {
                    p++;
                }
            }
            if (pat[0] == '\0') {
                free(pat);
                if (!ed->last_re) {
                    free(repl);
                    ed_err(ed, "no previous pattern");
                    return -1;
                }
                pat = strdup(ed->last_re);
            } else {
                free(ed->last_re);
                ed->last_re = strdup(pat);
            }
            if (strcmp(repl, "%") == 0) {
                free(repl);
                if (!ed->last_repl) {
                    free(pat);
                    ed_err(ed, "no previous substitution");
                    return -1;
                }
                repl = strdup(ed->last_repl);
            } else {
                free(ed->last_repl);
                ed->last_repl = strdup(repl);
            }
        }
        regex_t re;
        if (regcomp(&re, pat ? pat : "", 0) != 0) {
            free(pat);
            free(repl);
            ed_err(ed, "invalid pattern");
            return -1;
        }
        save_undo(ed);
        int any = 0;
        size_t last_hit = 0;
        for (size_t i = (size_t)a1; i <= (size_t)a2; i++) {
            int hit = 0;
            char *nl = sub_line(ed->cur.lines[i - 1], &re, repl ? repl : "",
                                global, nth, &hit);
            if (hit && nl) {
                free(ed->cur.lines[i - 1]);
                ed->cur.lines[i - 1] = nl;
                any = 1;
                last_hit = i;
            } else {
                free(nl);
            }
        }
        regfree(&re);
        free(pat);
        free(repl);
        if (!any) {
            ed_err(ed, "no match");
            return -1;
        }
        ed->cur.cur = last_hit;
        ed->dirty = 1;
        if (pmode != '\0') {
            print_line_fmt(ed->cur.lines[last_hit - 1], last_hit, pmode);
        }
        return 0;
    }
    if (cmd == 'g' || cmd == 'v') {
        return cmd_global(ed, a1, a2, cnt, p, (cmd == 'v'), in, quit);
    }

    ed_err(ed, "unknown command");
    return -1;
}

int main(int argc, char **argv) {
    ed_t ed = {0};
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:s")) != -1) {
        switch (opt) {
        case 'p':
            free(ed.prompt);
            ed.prompt = strdup(optarg);
            ed.show_prompt = 1;
            break;
        case 's':
            ed.silent = 1;
            break;
        default:
            (void)fprintf(stderr, "usage: ed [-p string] [-s] [file]\n");
            free(ed.prompt);
            return 1;
        }
    }
    if (!ed.prompt) {
        ed.prompt = strdup("*");
    }

    if (optind < argc) {
        const char *fn = argv[optind];
        if (fn[0] != '!') {
            ed.def_file = strdup(fn);
        }
        if (access(fn, F_OK) == 0 || fn[0] == '!') {
            (void)read_file_into(&ed, fn, 0, 1);
            ed.had_err = 0;
        } else if (!ed.silent) {
            (void)printf("%s: No such file or directory\n", fn);
        }
    }

    int quit = 0;
    while (!quit) {
        if (ed.show_prompt && ed.prompt) {
            (void)fputs(ed.prompt, stdout);
            (void)fflush(stdout);
        }
        char *cmd = read_line(stdin);
        if (!cmd) {
            break;
        }
        (void)exec_cmd(&ed, cmd, stdin, &quit);
        free(cmd);
    }

    int rc = ed.had_err ? 1 : 0;
    buf_free(&ed.cur);
    buf_free(&ed.undo);
    free(ed.prompt);
    free(ed.def_file);
    free(ed.last_re);
    free(ed.last_repl);
    free(ed.last_shell);
    return rc;
}
