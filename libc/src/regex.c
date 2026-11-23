/**
 * @file     regex.c
 * @brief    POSIX.1-2024 BRE and ERE regular expression compiler and matcher
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0014 */

#define RE_MAX_GROUPS 32u
#define RE_MAX_LOOPS 32u
#define RE_DUP_LIMIT 255
#define RE_MAX_STEPS 200000u

enum re_op {
    OP_MATCH = 0,
    OP_CHAR,
    OP_DOT,
    OP_CLASS,
    OP_BOL,
    OP_EOL,
    OP_BACKREF,
    OP_SAVE_SO,
    OP_SAVE_EO,
    OP_JMP,
    OP_SPLIT,
    OP_LOOP_GUARD,
};

struct re_inst {
    uint8_t op;
    uint8_t ch;
    uint16_t arg;
    uint32_t x;
    uint32_t y;
};

struct re_class {
    uint8_t bits[32];
};

struct re_prog {
    struct re_inst *code;
    size_t len;
    size_t cap;
    struct re_class *classes;
    size_t nclasses;
    size_t cap_classes;
    size_t nsub;
    size_t nloops;
    int cflags;
};

static void cls_set(struct re_class *c, unsigned char b) {
    c->bits[b >> 3] |= (uint8_t)(1u << (b & 7u));
}

static int cls_test(const struct re_class *c, unsigned char b) {
    return (c->bits[b >> 3] & (uint8_t)(1u << (b & 7u))) != 0u;
}

static int emit_inst(struct re_prog *p, struct re_inst ins, size_t *idx_out) {
    if (p->len == p->cap) {
        size_t ncap = p->cap ? p->cap * 2u : 64u;
        struct re_inst *nc =
            (struct re_inst *)reallocarray(p->code, ncap, sizeof(*nc));
        if (!nc) {
            return REG_ESPACE;
        }
        p->code = nc;
        p->cap = ncap;
    }
    if (idx_out) {
        *idx_out = p->len;
    }
    p->code[p->len++] = ins;
    return 0;
}

static int add_class(struct re_prog *p, const struct re_class *cls,
                     uint16_t *id_out) {
    if (p->nclasses == p->cap_classes) {
        size_t ncap = p->cap_classes ? p->cap_classes * 2u : 8u;
        struct re_class *nc =
            (struct re_class *)reallocarray(p->classes, ncap, sizeof(*nc));
        if (!nc) {
            return REG_ESPACE;
        }
        p->classes = nc;
        p->cap_classes = ncap;
    }
    *id_out = (uint16_t)p->nclasses;
    p->classes[p->nclasses++] = *cls;
    return 0;
}

/* AST used during parsing so quantifiers can duplicate/wrap atoms cleanly. */
enum ast_kind {
    AST_EMPTY = 0,
    AST_CHAR,
    AST_DOT,
    AST_CLASS,
    AST_BOL,
    AST_EOL,
    AST_BACKREF,
    AST_SEQ,
    AST_ALT,
    AST_REP,
    AST_GROUP,
};

struct ast_node {
    enum ast_kind kind;
    unsigned char ch;
    uint16_t arg;
    int min_rep;
    int max_rep;
    struct ast_node *a;
    struct ast_node *b;
};

struct parser {
    const char *s;
    size_t pos;
    int cflags;
    int err;
    struct re_prog *prog;
    uint32_t closed_groups;
};

static struct ast_node *ast_new(struct parser *ps, enum ast_kind k) {
    struct ast_node *n = (struct ast_node *)calloc(1u, sizeof(*n));
    if (!n) {
        ps->err = REG_ESPACE;
    }
    if (n) {
        n->kind = k;
    }
    return n;
}

static void ast_free(struct ast_node *n) {
    if (!n) {
        return;
    }
    ast_free(n->a);
    ast_free(n->b);
    free(n);
}

