/**
 * @file     lex.c
 * @brief    Lexer for Ring 3 POSIX.1-2024 XCU awk
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "awk.h"

/* need:: COMP_LIBC_0024 */

void lex_init(lexer_t *lx, const char *src) {
    (void)memset(lx, 0, sizeof(*lx));
    lx->src = src ? src : "";
    lx->line = 1;
    lx->prev_kind = TOK_EOF;
}

void lex_free_tok(token_t *t) {
    free(t->text);
    t->text = NULL;
}

static int can_start_regex(tok_kind_t k) {
    switch (k) {
    case TOK_NUM:
    case TOK_STR:
    case TOK_IDENT:
    case TOK_RPAREN:
    case TOK_RBRACK:
    case TOK_INC:
    case TOK_DEC:
        return 0;
    default:
        return 1;
    }
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

char *awk_unescape(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    if (!out) {
        return NULL;
    }
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] != '\\' || i + 1 >= n) {
            out[j++] = s[i];
            continue;
        }
        i++;
        switch (s[i]) {
        case 'a':
            out[j++] = '\a';
            break;
        case 'b':
            out[j++] = '\b';
            break;
        case 'f':
            out[j++] = '\f';
            break;
        case 'n':
            out[j++] = '\n';
            break;
        case 'r':
            out[j++] = '\r';
            break;
        case 't':
            out[j++] = '\t';
            break;
        case 'v':
            out[j++] = '\v';
            break;
        case '\\':
            out[j++] = '\\';
            break;
        case '"':
            out[j++] = '"';
            break;
        case '/':
            out[j++] = '/';
            break;
        case 'x': {
            int v = 0;
            int cnt = 0;
            while (cnt < 2 && i + 1 < n) {
                int hv = hex_val(s[i + 1]);
                if (hv < 0) {
                    break;
                }
                v = (v << 4) | hv;
                i++;
                cnt++;
            }
            out[j++] = (char)(cnt > 0 ? v : 'x');
            break;
        }
        default:
            if (s[i] >= '0' && s[i] <= '7') {
                int v = s[i] - '0';
                for (int k = 0;
                     k < 2 && i + 1 < n && s[i + 1] >= '0' && s[i + 1] <= '7';
                     k++) {
                    v = (v << 3) | (s[++i] - '0');
                }
                out[j++] = (char)v;
            } else {
                out[j++] = s[i];
            }
            break;
        }
    }
    out[j] = '\0';
    return out;
}

static tok_kind_t lookup_kw(const char *s) {
    static const struct {
        const char *name;
        tok_kind_t kind;
    } kws[] = {
        {"BEGIN", TOK_BEGIN},     {"END", TOK_END},
        {"if", TOK_IF},           {"else", TOK_ELSE},
        {"while", TOK_WHILE},     {"do", TOK_DO},
        {"for", TOK_FOR},         {"in", TOK_IN},
        {"break", TOK_BREAK},     {"continue", TOK_CONTINUE},
        {"next", TOK_NEXT},       {"exit", TOK_EXIT},
        {"return", TOK_RETURN},   {"delete", TOK_DELETE},
        {"print", TOK_PRINT},     {"printf", TOK_PRINTF},
        {"getline", TOK_GETLINE}, {"function", TOK_FUNCTION},
    };
    for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
        if (strcmp(s, kws[i].name) == 0) {
            return kws[i].kind;
        }
    }
    return TOK_IDENT;
}

