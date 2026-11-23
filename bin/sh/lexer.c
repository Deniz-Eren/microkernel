/**
 * @file     lexer.c
 * @brief    POSIX.1-2024 shell tokenizer with quotes, heredocs, and buffers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sh.h"

/* need:: COMP_LIBC_0017 */

int sh_buf_push(sh_buf_t *b, char c) {
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

int sh_buf_putn(sh_buf_t *b, const char *s, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (sh_buf_push(b, s[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

int sh_buf_puts(sh_buf_t *b, const char *s) {
    if (!s) {
        return 0;
    }
    return sh_buf_putn(b, s, strlen(s));
}

void sh_buf_free(sh_buf_t *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0u;
    b->cap = 0u;
}

int sh_vec_push(sh_vec_t *v, char *s) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2u : 16u;
        char **ni = (char **)reallocarray(v->items, ncap, sizeof(char *));
        if (!ni) {
            return -1;
        }
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->len++] = s;
    return 0;
}

void sh_vec_free(sh_vec_t *v) {
    for (size_t i = 0u; i < v->len; i++) {
        free(v->items[i]);
    }
    free(v->items);
    v->items = NULL;
    v->len = 0u;
    v->cap = 0u;
}

static int tokvec_push(sh_tokvec_t *tv, sh_tok_t tok) {
    if (tv->len == tv->cap) {
        size_t ncap = tv->cap ? tv->cap * 2u : 64u;
        sh_tok_t *ni =
            (sh_tok_t *)reallocarray(tv->items, ncap, sizeof(sh_tok_t));
        if (!ni) {
            return -1;
        }
        tv->items = ni;
        tv->cap = ncap;
    }
    tv->items[tv->len++] = tok;
    return 0;
}

void sh_tokvec_free(sh_tokvec_t *tv) {
    for (size_t i = 0u; i < tv->len; i++) {
        free(tv->items[i].text);
        free(tv->items[i].heredoc);
    }
    free(tv->items);
    tv->items = NULL;
    tv->len = 0u;
    tv->cap = 0u;
}

static int is_op_start(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>' ||
           c == '(' || c == ')' || c == '\n';
}

/* Scan nested $(...) or $((...)) or ${...} preserving raw text for expander. */
static int scan_dollar_nested(const char **pp, sh_buf_t *b) {
    const char *p = *pp;
    if (p[0] == '$' && p[1] == '\'') {
        if (sh_buf_putn(b, p, 2u) != 0) {
            return -1;
        }
        p += 2;
        while (*p != '\0' && *p != '\'') {
            if (*p == '\\' && p[1] != '\0') {
                if (sh_buf_putn(b, p, 2u) != 0) {
                    return -1;
                }
                p += 2;
            } else {
                if (sh_buf_push(b, *p++) != 0) {
                    return -1;
                }
            }
        }
        if (*p == '\'') {
            if (sh_buf_push(b, *p++) != 0) {
                return -1;
            }
        }
        *pp = p;
        return 0;
    }
    if (p[0] == '$' && p[1] == '{') {
        int depth = 1;
        if (sh_buf_putn(b, p, 2u) != 0) {
            return -1;
        }
        p += 2;
        while (*p != '\0' && depth > 0) {
            if (*p == '\\' && p[1] != '\0') {
                if (sh_buf_putn(b, p, 2u) != 0) {
                    return -1;
                }
                p += 2;
                continue;
            }
            if (*p == '\'') {
                if (sh_buf_push(b, *p++) != 0) {
                    return -1;
                }
                while (*p != '\0' && *p != '\'') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                if (*p == '\'') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                continue;
            }
            if (*p == '"') {
                if (sh_buf_push(b, *p++) != 0) {
                    return -1;
                }
                while (*p != '\0' && *p != '"') {
                    if (*p == '\\' && p[1] != '\0') {
                        if (sh_buf_putn(b, p, 2u) != 0) {
                            return -1;
                        }
                        p += 2;
                    } else {
                        if (sh_buf_push(b, *p++) != 0) {
                            return -1;
                        }
                    }
                }
                if (*p == '"') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                continue;
            }
            if (*p == '{') {
                depth++;
            } else if (*p == '}') {
                depth--;
            }
            if (sh_buf_push(b, *p++) != 0) {
                return -1;
            }
        }
        *pp = p;
        return 0;
    }
    if (p[0] == '$' && p[1] == '(') {
        int is_arith = (p[2] == '(');
        int depth = is_arith ? 2 : 1;
        if (sh_buf_putn(b, p, is_arith ? 3u : 2u) != 0) {
            return -1;
        }
        p += is_arith ? 3 : 2;
        while (*p != '\0' && depth > 0) {
            if (*p == '\\' && p[1] != '\0') {
                if (sh_buf_putn(b, p, 2u) != 0) {
                    return -1;
                }
                p += 2;
                continue;
            }
            if (*p == '\'') {
                if (sh_buf_push(b, *p++) != 0) {
                    return -1;
                }
                while (*p != '\0' && *p != '\'') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                if (*p == '\'') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                continue;
            }
            if (*p == '"') {
                if (sh_buf_push(b, *p++) != 0) {
                    return -1;
                }
                while (*p != '\0' && *p != '"') {
                    if (*p == '\\' && p[1] != '\0') {
                        if (sh_buf_putn(b, p, 2u) != 0) {
                            return -1;
                        }
                        p += 2;
                    } else {
                        if (sh_buf_push(b, *p++) != 0) {
                            return -1;
                        }
                    }
                }
                if (*p == '"') {
                    if (sh_buf_push(b, *p++) != 0) {
                        return -1;
                    }
                }
                continue;
            }
            if (*p == '(') {
                depth++;
            } else if (*p == ')') {
                depth--;
            }
            if (sh_buf_push(b, *p++) != 0) {
                return -1;
            }
        }
        *pp = p;
        return 0;
    }
    if (sh_buf_push(b, *p++) != 0) {
        return -1;
    }
    *pp = p;
    return 0;
}

static void strip_delim_quotes(const char *raw, char *out, size_t out_sz,
                               int *quoted_out) {
    size_t j = 0u;
    int q = 0;
    for (const char *p = raw; *p != '\0' && j + 1u < out_sz; p++) {
        if (*p == '\'' || *p == '"') {
            q = 1;
            char qc = *p++;
            while (*p != '\0' && *p != qc && j + 1u < out_sz) {
                if (qc == '"' && *p == '\\' && p[1] != '\0') {
                    p++;
                }
                out[j++] = *p++;
            }
            if (*p == '\0') {
                break;
            }
        } else if (*p == '\\' && p[1] != '\0') {
            q = 1;
            out[j++] = *++p;
        } else {
            out[j++] = *p;
        }
    }
    out[j] = '\0';
    *quoted_out = q;
}

struct pending_hd {
    size_t tok_idx;
    char delim[128];
    int strip_tabs;
    int quoted;
};

static int read_pending_heredocs(const char **pp, sh_tokvec_t *out,
                                 struct pending_hd *hds, size_t *nhds) {
    const char *p = *pp;
    for (size_t i = 0u; i < *nhds; i++) {
        sh_buf_t body = {0};
        while (*p != '\0') {
            const char *line_start = p;
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            const char *line_end = p;
            if (*p == '\n') {
                p++;
            }
            const char *cmp_start = line_start;
            if (hds[i].strip_tabs) {
                while (cmp_start < line_end && *cmp_start == '\t') {
                    cmp_start++;
                }
            }
            size_t llen = (size_t)(line_end - cmp_start);
            if (llen > 0u && cmp_start[llen - 1u] == '\r') {
                llen--;
            }
            if (llen == strlen(hds[i].delim) &&
                memcmp(cmp_start, hds[i].delim, llen) == 0) {
                break;
            }
            if (sh_buf_putn(&body, cmp_start, (size_t)(line_end - cmp_start)) !=
                    0 ||
                sh_buf_push(&body, '\n') != 0) {
                sh_buf_free(&body);
                return -1;
            }
        }
        out->items[hds[i].tok_idx].heredoc = body.data ? body.data : strdup("");
        out->items[hds[i].tok_idx].heredoc_quoted = hds[i].quoted;
    }
    *nhds = 0u;
    *pp = p;
    return 0;
}

static int scan_word(const char **pp, sh_buf_t *w) {
    const char *p = *pp;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' &&
           !is_op_start(*p)) {
        if (*p == '\\') {
            if (p[1] == '\n') {
                p += 2;
                continue;
            }
            if (p[1] != '\0') {
                if (sh_buf_putn(w, p, 2u) != 0) {
                    return -1;
                }
                p += 2;
            } else {
                if (sh_buf_push(w, *p++) != 0) {
                    return -1;
                }
            }
        } else if (*p == '\'') {
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
            while (*p != '\0' && *p != '\'') {
                if (sh_buf_push(w, *p++) != 0) {
                    return -1;
                }
            }
            if (*p != '\'') {
                fprintf(stderr, "sh: unterminated single quote\n");
                return -1;
            }
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
        } else if (*p == '"') {
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
            while (*p != '\0' && *p != '"') {
                if (*p == '\\' && p[1] == '\n') {
                    p += 2;
                    continue;
                }
                if (*p == '\\' && p[1] != '\0') {
                    if (sh_buf_putn(w, p, 2u) != 0) {
                        return -1;
                    }
                    p += 2;
                } else if (*p == '$') {
                    if (scan_dollar_nested(&p, w) != 0) {
                        return -1;
                    }
                } else if (*p == '`') {
                    if (sh_buf_push(w, *p++) != 0) {
                        return -1;
                    }
                    while (*p != '\0' && *p != '`') {
                        if (*p == '\\' && p[1] != '\0') {
                            if (sh_buf_putn(w, p, 2u) != 0) {
                                return -1;
                            }
                            p += 2;
                        } else {
                            if (sh_buf_push(w, *p++) != 0) {
                                return -1;
                            }
                        }
                    }
                    if (*p == '`') {
                        if (sh_buf_push(w, *p++) != 0) {
                            return -1;
                        }
                    }
                } else {
                    if (sh_buf_push(w, *p++) != 0) {
                        return -1;
                    }
                }
            }
            if (*p != '"') {
                fprintf(stderr, "sh: unterminated double quote\n");
                return -1;
            }
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
        } else if (*p == '$') {
            if (scan_dollar_nested(&p, w) != 0) {
                return -1;
            }
        } else if (*p == '`') {
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
            while (*p != '\0' && *p != '`') {
                if (*p == '\\' && p[1] != '\0') {
                    if (sh_buf_putn(w, p, 2u) != 0) {
                        return -1;
                    }
                    p += 2;
                } else {
                    if (sh_buf_push(w, *p++) != 0) {
                        return -1;
                    }
                }
            }
            if (*p != '`') {
                fprintf(stderr, "sh: unterminated backquote\n");
                return -1;
            }
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
        } else {
            if (sh_buf_push(w, *p++) != 0) {
                return -1;
            }
        }
    }
    *pp = p;
    return 0;
}

static int sh_lex_internal(const char *src, sh_tokvec_t *out,
                           const char *blocked_alias, int depth) {
    const char *p = src;
    struct pending_hd hds[8];
    size_t nhds = 0u;
    int cmd_pos = 1;
    int expecting_hd_delim = 0;
    size_t hd_tok_idx = 0u;
    int hd_strip = 0;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p == '\\' && p[1] == '\n') {
            p += 2;
            continue;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == '#') {
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }
        if (*p == '\n') {
            p++;
            sh_tok_t t = {.kind = T_NL, .io_fd = -1};
            if (tokvec_push(out, t) != 0) {
                return -1;
            }
            if (nhds > 0u && read_pending_heredocs(&p, out, hds, &nhds) != 0) {
                return -1;
            }
            cmd_pos = 1;
            continue;
        }

        /* Optional leading single-digit IO_NUMBER before '<' or '>'. */
        int io_fd = -1;
        if (isdigit((unsigned char)p[0]) && (p[1] == '<' || p[1] == '>')) {
            io_fd = p[0] - '0';
            p++;
        }

        if (is_op_start(*p)) {
            sh_tok_t t = {.io_fd = io_fd};
            if (p[0] == '&' && p[1] == '&') {
                t.kind = T_AND;
                p += 2;
                cmd_pos = 1;
            } else if (p[0] == '|' && p[1] == '|') {
                t.kind = T_OR;
                p += 2;
                cmd_pos = 1;
            } else if (p[0] == ';' && p[1] == ';') {
                t.kind = T_DSEMI;
                p += 2;
                cmd_pos = 1;
            } else if (p[0] == ';') {
                t.kind = T_SEMI;
                p++;
                cmd_pos = 1;
            } else if (p[0] == '&') {
                t.kind = T_AMP;
                p++;
                cmd_pos = 1;
            } else if (p[0] == '|') {
                t.kind = T_PIPE;
                p++;
                cmd_pos = 1;
            } else if (p[0] == '(') {
                t.kind = T_LPAREN;
                p++;
                cmd_pos = 1;
            } else if (p[0] == ')') {
                t.kind = T_RPAREN;
                p++;
                cmd_pos = 0;
            } else if (p[0] == '<' && p[1] == '<' && p[2] == '-') {
                t.kind = T_HEREDOC_STRIP;
                p += 3;
                expecting_hd_delim = 1;
                hd_strip = 1;
                hd_tok_idx = out->len;
            } else if (p[0] == '<' && p[1] == '<') {
                t.kind = T_HEREDOC;
                p += 2;
                expecting_hd_delim = 1;
                hd_strip = 0;
                hd_tok_idx = out->len;
            } else if (p[0] == '<' && p[1] == '&') {
                t.kind = T_DUP_IN;
                p += 2;
            } else if (p[0] == '<' && p[1] == '>') {
                t.kind = T_REDIR_RDWR;
                p += 2;
            } else if (p[0] == '<') {
                t.kind = T_REDIR_IN;
                p++;
            } else if (p[0] == '>' && p[1] == '>') {
                t.kind = T_REDIR_APP;
                p += 2;
            } else if (p[0] == '>' && p[1] == '&') {
                t.kind = T_DUP_OUT;
                p += 2;
            } else if (p[0] == '>' && p[1] == '|') {
                t.kind = T_REDIR_CLOBBER;
                p += 2;
            } else {
                t.kind = T_REDIR_OUT;
                p++;
            }
            if (tokvec_push(out, t) != 0) {
                return -1;
            }
            continue;
        }

        sh_buf_t w = {0};
        if (scan_word(&p, &w) != 0) {
            sh_buf_free(&w);
            return -1;
        }
        char *text = w.data ? w.data : strdup("");
        if (!text) {
            return -1;
        }

        if (expecting_hd_delim) {
            expecting_hd_delim = 0;
            if (nhds < 8u) {
                hds[nhds].tok_idx = hd_tok_idx;
                hds[nhds].strip_tabs = hd_strip;
                strip_delim_quotes(text, hds[nhds].delim,
                                   sizeof(hds[nhds].delim), &hds[nhds].quoted);
                nhds++;
            }
            sh_tok_t wt = {.kind = T_WORD, .io_fd = -1, .text = text};
            if (tokvec_push(out, wt) != 0) {
                free(text);
                return -1;
            }
            continue;
        }

        /* Alias substitution in command position (with recursion guard). */
        if (cmd_pos && depth < 8 &&
            (!blocked_alias || strcmp(text, blocked_alias) != 0) &&
            strpbrk(text, "'\"\\$") == NULL) {
            const char *aval = sh_alias_get(text);
            if (aval) {
                size_t alen = strlen(aval);
                int trail_sp = (alen > 0u && (aval[alen - 1u] == ' ' ||
                                              aval[alen - 1u] == '\t'));
                if (sh_lex_internal(aval, out, text, depth + 1) != 0) {
                    free(text);
                    return -1;
                }
                free(text);
                cmd_pos = trail_sp ? 1 : 0;
                continue;
            }
        }

        /* Reserved words that keep the next token in command position. */
        if (cmd_pos &&
            (strcmp(text, "if") == 0 || strcmp(text, "then") == 0 ||
             strcmp(text, "elif") == 0 || strcmp(text, "else") == 0 ||
             strcmp(text, "do") == 0 || strcmp(text, "while") == 0 ||
             strcmp(text, "until") == 0 || strcmp(text, "{") == 0 ||
             strcmp(text, "!") == 0 || sh_is_assignment(text))) {
            cmd_pos = 1;
        } else {
            cmd_pos = 0;
        }

        sh_tok_t wt = {.kind = T_WORD, .io_fd = -1, .text = text};
        if (tokvec_push(out, wt) != 0) {
            free(text);
            return -1;
        }
    }

    if (nhds > 0u) {
        (void)read_pending_heredocs(&p, out, hds, &nhds);
    }
    return 0;
}

int sh_lex(const char *src, sh_tokvec_t *out) {
    memset(out, 0, sizeof(*out));
    if (sh_lex_internal(src, out, NULL, 0) != 0) {
        sh_tokvec_free(out);
        return -1;
    }
    sh_tok_t eof = {.kind = T_EOF, .io_fd = -1};
    if (tokvec_push(out, eof) != 0) {
        sh_tokvec_free(out);
        return -1;
    }
    return 0;
}