static int match_posix_class(const char *name, size_t len, unsigned char c) {
    if (len == 5u && memcmp(name, "alnum", 5u) == 0) {
        return isalnum((int)c);
    }
    if (len == 5u && memcmp(name, "alpha", 5u) == 0) {
        return isalpha((int)c);
    }
    if (len == 5u && memcmp(name, "blank", 5u) == 0) {
        return isblank((int)c);
    }
    if (len == 5u && memcmp(name, "cntrl", 5u) == 0) {
        return iscntrl((int)c);
    }
    if (len == 5u && memcmp(name, "digit", 5u) == 0) {
        return isdigit((int)c);
    }
    if (len == 5u && memcmp(name, "graph", 5u) == 0) {
        return isgraph((int)c);
    }
    if (len == 5u && memcmp(name, "lower", 5u) == 0) {
        return islower((int)c);
    }
    if (len == 5u && memcmp(name, "print", 5u) == 0) {
        return isprint((int)c);
    }
    if (len == 5u && memcmp(name, "punct", 5u) == 0) {
        return ispunct((int)c);
    }
    if (len == 5u && memcmp(name, "space", 5u) == 0) {
        return isspace((int)c);
    }
    if (len == 5u && memcmp(name, "upper", 5u) == 0) {
        return isupper((int)c);
    }
    if (len == 6u && memcmp(name, "xdigit", 6u) == 0) {
        return isxdigit((int)c);
    }
    return -1;
}

static void cls_add_char(struct re_class *cls, unsigned char c, int icase) {
    cls_set(cls, c);
    if (icase) {
        cls_set(cls, (unsigned char)tolower((int)c));
        cls_set(cls, (unsigned char)toupper((int)c));
    }
}

static struct ast_node *parse_bracket(struct parser *ps) {
    ps->pos++; /* skip '[' */
    int negate = 0;
    if (ps->s[ps->pos] == '^') {
        negate = 1;
        ps->pos++;
    }
    struct re_class cls = {{0}};
    int icase = (ps->cflags & REG_ICASE) != 0;
    int first = 1;

    while (ps->s[ps->pos] != '\0') {
        if (ps->s[ps->pos] == ']' && !first) {
            ps->pos++;
            if (negate) {
                for (size_t i = 0u; i < 32u; i++) {
                    cls.bits[i] = (uint8_t)~cls.bits[i];
                }
                cls.bits[0] &= (uint8_t)~1u; /* never match NUL */
                if (ps->cflags & REG_NEWLINE) {
                    cls.bits['\n' >> 3] &= (uint8_t) ~(1u << ('\n' & 7u));
                }
            }
            uint16_t id = 0u;
            int rc = add_class(ps->prog, &cls, &id);
            if (rc != 0) {
                ps->err = rc;
                return NULL;
            }
            struct ast_node *n = ast_new(ps, AST_CLASS);
            if (n) {
                n->arg = id;
            }
            return n;
        }
        first = 0;

        if (ps->s[ps->pos] == '[' && ps->s[ps->pos + 1u] == ':') {
            size_t cs = ps->pos + 2u;
            size_t ce = cs;
            while (ps->s[ce] != '\0' &&
                   !(ps->s[ce] == ':' && ps->s[ce + 1u] == ']')) {
                ce++;
            }
            if (ps->s[ce] != ':') {
                ps->err = REG_EBRACK;
                return NULL;
            }
            if (match_posix_class(ps->s + cs, ce - cs, 'a') < 0) {
                ps->err = REG_ECTYPE;
                return NULL;
            }
            for (unsigned int ch = 1u; ch < 256u; ch++) {
                if (match_posix_class(ps->s + cs, ce - cs, (unsigned char)ch) >
                    0) {
                    cls_add_char(&cls, (unsigned char)ch, icase);
                }
            }
            ps->pos = ce + 2u;
            continue;
        }

        unsigned char lo = (unsigned char)ps->s[ps->pos++];
        if (ps->s[ps->pos] == '-' && ps->s[ps->pos + 1u] != '\0' &&
            ps->s[ps->pos + 1u] != ']') {
            ps->pos++;
            unsigned char hi = (unsigned char)ps->s[ps->pos++];
            if (lo > hi) {
                ps->err = REG_ERANGE;
                return NULL;
            }
            for (unsigned int ch = lo; ch <= hi; ch++) {
                cls_add_char(&cls, (unsigned char)ch, icase);
            }
        } else {
            cls_add_char(&cls, lo, icase);
        }
    }