static token_t lex_scan(lexer_t *lx) {
    const char *s = lx->src;
    for (;;) {
        char c = s[lx->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            lx->pos++;
            continue;
        }
        if (c == '\\' && s[lx->pos + 1] == '\n') {
            lx->pos += 2;
            lx->line++;
            continue;
        }
        if (c == '#') {
            while (s[lx->pos] != '\0' && s[lx->pos] != '\n') {
                lx->pos++;
            }
            continue;
        }
        break;
    }

    token_t t = {.kind = TOK_EOF, .line = lx->line};
    char c = s[lx->pos];
    if (c == '\0') {
        return t;
    }
    if (c == '\n') {
        lx->pos++;
        lx->line++;
        t.kind = TOK_NL;
        return t;
    }
    if (c == '"') {
        lx->pos++;
        size_t start = lx->pos;
        while (s[lx->pos] != '\0' && s[lx->pos] != '"') {
            if (s[lx->pos] == '\\' && s[lx->pos + 1] != '\0') {
                lx->pos += 2;
            } else {
                if (s[lx->pos] == '\n') {
                    lx->line++;
                }
                lx->pos++;
            }
        }
        size_t len = lx->pos - start;
        char *raw = calloc(1, len + 1);
        if (raw) {
            if (len > 0) {
                (void)memcpy(raw, s + start, len);
            }
            raw[len] = '\0';
            t.text = awk_unescape(raw);
            free(raw);
        }
        if (s[lx->pos] == '"') {
            lx->pos++;
        } else {
            lx->err = 1;
        }
        t.kind = TOK_STR;
        return t;
    }
    if (c == '/' && can_start_regex(lx->prev_kind)) {
        lx->pos++;
        size_t cap = 64;
        size_t len = 0;
        char *buf = malloc(cap);
        int in_class = 0;
        while (s[lx->pos] != '\0' && (in_class || s[lx->pos] != '/')) {
            if (len + 3 >= cap) {
                size_t ncap = cap * 2;
                char *nb = realloc(buf, ncap);
                if (!nb) {
                    free(buf);
                    lx->err = 1;
                    return t;
                }
                buf = nb;
                cap = ncap;
            }
            if (s[lx->pos] == '\\' && s[lx->pos + 1] != '\0') {
                if (s[lx->pos + 1] == '/') {
                    buf[len++] = '/';
                    lx->pos += 2;
                } else {
                    buf[len++] = s[lx->pos++];
                    buf[len++] = s[lx->pos++];
                }
                continue;
            }
            if (s[lx->pos] == '[' && !in_class) {
                in_class = 1;
            } else if (s[lx->pos] == ']' && in_class) {
                in_class = 0;
            }
            buf[len++] = s[lx->pos++];
        }
        if (buf) {
            buf[len] = '\0';
        }
        if (s[lx->pos] == '/') {
            lx->pos++;
        } else {
            lx->err = 1;
        }
        t.kind = TOK_REGEX;
        t.text = buf;
        return t;
    }
    if (isdigit((unsigned char)c) ||
        (c == '.' && isdigit((unsigned char)s[lx->pos + 1]))) {
        char *end = NULL;
        t.num = strtod(s + lx->pos, &end);
        lx->pos = (size_t)(end - s);
        t.kind = TOK_NUM;
        return t;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        size_t start = lx->pos;
        while (isalnum((unsigned char)s[lx->pos]) || s[lx->pos] == '_') {
            lx->pos++;
        }
        size_t len = lx->pos - start;
        char *id = malloc(len + 1);
        if (id) {
            (void)memcpy(id, s + start, len);
            id[len] = '\0';
        }
        t.kind = id ? lookup_kw(id) : TOK_EOF;
        if (t.kind == TOK_IDENT) {
            t.text = id;
        } else {
            free(id);
        }
        return t;
    }

    char c2 = s[lx->pos + 1];
    lx->pos++;
    switch (c) {
    case ';':
        t.kind = TOK_SEMI;
        return t;
    case '{':
        t.kind = TOK_LBRACE;
        return t;
    case '}':
        t.kind = TOK_RBRACE;
        return t;
    case '(':
        t.kind = TOK_LPAREN;
        return t;
    case ')':
        t.kind = TOK_RPAREN;
        return t;
    case '[':
        t.kind = TOK_LBRACK;
        return t;
    case ']':
        t.kind = TOK_RBRACK;
        return t;
    case ',':
        t.kind = TOK_COMMA;
        return t;
    case '?':
        t.kind = TOK_QUEST;
        return t;
    case ':':
        t.kind = TOK_COLON;
        return t;
    case '$':
        t.kind = TOK_DOLLAR;
        return t;
    case '~':
        t.kind = TOK_MATCH;
        return t;
    case '=':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_EQ;
        } else {
            t.kind = TOK_ASSIGN;
        }
        return t;
    case '!':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_NE;
        } else if (c2 == '~') {
            lx->pos++;
            t.kind = TOK_NOMATCH;
        } else {
            t.kind = TOK_NOT;
        }
        return t;
    case '<':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_LE;
        } else {
            t.kind = TOK_LT;
        }
        return t;
    case '>':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_GE;
        } else if (c2 == '>') {
            lx->pos++;
            t.kind = TOK_APPEND;
        } else {
            t.kind = TOK_GT;
        }
        return t;
    case '|':
        if (c2 == '|') {
            lx->pos++;
            t.kind = TOK_OR;
        } else {
            t.kind = TOK_PIPE;
        }
        return t;
    case '&':
        if (c2 == '&') {
            lx->pos++;
            t.kind = TOK_AND;
            return t;
        }
        lx->err = 1;
        return t;
    case '+':
        if (c2 == '+') {
            lx->pos++;
            t.kind = TOK_INC;
        } else if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_ADD_ASSIGN;
        } else {
            t.kind = TOK_PLUS;
        }
        return t;
    case '-':
        if (c2 == '-') {
            lx->pos++;
            t.kind = TOK_DEC;
        } else if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_SUB_ASSIGN;
        } else {
            t.kind = TOK_MINUS;
        }
        return t;
    case '*':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_MUL_ASSIGN;
        } else if (c2 == '*') {
            lx->pos++;
            if (s[lx->pos] == '=') {
                lx->pos++;
                t.kind = TOK_POW_ASSIGN;
            } else {
                t.kind = TOK_CARET;
            }
        } else {
            t.kind = TOK_STAR;
        }
        return t;
    case '/':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_DIV_ASSIGN;
        } else {
            t.kind = TOK_SLASH;
        }
        return t;
    case '%':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_MOD_ASSIGN;
        } else {
            t.kind = TOK_PERCENT;
        }
        return t;
    case '^':
        if (c2 == '=') {
            lx->pos++;
            t.kind = TOK_POW_ASSIGN;
        } else {
            t.kind = TOK_CARET;
        }
        return t;
    default:
        lx->err = 1;
        return t;
    }
}

token_t *lex_peek(lexer_t *lx) {
    if (!lx->has_cur) {
        lx->cur = lex_scan(lx);
        lx->has_cur = 1;
    }
    return &lx->cur;
}

token_t lex_next(lexer_t *lx) {
    token_t t;
    if (lx->has_cur) {
        t = lx->cur;
        lx->has_cur = 0;
        lx->cur.text = NULL;
    } else {
        t = lex_scan(lx);
    }
    lx->prev_kind = t.kind;
    return t;
}