    ps->err = REG_EBRACK;
    return NULL;
}

static int parse_bound(struct parser *ps, int is_ere, int *min_out,
                       int *max_out) {
    /* On entry: pos points right after '{' (ERE) or '\{' (BRE). */
    const char *p = ps->s + ps->pos;
    if (!isdigit((unsigned char)*p)) {
        ps->err = REG_BADBR;
        return -1;
    }
    int mn = 0;
    while (isdigit((unsigned char)*p)) {
        mn = mn * 10 + (*p++ - '0');
        if (mn > RE_DUP_LIMIT) {
            ps->err = REG_BADBR;
            return -1;
        }
    }
    int mx = mn;
    if (*p == ',') {
        p++;
        if ((!is_ere && p[0] == '\\' && p[1] == '}') ||
            (is_ere && p[0] == '}')) {
            mx = -1;
        } else if (isdigit((unsigned char)*p)) {
            mx = 0;
            while (isdigit((unsigned char)*p)) {
                mx = mx * 10 + (*p++ - '0');
                if (mx > RE_DUP_LIMIT) {
                    ps->err = REG_BADBR;
                    return -1;
                }
            }
            if (mx < mn) {
                ps->err = REG_BADBR;
                return -1;
            }
        } else {
            ps->err = REG_BADBR;
            return -1;
        }
    }
    if (is_ere) {
        if (*p != '}') {
            ps->err = REG_EBRACE;
            return -1;
        }
        p++;
    } else {
        if (p[0] != '\\' || p[1] != '}') {
            ps->err = REG_EBRACE;
            return -1;
        }
        p += 2;
    }
    ps->pos = (size_t)(p - ps->s);
    *min_out = mn;
    *max_out = mx;
    return 0;
}

static struct ast_node *parse_alt(struct parser *ps, int in_group);

static struct ast_node *parse_seq(struct parser *ps, int in_group) {
    int is_ere = (ps->cflags & REG_EXTENDED) != 0;
    struct ast_node *head = NULL;
    struct ast_node *tail = NULL;
    int at_start = 1;

    while (ps->s[ps->pos] != '\0' && ps->err == 0) {
        char c = ps->s[ps->pos];
        if (is_ere && c == '|') {
            break;
        }
        if (is_ere && in_group && c == ')') {
            break;
        }
        if (!is_ere && in_group && c == '\\' && ps->s[ps->pos + 1u] == ')') {
            break;
        }

        struct ast_node *atom = NULL;
        int repeatable = 1;

        if (is_ere) {
            if (c == '*' || c == '+' || c == '?' || c == '{') {
                ps->err = REG_BADRPT;
                break;
            }
            if (c == '^') {
                ps->pos++;
                atom = ast_new(ps, AST_BOL);
                repeatable = 0;
            } else if (c == '$') {
                ps->pos++;
                atom = ast_new(ps, AST_EOL);
                repeatable = 0;
            } else if (c == '.') {
                ps->pos++;
                atom = ast_new(ps, AST_DOT);
            } else if (c == '[') {
                atom = parse_bracket(ps);
            } else if (c == '(') {
                ps->pos++;
                size_t gid = ++ps->prog->nsub;
                struct ast_node *sub = parse_alt(ps, 1);
                if (ps->err != 0) {
                    ast_free(sub);
                    break;
                }
                if (ps->s[ps->pos] != ')') {
                    ast_free(sub);
                    ps->err = REG_EPAREN;
                    break;
                }
                ps->pos++;
                if (gid < RE_MAX_GROUPS) {
                    ps->closed_groups |= (1u << gid);
                }
                atom = ast_new(ps, AST_GROUP);
                if (atom) {
                    atom->arg = (uint16_t)gid;
                    atom->a = sub;
                } else {
                    ast_free(sub);
                }
            } else if (c == '\\') {
                ps->pos++;
                if (ps->s[ps->pos] == '\0') {
                    ps->err = REG_EESCAPE;
                    break;
                }
                atom = ast_new(ps, AST_CHAR);
                if (atom) {
                    atom->ch = (unsigned char)ps->s[ps->pos];
                }
                ps->pos++;
            } else {
                atom = ast_new(ps, AST_CHAR);
                if (atom) {
                    atom->ch = (unsigned char)c;
                }
                ps->pos++;
            }
        } else {
            /* BRE mode. */
            if (c == '^' && at_start) {
                ps->pos++;
                atom = ast_new(ps, AST_BOL);
                repeatable = 0;
            } else if (c == '$' && (ps->s[ps->pos + 1u] == '\0' ||
                                    (in_group && ps->s[ps->pos + 1u] == '\\' &&
                                     ps->s[ps->pos + 2u] == ')'))) {
                ps->pos++;
                atom = ast_new(ps, AST_EOL);
                repeatable = 0;
            } else if (c == '.') {
                ps->pos++;
                atom = ast_new(ps, AST_DOT);
            } else if (c == '[') {
                atom = parse_bracket(ps);
            } else if (c == '\\') {
                char nx = ps->s[ps->pos + 1u];
                if (nx == '\0') {
                    ps->err = REG_EESCAPE;
                    break;
                }
                if (nx == '(') {
                    ps->pos += 2u;
                    size_t gid = ++ps->prog->nsub;
                    struct ast_node *sub = parse_seq(ps, 1);
                    if (ps->err != 0) {
                        ast_free(sub);
                        break;
                    }
                    if (ps->s[ps->pos] != '\\' || ps->s[ps->pos + 1u] != ')') {
                        ast_free(sub);
                        ps->err = REG_EPAREN;
                        break;
                    }
                    ps->pos += 2u;
                    if (gid < RE_MAX_GROUPS) {
                        ps->closed_groups |= (1u << gid);
                    }
                    atom = ast_new(ps, AST_GROUP);
                    if (atom) {
                        atom->arg = (uint16_t)gid;
                        atom->a = sub;
                    } else {
                        ast_free(sub);
                    }
                } else if (nx == ')') {
                    ps->err = REG_EPAREN;
                    break;
                } else if (nx >= '1' && nx <= '9') {
                    unsigned int ref = (unsigned int)(nx - '0');
                    if (ref >= RE_MAX_GROUPS ||
                        (ps->closed_groups & (1u << ref)) == 0u) {
                        ps->err = REG_ESUBREG;
                        break;
                    }
                    ps->pos += 2u;
                    atom = ast_new(ps, AST_BACKREF);
                    if (atom) {
                        atom->arg = (uint16_t)ref;
                    }
                } else if (nx == '{') {
                    ps->err = REG_BADRPT;
                    break;
                } else {
                    ps->pos += 2u;
                    atom = ast_new(ps, AST_CHAR);
                    if (atom) {
                        atom->ch = (unsigned char)nx;
                    }
                }
            } else {
                /* In BRE, '*' at start of pattern or after '^' is literal. */
                atom = ast_new(ps, AST_CHAR);
                if (atom) {
                    atom->ch = (unsigned char)c;
                }
                ps->pos++;
            }
        }

        if (ps->err != 0 || !atom) {
            ast_free(atom);
            break;
        }
        at_start = 0;

        /* Attach postfix quantifiers. */
        if (repeatable) {
            for (;;) {
                char q = ps->s[ps->pos];
                int mn = 0;
                int mx = 0;
                if (q == '*') {
                    ps->pos++;
                    mn = 0;
                    mx = -1;
                } else if (is_ere && q == '+') {
                    ps->pos++;
                    mn = 1;
                    mx = -1;
                } else if (is_ere && q == '?') {
                    ps->pos++;
                    mn = 0;
                    mx = 1;
                } else if (is_ere && q == '{') {
                    ps->pos++;
                    if (parse_bound(ps, 1, &mn, &mx) != 0) {
                        break;
                    }
                } else if (!is_ere && q == '\\' && ps->s[ps->pos + 1u] == '{') {
                    ps->pos += 2u;
                    if (parse_bound(ps, 0, &mn, &mx) != 0) {
                        break;
                    }
                } else {
                    break;
                }
                struct ast_node *rep = ast_new(ps, AST_REP);
                if (!rep) {
                    break;
                }
                rep->min_rep = mn;
                rep->max_rep = mx;
                rep->a = atom;
                atom = rep;
                if (!is_ere) {
                    break;
                }
            }
        }
        if (ps->err != 0) {
            ast_free(atom);
            break;
        }

        if (!head) {
            head = atom;
            tail = atom;
        } else {
            struct ast_node *seq = ast_new(ps, AST_SEQ);
            if (!seq) {
                ast_free(atom);
                break;
            }
            seq->a = head;
            seq->b = atom;
            head = seq;
            tail = atom;
            (void)tail;
        }
    }

    if (ps->err != 0) {
        ast_free(head);
        return NULL;
    }
    return head ? head : ast_new(ps, AST_EMPTY);
}

static struct ast_node *parse_alt(struct parser *ps, int in_group) {
    struct ast_node *left = parse_seq(ps, in_group);
    if (ps->err != 0) {
        return NULL;
    }
    while ((ps->cflags & REG_EXTENDED) && ps->s[ps->pos] == '|') {
        ps->pos++;
        struct ast_node *right = parse_seq(ps, in_group);
        if (ps->err != 0) {
            ast_free(left);
            return NULL;
        }
        struct ast_node *alt = ast_new(ps, AST_ALT);
        if (!alt) {
            ast_free(left);
            ast_free(right);
            return NULL;
        }
        alt->a = left;
        alt->b = right;
        left = alt;
    }
    return left;
}

static int compile_node(struct re_prog *p, const struct ast_node *n) {
    if (!n) {
        return 0;
    }
    struct re_inst ins = {0};
    switch (n->kind) {
    case AST_EMPTY:
        return 0;
    case AST_CHAR:
        ins.op = OP_CHAR;
        ins.ch = n->ch;
        return emit_inst(p, ins, NULL);
    case AST_DOT:
        ins.op = OP_DOT;
        return emit_inst(p, ins, NULL);
    case AST_CLASS:
        ins.op = OP_CLASS;
        ins.arg = n->arg;
        return emit_inst(p, ins, NULL);
    case AST_BOL:
        ins.op = OP_BOL;
        return emit_inst(p, ins, NULL);
    case AST_EOL:
        ins.op = OP_EOL;
        return emit_inst(p, ins, NULL);
    case AST_BACKREF:
        ins.op = OP_BACKREF;
        ins.arg = n->arg;
        return emit_inst(p, ins, NULL);
    case AST_SEQ: {
        int rc = compile_node(p, n->a);
        if (rc != 0) {
            return rc;
        }
        return compile_node(p, n->b);
    }
    case AST_GROUP: {
        int save = n->arg < RE_MAX_GROUPS;
        if (save) {
            ins.op = OP_SAVE_SO;
            ins.arg = n->arg;
            int rc = emit_inst(p, ins, NULL);
            if (rc != 0) {
                return rc;
            }
        }
        int rc = compile_node(p, n->a);
        if (rc != 0) {
            return rc;
        }
        if (save) {
            ins.op = OP_SAVE_EO;
            ins.arg = n->arg;
            return emit_inst(p, ins, NULL);
        }
        return 0;
    }
    case AST_ALT: {
        size_t split_pc = 0u;
        ins.op = OP_SPLIT;
        int rc = emit_inst(p, ins, &split_pc);
        if (rc != 0) {
            return rc;
        }
        p->code[split_pc].x = (uint32_t)p->len;
        rc = compile_node(p, n->a);
        if (rc != 0) {
            return rc;
        }
        size_t jmp_pc = 0u;
        ins.op = OP_JMP;
        rc = emit_inst(p, ins, &jmp_pc);
        if (rc != 0) {
            return rc;
        }
        p->code[split_pc].y = (uint32_t)p->len;
        rc = compile_node(p, n->b);
        if (rc != 0) {
            return rc;
        }
        p->code[jmp_pc].x = (uint32_t)p->len;
        return 0;
    }
    case AST_REP: {
        for (int i = 0; i < n->min_rep; i++) {
            int rc = compile_node(p, n->a);
            if (rc != 0) {
                return rc;
            }
        }
        if (n->max_rep < 0) {
            size_t loop_id = p->nloops < RE_MAX_LOOPS ? p->nloops++ : 0u;
            size_t split_pc = 0u;
            ins.op = OP_SPLIT;
            int rc = emit_inst(p, ins, &split_pc);
            if (rc != 0) {
                return rc;
            }
            p->code[split_pc].x = (uint32_t)p->len;
            struct re_inst g = {0};
            g.op = OP_LOOP_GUARD;
            g.arg = (uint16_t)loop_id;
            rc = emit_inst(p, g, NULL);
            if (rc != 0) {
                return rc;
            }
            rc = compile_node(p, n->a);
            if (rc != 0) {
                return rc;
            }
            struct re_inst j = {0};
            j.op = OP_JMP;
            j.x = (uint32_t)split_pc;
            rc = emit_inst(p, j, NULL);
            if (rc != 0) {
                return rc;
            }
            p->code[split_pc].y = (uint32_t)p->len;
            return 0;
        }
        for (int i = n->min_rep; i < n->max_rep; i++) {
            size_t split_pc = 0u;
            ins.op = OP_SPLIT;
            int rc = emit_inst(p, ins, &split_pc);
            if (rc != 0) {
                return rc;
            }
            p->code[split_pc].x = (uint32_t)p->len;
            rc = compile_node(p, n->a);
            if (rc != 0) {
                return rc;
            }
            p->code[split_pc].y = (uint32_t)p->len;
        }
        return 0;
    }
    }
    return 0;
}

int regcomp(regex_t *restrict preg, const char *restrict pattern, int cflags) {
    if (!preg || !pattern) {
        return REG_BADPAT;
    }
    preg->re_nsub = 0u;
    preg->__impl = NULL;

    struct re_prog *prog = (struct re_prog *)calloc(1u, sizeof(*prog));
    if (!prog) {
        return REG_ESPACE;
    }
    prog->cflags = cflags;

    struct parser ps = {
        .s = pattern,
        .pos = 0u,
        .cflags = cflags,
        .err = 0,
        .prog = prog,
        .closed_groups = 0u,
    };
    struct ast_node *root = parse_alt(&ps, 0);
    if (ps.err == 0 && pattern[ps.pos] != '\0') {
        ps.err = (pattern[ps.pos] == ')') ? REG_EPAREN : REG_BADPAT;
    }
    if (ps.err != 0) {
        ast_free(root);
        free(prog->code);
        free(prog->classes);
        free(prog);
        return ps.err;
    }

    int rc = compile_node(prog, root);
    ast_free(root);
    if (rc == 0) {
        struct re_inst fin = {.op = OP_MATCH};
        rc = emit_inst(prog, fin, NULL);
    }
    if (rc != 0) {
        free(prog->code);
        free(prog->classes);
        free(prog);
        return rc;
    }

    preg->re_nsub = prog->nsub;
    preg->__impl = prog;
    return 0;
}

struct exec_ctx {
    const struct re_prog *prog;
    const char *str;
    size_t slen;
    int eflags;
    size_t steps;
    regoff_t cur_so[RE_MAX_GROUPS];
    regoff_t cur_eo[RE_MAX_GROUPS];
    regoff_t loop_pos[RE_MAX_LOOPS];
    int best_found;
    size_t best_end;
    regoff_t best_so[RE_MAX_GROUPS];
    regoff_t best_eo[RE_MAX_GROUPS];
};

static int chars_eq(unsigned char a, unsigned char b, int icase) {
    if (a == b) {
        return 1;
    }
    if (!icase) {
        return 0;
    }
    return tolower((int)a) == tolower((int)b);
}

static void vm_run(struct exec_ctx *ctx, size_t pc, size_t pos) {
    const struct re_prog *p = ctx->prog;
    int icase = (p->cflags & REG_ICASE) != 0;
    int nl = (p->cflags & REG_NEWLINE) != 0;

    while (pc < p->len) {
        if (++ctx->steps > RE_MAX_STEPS) {
            return;
        }
        const struct re_inst *ins = &p->code[pc];
        switch ((enum re_op)ins->op) {
        case OP_MATCH:
            if (!ctx->best_found || pos > ctx->best_end) {
                ctx->best_found = 1;
                ctx->best_end = pos;
                memcpy(ctx->best_so, ctx->cur_so, sizeof(ctx->best_so));
                memcpy(ctx->best_eo, ctx->cur_eo, sizeof(ctx->best_eo));
            }
            return;
        case OP_CHAR:
            if (pos >= ctx->slen ||
                !chars_eq((unsigned char)ctx->str[pos], ins->ch, icase)) {
                return;
            }
            pos++;
            pc++;
            break;
        case OP_DOT:
            if (pos >= ctx->slen || (nl && ctx->str[pos] == '\n')) {
                return;
            }
            pos++;
            pc++;
            break;
        case OP_CLASS:
            if (pos >= ctx->slen || !cls_test(&p->classes[ins->arg],
                                              (unsigned char)ctx->str[pos])) {
                return;
            }
            pos++;
            pc++;
            break;
        case OP_BOL: {
            int ok = 0;
            if (pos == 0u && !(ctx->eflags & REG_NOTBOL)) {
                ok = 1;
            } else if (nl && pos > 0u && ctx->str[pos - 1u] == '\n') {
                ok = 1;
            }
            if (!ok) {
                return;
            }
            pc++;
            break;
        }
        case OP_EOL: {
            int ok = 0;
            if (pos == ctx->slen && !(ctx->eflags & REG_NOTEOL)) {
                ok = 1;
            } else if (nl && pos < ctx->slen && ctx->str[pos] == '\n') {
                ok = 1;
            }
            if (!ok) {
                return;
            }
            pc++;
            break;
        }
        case OP_BACKREF: {
            uint16_t gid = ins->arg;
            if (gid >= RE_MAX_GROUPS || ctx->cur_so[gid] < 0 ||
                ctx->cur_eo[gid] < ctx->cur_so[gid]) {
                return;
            }
            size_t rlen = (size_t)(ctx->cur_eo[gid] - ctx->cur_so[gid]);
            if (pos + rlen > ctx->slen) {
                return;
            }
            for (size_t i = 0u; i < rlen; i++) {
                if (!chars_eq(
                        (unsigned char)ctx->str[pos + i],
                        (unsigned char)ctx->str[(size_t)ctx->cur_so[gid] + i],
                        icase)) {
                    return;
                }
            }
            pos += rlen;
            pc++;
            break;
        }
        case OP_SAVE_SO: {
            uint16_t gid = ins->arg;
            regoff_t prev = ctx->cur_so[gid];
            ctx->cur_so[gid] = (regoff_t)pos;
            vm_run(ctx, pc + 1u, pos);
            ctx->cur_so[gid] = prev;
            return;
        }
        case OP_SAVE_EO: {
            uint16_t gid = ins->arg;
            regoff_t prev = ctx->cur_eo[gid];
            ctx->cur_eo[gid] = (regoff_t)pos;
            vm_run(ctx, pc + 1u, pos);
            ctx->cur_eo[gid] = prev;
            return;
        }
        case OP_JMP:
            pc = ins->x;
            break;
        case OP_LOOP_GUARD: {
            uint16_t lid = ins->arg;
            regoff_t prev = ctx->loop_pos[lid];
            if (prev == (regoff_t)pos) {
                return;
            }
            ctx->loop_pos[lid] = (regoff_t)pos;
            vm_run(ctx, pc + 1u, pos);
            ctx->loop_pos[lid] = prev;
            return;
        }
        case OP_SPLIT:
            vm_run(ctx, ins->x, pos);
            if (ctx->best_found &&
                ((p->cflags & REG_NOSUB) || ctx->best_end == ctx->slen)) {
                return;
            }
            pc = ins->y;
            break;
        }
    }
}

int regexec(const regex_t *restrict preg, const char *restrict string,
            size_t nmatch, regmatch_t pmatch[restrict], int eflags) {
    if (!preg || !preg->__impl || !string) {
        return REG_NOMATCH;
    }
    const struct re_prog *prog = (const struct re_prog *)preg->__impl;
    size_t slen = strlen(string);

    struct exec_ctx ctx = {0};
    ctx.prog = prog;
    ctx.str = string;
    ctx.slen = slen;
    ctx.eflags = eflags;

    for (size_t start = 0u; start <= slen; start++) {
        ctx.steps = 0u;
        ctx.best_found = 0;
        ctx.best_end = 0u;
        for (size_t i = 0u; i < RE_MAX_GROUPS; i++) {
            ctx.cur_so[i] = -1;
            ctx.cur_eo[i] = -1;
            ctx.best_so[i] = -1;
            ctx.best_eo[i] = -1;
        }
        for (size_t i = 0u; i < RE_MAX_LOOPS; i++) {
            ctx.loop_pos[i] = -1;
        }
        ctx.cur_so[0] = (regoff_t)start;
        vm_run(&ctx, 0u, start);
        if (ctx.best_found) {
            ctx.best_so[0] = (regoff_t)start;
            ctx.best_eo[0] = (regoff_t)ctx.best_end;
            if (!(prog->cflags & REG_NOSUB) && pmatch && nmatch > 0u) {
                for (size_t i = 0u; i < nmatch; i++) {
                    if (i < RE_MAX_GROUPS) {
                        pmatch[i].rm_so = ctx.best_so[i];
                        pmatch[i].rm_eo = ctx.best_eo[i];
                    } else {
                        pmatch[i].rm_so = -1;
                        pmatch[i].rm_eo = -1;
                    }
                }
            }
            return 0;
        }
    }
    return REG_NOMATCH;
}

size_t regerror(int errcode, const regex_t *restrict preg,
                char *restrict errbuf, size_t errbuf_size) {
    (void)preg;
    const char *msg = "Unknown regular expression error";
    switch (errcode) {
    case 0:
        msg = "Success";
        break;
    case REG_NOMATCH:
        msg = "No match";
        break;
    case REG_BADPAT:
        msg = "Invalid regular expression";
        break;
    case REG_ECOLLATE:
        msg = "Invalid collating element";
        break;
    case REG_ECTYPE:
        msg = "Invalid character class";
        break;
    case REG_EESCAPE:
        msg = "Trailing backslash";
        break;
    case REG_ESUBREG:
        msg = "Invalid back reference";
        break;
    case REG_EBRACK:
        msg = "Unmatched [ or [^";
        break;
    case REG_EPAREN:
        msg = "Unmatched ( or )";
        break;
    case REG_EBRACE:
        msg = "Unmatched { or }";
        break;
    case REG_BADBR:
        msg = "Invalid content of {}";
        break;
    case REG_ERANGE:
        msg = "Invalid range end";
        break;
    case REG_ESPACE:
        msg = "Memory exhausted";
        break;
    case REG_BADRPT:
        msg = "Invalid preceding regular expression";
        break;
    default:
        break;
    }
    size_t need = strlen(msg) + 1u;
    if (errbuf && errbuf_size > 0u) {
        size_t copy =
            need - 1u < errbuf_size - 1u ? need - 1u : errbuf_size - 1u;
        if (copy > 0u) {
            memcpy(errbuf, msg, copy);
        }
        errbuf[copy] = '\0';
    }
    return need;
}

void regfree(regex_t *preg) {
    if (!preg || !preg->__impl) {
        return;
    }
    struct re_prog *prog = (struct re_prog *)preg->__impl;
    free(prog->code);
    free(prog->classes);
    free(prog);
    preg->__impl = NULL;
    preg->re_nsub = 0u;
}
