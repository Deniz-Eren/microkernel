/**
 * @file     main.c
 * @brief    Ring 3 command shell (sh) targeting POSIX.1-2024 XCU interfaces
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: COMP_LIBC_0005 COMP_PROC_0010 */

#define SH_MAX_TOKENS 128
#define SH_MAX_WORD 512
#define SH_MAX_ARGS 32
#define SH_MAX_REDIRS 8
#define SH_MAX_STAGES 8
#define SH_MAX_VARS 64
#define SH_MAX_LINE 1024

typedef enum {
    TOK_WORD = 0,
    TOK_PIPE,
    TOK_AND,
    TOK_OR,
    TOK_SEMI,
    TOK_NL,
    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_APP,
    TOK_REDIR_ERR,
    TOK_REDIR_ERR_APP,
    TOK_DUP_ERR_TO_OUT,
    TOK_DUP_OUT_TO_ERR,
    TOK_EOF
} tok_kind_t;

typedef struct {
    tok_kind_t kind;
    char text[SH_MAX_WORD];
} token_t;

typedef struct {
    tok_kind_t kind;
    char target[PATH_MAX];
} redir_t;

typedef struct {
    int argc;
    char *argv[SH_MAX_ARGS];
    int nredirs;
    redir_t redirs[SH_MAX_REDIRS];
} stage_t;

typedef struct {
    int used;
    int exported;
    char name[64];
    char val[SH_MAX_WORD];
} sh_var_t;

static sh_var_t g_vars[SH_MAX_VARS];
static token_t g_tokens[SH_MAX_TOKENS];
static stage_t g_stages[SH_MAX_STAGES];
static int g_last_status = 0;
static int g_pipefail = 0;
static int g_errexit = 0;
static int g_should_exit = 0;
static int g_exit_code = 0;
static int g_interactive = 0;
static pid_t g_shell_pgid = 0;

static const char *var_get(const char *name) {
    if (!name || !name[0]) {
        return NULL;
    }
    for (int i = 0; i < SH_MAX_VARS; i++) {
        if (g_vars[i].used && strcmp(g_vars[i].name, name) == 0) {
            return g_vars[i].val;
        }
    }
    return getenv(name);
}

static void var_set(const char *name, const char *val, int do_export) {
    if (!name || !name[0]) {
        return;
    }
    int slot = -1;
    for (int i = 0; i < SH_MAX_VARS; i++) {
        if (g_vars[i].used && strcmp(g_vars[i].name, name) == 0) {
            slot = i;
            break;
        }
        if (!g_vars[i].used && slot < 0) {
            slot = i;
        }
    }
    if (slot < 0) {
        return;
    }
    if (!g_vars[slot].used) {
        g_vars[slot].used = 1;
        g_vars[slot].exported = 0;
        (void)strlcpy(g_vars[slot].name, name, sizeof(g_vars[slot].name));
    }
    if (val) {
        (void)strlcpy(g_vars[slot].val, val, sizeof(g_vars[slot].val));
    }
    if (do_export) {
        g_vars[slot].exported = 1;
    }
    if (g_vars[slot].exported) {
        (void)setenv(g_vars[slot].name, g_vars[slot].val, 1);
    }
}

static void var_unset(const char *name) {
    if (!name || !name[0]) {
        return;
    }
    for (int i = 0; i < SH_MAX_VARS; i++) {
        if (g_vars[i].used && strcmp(g_vars[i].name, name) == 0) {
            g_vars[i].used = 0;
            g_vars[i].exported = 0;
            g_vars[i].name[0] = '\0';
            g_vars[i].val[0] = '\0';
        }
    }
    (void)unsetenv(name);
}

static int is_valid_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_valid_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int is_assignment_word(const char *w) {
    if (!w || !is_valid_name_start(w[0])) {
        return 0;
    }
    for (size_t i = 1; w[i]; i++) {
        if (w[i] == '=') {
            return 1;
        }
        if (!is_valid_name_char(w[i])) {
            return 0;
        }
    }
    return 0;
}

static void append_char(char *buf, size_t *len, size_t cap, char c) {
    if (*len + 1u < cap) {
        buf[(*len)++] = c;
        buf[*len] = '\0';
    }
}

static void append_str(char *buf, size_t *len, size_t cap, const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        append_char(buf, len, cap, *s++);
    }
}

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

static void append_utf8(char *buf, size_t *len, size_t cap, unsigned int cp) {
    if (cp <= 0x7Fu) {
        append_char(buf, len, cap, (char)cp);
    } else if (cp <= 0x7FFu) {
        append_char(buf, len, cap, (char)(0xC0u | (cp >> 6)));
        append_char(buf, len, cap, (char)(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        append_char(buf, len, cap, (char)(0xE0u | (cp >> 12)));
        append_char(buf, len, cap, (char)(0x80u | ((cp >> 6) & 0x3Fu)));
        append_char(buf, len, cap, (char)(0x80u | (cp & 0x3Fu)));
    }
}

/* POSIX.1-2024 Issue 8 ANSI-C $'...' quoting parser. */
static const char *parse_ansi_c_quote(const char *p, char *out, size_t *len,
                                      size_t cap) {
    /* p points past "$'". */
    while (*p && *p != '\'') {
        if (*p != '\\') {
            append_char(out, len, cap, *p++);
            continue;
        }
        p++;
        if (!*p) {
            break;
        }
        switch (*p) {
        case 'a':
            append_char(out, len, cap, '\a');
            p++;
            break;
        case 'b':
            append_char(out, len, cap, '\b');
            p++;
            break;
        case 'e':
        case 'E':
            append_char(out, len, cap, 0x1B);
            p++;
            break;
        case 'f':
            append_char(out, len, cap, '\f');
            p++;
            break;
        case 'n':
            append_char(out, len, cap, '\n');
            p++;
            break;
        case 'r':
            append_char(out, len, cap, '\r');
            p++;
            break;
        case 't':
            append_char(out, len, cap, '\t');
            p++;
            break;
        case 'v':
            append_char(out, len, cap, '\v');
            p++;
            break;
        case '\\':
        case '\'':
        case '"':
        case '?':
            append_char(out, len, cap, *p++);
            break;
        case 'x': {
            p++;
            unsigned int val = 0;
            int digits = 0;
            while (digits < 2 && hex_val(*p) >= 0) {
                val = (val << 4) | (unsigned int)hex_val(*p++);
                digits++;
            }
            if (digits > 0) {
                append_char(out, len, cap, (char)val);
            }
            break;
        }
        case 'u': {
            p++;
            unsigned int cp = 0;
            int digits = 0;
            while (digits < 4 && hex_val(*p) >= 0) {
                cp = (cp << 4) | (unsigned int)hex_val(*p++);
                digits++;
            }
            if (digits > 0) {
                append_utf8(out, len, cap, cp);
            }
            break;
        }
        default:
            if (*p >= '0' && *p <= '7') {
                unsigned int val = 0;
                int digits = 0;
                while (digits < 3 && *p >= '0' && *p <= '7') {
                    val = (val << 3) | (unsigned int)(*p++ - '0');
                    digits++;
                }
                append_char(out, len, cap, (char)(val & 0xFFu));
            } else {
                append_char(out, len, cap, '\\');
                append_char(out, len, cap, *p++);
            }
            break;
        }
    }
    if (*p == '\'') {
        p++;
    }
    return p;
}

static const char *expand_dollar(const char *p, char *out, size_t *len,
                                 size_t cap) {
    /* p points right after '$'. */
    if (*p == '?') {
        char num[16];
        (void)snprintf(num, sizeof(num), "%d", g_last_status);
        append_str(out, len, cap, num);
        return p + 1;
    }
    if (*p == '$') {
        char num[16];
        (void)snprintf(num, sizeof(num), "%ld", (long)getpid());
        append_str(out, len, cap, num);
        return p + 1;
    }
    if (*p == '{') {
        p++;
        char name[64];
        size_t nl = 0;
        while (*p && *p != '}') {
            if (nl + 1u < sizeof(name)) {
                name[nl++] = *p;
            }
            p++;
        }
        name[nl] = '\0';
        if (*p == '}') {
            p++;
        }
        if (strcmp(name, "?") == 0) {
            char num[16];
            (void)snprintf(num, sizeof(num), "%d", g_last_status);
            append_str(out, len, cap, num);
        } else if (strcmp(name, "$") == 0) {
            char num[16];
            (void)snprintf(num, sizeof(num), "%ld", (long)getpid());
            append_str(out, len, cap, num);
        } else {
            append_str(out, len, cap, var_get(name));
        }
        return p;
    }
    if (is_valid_name_start(*p)) {
        char name[64];
        size_t nl = 0;
        while (is_valid_name_char(*p)) {
            if (nl + 1u < sizeof(name)) {
                name[nl++] = *p;
            }
            p++;
        }
        name[nl] = '\0';
        append_str(out, len, cap, var_get(name));
        return p;
    }
    append_char(out, len, cap, '$');
    return p;
}

static int is_op_char(char c) {
    return c == '|' || c == '&' || c == ';' || c == '<' || c == '>' ||
           c == '\n';
}

/* Parse one pipeline (stopping before &&, ||, ;, \n, or EOF) with live
 * variable expansion so earlier commands in the same script affect $VAR/$?. */
static int next_pipeline_tokens(const char **pp, tok_kind_t *sep_out) {
    int ntok = 0;
    const char *p = *pp;
    *sep_out = TOK_EOF;

    while (*p && ntok < SH_MAX_TOKENS - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (!*p) {
            break;
        }
        if (*p == '#') {
            while (*p && *p != '\n') {
                p++;
            }
            continue;
        }
        if (*p == '\n') {
            *sep_out = TOK_NL;
            p++;
            break;
        }
        if (p[0] == '&' && p[1] == '&') {
            *sep_out = TOK_AND;
            p += 2;
            break;
        }
        if (p[0] == '|' && p[1] == '|') {
            *sep_out = TOK_OR;
            p += 2;
            break;
        }
        if (p[0] == ';') {
            *sep_out = TOK_SEMI;
            p++;
            break;
        }
        if (p[0] == '|') {
            g_tokens[ntok++].kind = TOK_PIPE;
            p++;
            continue;
        }
        if (strncmp(p, "2>&1", 4) == 0 &&
            (!p[4] || isspace((unsigned char)p[4]) || is_op_char(p[4]))) {
            g_tokens[ntok++].kind = TOK_DUP_ERR_TO_OUT;
            p += 4;
            continue;
        }
        if (strncmp(p, "1>&2", 4) == 0 &&
            (!p[4] || isspace((unsigned char)p[4]) || is_op_char(p[4]))) {
            g_tokens[ntok++].kind = TOK_DUP_OUT_TO_ERR;
            p += 4;
            continue;
        }
        if (strncmp(p, ">&2", 3) == 0 &&
            (!p[3] || isspace((unsigned char)p[3]) || is_op_char(p[3]))) {
            g_tokens[ntok++].kind = TOK_DUP_OUT_TO_ERR;
            p += 3;
            continue;
        }
        if (p[0] == '2' && p[1] == '>' && p[2] == '>') {
            g_tokens[ntok++].kind = TOK_REDIR_ERR_APP;
            p += 3;
            continue;
        }
        if (p[0] == '2' && p[1] == '>') {
            g_tokens[ntok++].kind = TOK_REDIR_ERR;
            p += 2;
            continue;
        }
        if (p[0] == '>' && p[1] == '>') {
            g_tokens[ntok++].kind = TOK_REDIR_APP;
            p += 2;
            continue;
        }
        if (p[0] == '>') {
            g_tokens[ntok++].kind = TOK_REDIR_OUT;
            p++;
            continue;
        }
        if (p[0] == '<') {
            g_tokens[ntok++].kind = TOK_REDIR_IN;
            p++;
            continue;
        }

        /* Word token with single, double, backslash, and $'...' quoting. */
        token_t *tok = &g_tokens[ntok];
        tok->kind = TOK_WORD;
        tok->text[0] = '\0';
        size_t len = 0;

        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && !is_op_char(*p)) {
            if (p[0] == '$' && p[1] == '\'') {
                p = parse_ansi_c_quote(p + 2, tok->text, &len,
                                       sizeof(tok->text));
            } else if (*p == '\'') {
                p++;
                while (*p && *p != '\'') {
                    append_char(tok->text, &len, sizeof(tok->text), *p++);
                }
                if (*p == '\'') {
                    p++;
                }
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (p[1] == '"' || p[1] == '\\' ||
                                       p[1] == '$' || p[1] == '`')) {
                        p++;
                        append_char(tok->text, &len, sizeof(tok->text), *p++);
                    } else if (*p == '$') {
                        p = expand_dollar(p + 1, tok->text, &len,
                                          sizeof(tok->text));
                    } else {
                        append_char(tok->text, &len, sizeof(tok->text), *p++);
                    }
                }
                if (*p == '"') {
                    p++;
                }
            } else if (*p == '\\' && p[1]) {
                p++;
                append_char(tok->text, &len, sizeof(tok->text), *p++);
            } else if (*p == '$') {
                p = expand_dollar(p + 1, tok->text, &len, sizeof(tok->text));
            } else {
                append_char(tok->text, &len, sizeof(tok->text), *p++);
            }
        }
        ntok++;
    }

    *pp = p;
    g_tokens[ntok].kind = TOK_EOF;
    g_tokens[ntok].text[0] = '\0';
    return ntok;
}

static int apply_redirs(const stage_t *st) {
    for (int i = 0; i < st->nredirs; i++) {
        const redir_t *r = &st->redirs[i];
        int fd = -1;
        switch (r->kind) {
        case TOK_REDIR_IN:
            fd = open(r->target, O_RDONLY);
            if (fd < 0 || (fd != STDIN_FILENO && dup2(fd, STDIN_FILENO) < 0)) {
                perror(r->target);
                if (fd >= 0) {
                    (void)close(fd);
                }
                return -1;
            }
            if (fd != STDIN_FILENO) {
                (void)close(fd);
            }
            break;
        case TOK_REDIR_OUT:
        case TOK_REDIR_APP: {
            int fl = O_WRONLY | O_CREAT |
                     (r->kind == TOK_REDIR_APP ? O_APPEND : O_TRUNC);
            fd = open(r->target, fl, (mode_t)0666u);
            if (fd < 0 ||
                (fd != STDOUT_FILENO && dup2(fd, STDOUT_FILENO) < 0)) {
                perror(r->target);
                if (fd >= 0) {
                    (void)close(fd);
                }
                return -1;
            }
            if (fd != STDOUT_FILENO) {
                (void)close(fd);
            }
            break;
        }
        case TOK_REDIR_ERR:
        case TOK_REDIR_ERR_APP: {
            int fl = O_WRONLY | O_CREAT |
                     (r->kind == TOK_REDIR_ERR_APP ? O_APPEND : O_TRUNC);
            fd = open(r->target, fl, (mode_t)0666u);
            if (fd < 0 ||
                (fd != STDERR_FILENO && dup2(fd, STDERR_FILENO) < 0)) {
                perror(r->target);
                if (fd >= 0) {
                    (void)close(fd);
                }
                return -1;
            }
            if (fd != STDERR_FILENO) {
                (void)close(fd);
            }
            break;
        }
        case TOK_DUP_ERR_TO_OUT:
            if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
                return -1;
            }
            break;
        case TOK_DUP_OUT_TO_ERR:
            if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
                return -1;
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

static int builtin_read(int argc, char **argv) {
    int raw = 0;
    char delim = '\n';
    int i = 1;

    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        }
        if (strcmp(argv[i], "-r") == 0) {
            raw = 1;
            i++;
        } else if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "read: -d requires an argument\n");
                return 2;
            }
            /* POSIX.1-2024: empty -d '' terminates on NUL byte '\0'. */
            delim = argv[i + 1][0];
            i += 2;
        } else {
            fprintf(stderr, "read: unknown option %s\n", argv[i]);
            return 2;
        }
    }

    char line[SH_MAX_WORD];
    size_t len = 0;
    int got_delim = 0;

    for (;;) {
        char ch = 0;
        ssize_t r = read(STDIN_FILENO, &ch, 1);
        if (r <= 0) {
            break;
        }
        if (ch == delim) {
            got_delim = 1;
            break;
        }
        if (!raw && ch == '\\') {
            char next = 0;
            ssize_t r2 = read(STDIN_FILENO, &next, 1);
            if (r2 <= 0) {
                break;
            }
            if (next == '\n') {
                continue;
            }
            if (len + 1u < sizeof(line)) {
                line[len++] = next;
            }
            continue;
        }
        if (len + 1u < sizeof(line)) {
            line[len++] = ch;
        }
    }
    line[len] = '\0';

    int nvars = argc - i;
    if (nvars <= 0) {
        var_set("REPLY", line, 0);
        return got_delim ? 0 : 1;
    }

    const char *ifs = var_get("IFS");
    if (!ifs) {
        ifs = " \t\n";
    }
    char *p = line;
    for (int v = 0; v < nvars; v++) {
        while (*p && *p != delim && strchr(ifs, (unsigned char)*p) != NULL &&
               isspace((unsigned char)*p)) {
            p++;
        }
        if (v == nvars - 1) {
            /* Trim trailing IFS whitespace for the final variable. */
            size_t rem = strlen(p);
            while (rem > 0u && p[rem - 1u] != delim &&
                   strchr(ifs, (unsigned char)p[rem - 1u]) != NULL &&
                   isspace((unsigned char)p[rem - 1u])) {
                p[--rem] = '\0';
            }
            var_set(argv[i + v], p, 0);
        } else {
            char *start = p;
            while (*p && strchr(ifs, (unsigned char)*p) == NULL) {
                p++;
            }
            if (*p) {
                *p++ = '\0';
            }
            var_set(argv[i + v], start, 0);
        }
    }
    return got_delim ? 0 : 1;
}

static int builtin_set(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "pipefail") == 0) {
                g_pipefail = 1;
            } else if (strcmp(argv[i], "errexit") == 0) {
                g_errexit = 1;
            } else {
                fprintf(stderr, "set: unknown option %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "+o") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "pipefail") == 0) {
                g_pipefail = 0;
            } else if (strcmp(argv[i], "errexit") == 0) {
                g_errexit = 0;
            } else {
                fprintf(stderr, "set: unknown option %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "-e") == 0) {
            g_errexit = 1;
        } else if (strcmp(argv[i], "+e") == 0) {
            g_errexit = 0;
        }
    }
    return 0;
}

static int builtin_umask(int argc, char **argv) {
    int symbolic = 0;
    int idx = 1;
    if (idx < argc && strcmp(argv[idx], "-S") == 0) {
        symbolic = 1;
        idx++;
    }
    if (idx < argc && strcmp(argv[idx], "--") == 0) {
        idx++;
    }
    if (idx >= argc) {
        mode_t m = umask(0u);
        (void)umask(m);
        if (symbolic) {
            mode_t allow = (~m) & 0777u;
            const char *who[3] = {"u", "g", "o"};
            int shifts[3] = {6, 3, 0};
            for (int i = 0; i < 3; i++) {
                unsigned bits = (allow >> shifts[i]) & 7u;
                if (i > 0) {
                    (void)putchar(',');
                }
                (void)printf("%s=", who[i]);
                if (bits & 4u) {
                    (void)putchar('r');
                }
                if (bits & 2u) {
                    (void)putchar('w');
                }
                if (bits & 1u) {
                    (void)putchar('x');
                }
            }
            (void)putchar('\n');
        } else {
            (void)printf("%04o\n", (unsigned)(m & 0777u));
        }
        (void)fflush(stdout);
        return 0;
    }
    char *end = NULL;
    unsigned long val = strtoul(argv[idx], &end, 8);
    if (end == argv[idx] || *end != '\0' || val > 0777UL) {
        (void)fprintf(stderr, "umask: invalid mode: %s\n", argv[idx]);
        return 1;
    }
    (void)umask((mode_t)val);
    return 0;
}

static void print_rlim_val(rlim_t val, rlim_t scale) {
    if (val == RLIM_INFINITY) {
        (void)puts("unlimited");
    } else {
        (void)printf("%lu\n", (unsigned long)(val / scale));
    }
}

static int builtin_ulimit(int argc, char **argv) {
    int hard = 0;
    int soft = 0;
    int res = RLIMIT_FSIZE;
    rlim_t scale = 512UL;
    int show_all = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        for (const char *p = argv[idx] + 1; *p; p++) {
            switch (*p) {
            case 'H':
                hard = 1;
                break;
            case 'S':
                soft = 1;
                break;
            case 'a':
                show_all = 1;
                break;
            case 'c':
                res = RLIMIT_CORE;
                scale = 512UL;
                break;
            case 'd':
                res = RLIMIT_DATA;
                scale = 1024UL;
                break;
            case 'f':
                res = RLIMIT_FSIZE;
                scale = 512UL;
                break;
            case 'n':
                res = RLIMIT_NOFILE;
                scale = 1UL;
                break;
            case 's':
                res = RLIMIT_STACK;
                scale = 1024UL;
                break;
            case 't':
                res = RLIMIT_CPU;
                scale = 1UL;
                break;
            case 'v':
                res = RLIMIT_AS;
                scale = 1024UL;
                break;
            default:
                (void)fprintf(stderr, "ulimit: invalid option -%c\n", *p);
                return 1;
            }
        }
        idx++;
    }

    if (show_all) {
        static const struct {
            const char *label;
            int id;
            rlim_t unit;
        } items[] = {
            {"core file size (blocks, -c) ", RLIMIT_CORE, 512UL},
            {"cpu time (seconds, -t)      ", RLIMIT_CPU, 1UL},
            {"data seg size (kbytes, -d)  ", RLIMIT_DATA, 1024UL},
            {"file size (blocks, -f)      ", RLIMIT_FSIZE, 512UL},
            {"open files (-n)             ", RLIMIT_NOFILE, 1UL},
            {"stack size (kbytes, -s)     ", RLIMIT_STACK, 1024UL},
            {"virtual memory (kbytes, -v) ", RLIMIT_AS, 1024UL},
        };
        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
            struct rlimit rl;
            if (getrlimit(items[i].id, &rl) == 0) {
                (void)fputs(items[i].label, stdout);
                print_rlim_val(hard && !soft ? rl.rlim_max : rl.rlim_cur,
                               items[i].unit);
            }
        }
        (void)fflush(stdout);
        return 0;
    }

    struct rlimit rl;
    if (getrlimit(res, &rl) != 0) {
        perror("ulimit");
        return 1;
    }
    if (idx >= argc) {
        print_rlim_val(hard && !soft ? rl.rlim_max : rl.rlim_cur, scale);
        (void)fflush(stdout);
        return 0;
    }

    rlim_t nv = 0UL;
    if (strcmp(argv[idx], "unlimited") == 0) {
        nv = RLIM_INFINITY;
    } else {
        char *end = NULL;
        errno = 0;
        unsigned long parsed = strtoul(argv[idx], &end, 10);
        if (errno != 0 || end == argv[idx] || *end != '\0') {
            (void)fprintf(stderr, "ulimit: invalid limit: %s\n", argv[idx]);
            return 1;
        }
        nv = (rlim_t)parsed * scale;
    }

    if (!hard && !soft) {
        rl.rlim_cur = nv;
        rl.rlim_max = nv;
    } else {
        if (hard) {
            rl.rlim_max = nv;
        }
        if (soft) {
            rl.rlim_cur = nv;
        }
    }
    if (setrlimit(res, &rl) != 0) {
        perror("ulimit");
        return 1;
    }
    return 0;
}

static int is_builtin_name(const char *cmd) {
    if (!cmd) {
        return 0;
    }
    return strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 ||
           strcmp(cmd, "export") == 0 || strcmp(cmd, "unset") == 0 ||
           strcmp(cmd, "set") == 0 || strcmp(cmd, "read") == 0 ||
           strcmp(cmd, "exit") == 0 || strcmp(cmd, "true") == 0 ||
           strcmp(cmd, "false") == 0 || strcmp(cmd, "echo") == 0 ||
           strcmp(cmd, "wait") == 0 || strcmp(cmd, "umask") == 0 ||
           strcmp(cmd, "ulimit") == 0;
}

static int builtin_wait(int argc, char **argv) {
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0) {
        i++;
    }
    if (i >= argc) {
        int wstatus = 0;
        pid_t pid;
        while ((pid = waitpid((pid_t)-1, &wstatus, 0)) > 0 ||
               (pid < 0 && errno == EINTR)) {
        }
        if (pid < 0 && errno != ECHILD) {
            return 1;
        }
        return 0;
    }
    int status = 0;
    for (; i < argc; i++) {
        char *end = (char *)0;
        errno = 0;
        long target = strtol(argv[i], &end, 10);
        if (errno != 0 || end == argv[i] || *end != '\0' || target <= 0) {
            (void)fprintf(stderr, "wait: %s: invalid pid\n", argv[i]);
            status = 127;
            continue;
        }
        int wstatus = 0;
        pid_t pid;
        while ((pid = waitpid((pid_t)target, &wstatus, 0)) < 0 &&
               errno == EINTR) {
        }
        if (pid < 0) {
            status = 127;
            continue;
        }
        if (WIFEXITED(wstatus)) {
            status = WEXITSTATUS(wstatus);
        } else if (WIFSIGNALED(wstatus)) {
            status = 128 + WTERMSIG(wstatus);
        }
    }
    return status;
}

static int run_builtin(int argc, char **argv) {
    const char *cmd = argv[0];
    if (strcmp(cmd, "true") == 0) {
        return 0;
    }
    if (strcmp(cmd, "false") == 0) {
        return 1;
    }
    if (strcmp(cmd, "echo") == 0) {
        int idx = 1;
        int nl = 1;
        if (idx < argc && strcmp(argv[idx], "-n") == 0) {
            nl = 0;
            idx++;
        }
        for (int first = 1; idx < argc; idx++, first = 0) {
            if (!first) {
                (void)putchar(' ');
            }
            (void)fputs(argv[idx], stdout);
        }
        if (nl) {
            (void)putchar('\n');
        }
        (void)fflush(stdout);
        return 0;
    }
    if (strcmp(cmd, "pwd") == 0) {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            perror("pwd");
            return 1;
        }
        printf("%s\n", cwd);
        (void)fflush(stdout);
        return 0;
    }
    if (strcmp(cmd, "cd") == 0) {
        const char *dir = (argc > 1) ? argv[1] : var_get("HOME");
        if (!dir || !dir[0]) {
            dir = "/";
        }
        if (chdir(dir) != 0) {
            perror("cd");
            return 1;
        }
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            var_set("PWD", cwd, 1);
        }
        return 0;
    }
    if (strcmp(cmd, "export") == 0) {
        for (int i = 1; i < argc; i++) {
            char *eq = strchr(argv[i], '=');
            if (eq) {
                *eq = '\0';
                var_set(argv[i], eq + 1, 1);
                *eq = '=';
            } else {
                const char *cur = var_get(argv[i]);
                var_set(argv[i], cur ? cur : "", 1);
            }
        }
        return 0;
    }
    if (strcmp(cmd, "unset") == 0) {
        for (int i = 1; i < argc; i++) {
            var_unset(argv[i]);
        }
        return 0;
    }
    if (strcmp(cmd, "set") == 0) {
        return builtin_set(argc, argv);
    }
    if (strcmp(cmd, "read") == 0) {
        return builtin_read(argc, argv);
    }
    if (strcmp(cmd, "umask") == 0) {
        return builtin_umask(argc, argv);
    }
    if (strcmp(cmd, "ulimit") == 0) {
        return builtin_ulimit(argc, argv);
    }
    if (strcmp(cmd, "wait") == 0) {
        return builtin_wait(argc, argv);
    }
    if (strcmp(cmd, "exit") == 0) {
        g_should_exit = 1;
        g_exit_code = (argc > 1) ? atoi(argv[1]) : g_last_status;
        return g_exit_code;
    }
    return 127;
}

static int run_single_builtin_with_redirs(const stage_t *st) {
    int saved[3] = {-1, -1, -1};
    if (st->nredirs > 0) {
        for (int fd = 0; fd < 3; fd++) {
            saved[fd] = fcntl(fd, F_DUPFD_CLOEXEC, 10);
        }
        if (apply_redirs(st) != 0) {
            for (int fd = 0; fd < 3; fd++) {
                if (saved[fd] >= 0) {
                    (void)dup2(saved[fd], fd);
                    (void)close(saved[fd]);
                }
            }
            return 1;
        }
    }

    int rc = run_builtin(st->argc, (char **)st->argv);
    (void)fflush(stdout);
    (void)fflush(stderr);

    for (int fd = 0; fd < 3; fd++) {
        if (saved[fd] >= 0) {
            (void)dup2(saved[fd], fd);
            (void)close(saved[fd]);
        }
    }
    return rc;
}

static int exec_pipeline(int nstages, int negated) {
    if (nstages <= 0) {
        return 0;
    }

    /* Handle pure variable assignment(s) when single stage has no command. */
    if (nstages == 1 && g_stages[0].argc > 0) {
        int all_assign = 1;
        for (int i = 0; i < g_stages[0].argc; i++) {
            if (!is_assignment_word(g_stages[0].argv[i])) {
                all_assign = 0;
                break;
            }
        }
        if (all_assign) {
            for (int i = 0; i < g_stages[0].argc; i++) {
                char *eq = strchr(g_stages[0].argv[i], '=');
                if (eq) {
                    *eq = '\0';
                    var_set(g_stages[0].argv[i], eq + 1, 0);
                    *eq = '=';
                }
            }
            return negated ? 1 : 0;
        }
    }

    /* Single-stage built-in runs in-process to mutate shell state. */
    if (nstages == 1 && g_stages[0].argc > 0 &&
        is_builtin_name(g_stages[0].argv[0])) {
        int rc = run_single_builtin_with_redirs(&g_stages[0]);
        return negated ? (rc == 0 ? 1 : 0) : rc;
    }

    int pipes[SH_MAX_STAGES][2];
    for (int i = 0; i < nstages - 1; i++) {
        if (pipe2(pipes[i], O_CLOEXEC) != 0) {
            perror("pipe2");
            for (int j = 0; j < i; j++) {
                (void)close(pipes[j][0]);
                (void)close(pipes[j][1]);
            }
            return 1;
        }
    }

    pid_t pids[SH_MAX_STAGES];
    for (int i = 0; i < SH_MAX_STAGES; i++) {
        pids[i] = -1;
    }
    pid_t job_pgid = 0;
    for (int i = 0; i < nstages; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            if (g_interactive) {
                pid_t pg = job_pgid > 0 ? job_pgid : getpid();
                (void)setpgid(0, pg);
                (void)signal(SIGINT, SIG_DFL);
                (void)signal(SIGQUIT, SIG_DFL);
            }
            if (i > 0) {
                (void)dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i + 1 < nstages) {
                (void)dup2(pipes[i][1], STDOUT_FILENO);
            }
            for (int j = 0; j < nstages - 1; j++) {
                (void)close(pipes[j][0]);
                (void)close(pipes[j][1]);
            }
            if (apply_redirs(&g_stages[i]) != 0) {
                _exit(1);
            }
            /* Apply any leading per-command VAR=VAL assignments in child. */
            int first_cmd = 0;
            while (first_cmd < g_stages[i].argc &&
                   is_assignment_word(g_stages[i].argv[first_cmd])) {
                char *eq = strchr(g_stages[i].argv[first_cmd], '=');
                if (eq) {
                    *eq = '\0';
                    (void)setenv(g_stages[i].argv[first_cmd], eq + 1, 1);
                    *eq = '=';
                }
                first_cmd++;
            }
            if (first_cmd >= g_stages[i].argc) {
                _exit(0);
            }
            char **cargv = &g_stages[i].argv[first_cmd];
            int cargc = g_stages[i].argc - first_cmd;
            if (is_builtin_name(cargv[0])) {
                _exit(run_builtin(cargc, cargv));
            }
            (void)execvp(cargv[0], cargv);
            int err = errno;
            perror(cargv[0]);
            _exit(err == ENOENT ? 127 : 126);
        }
        pids[i] = pid;
        if (g_interactive) {
            if (job_pgid == 0) {
                job_pgid = pid;
            }
            (void)setpgid(pid, job_pgid);
        }
    }

    if (g_interactive && job_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, job_pgid);
    }

    for (int j = 0; j < nstages - 1; j++) {
        (void)close(pipes[j][0]);
        (void)close(pipes[j][1]);
    }

    int statuses[SH_MAX_STAGES];
    for (int i = 0; i < nstages; i++) {
        statuses[i] = 1;
        if (pids[i] > 0) {
            int wst = 0;
            while (waitpid(pids[i], &wst, 0) < 0 && errno == EINTR) {
            }
            if (WIFEXITED(wst)) {
                statuses[i] = WEXITSTATUS(wst);
            } else if (WIFSIGNALED(wst)) {
                statuses[i] = 128 + WTERMSIG(wst);
            }
        }
    }

    if (g_interactive && g_shell_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    }

    int pipe_rc = 0;
    if (g_pipefail) {
        for (int i = 0; i < nstages; i++) {
            if (statuses[i] != 0) {
                pipe_rc = statuses[i];
            }
        }
    } else {
        pipe_rc = statuses[nstages - 1];
    }
    return negated ? (pipe_rc == 0 ? 1 : 0) : pipe_rc;
}

static int is_redir_tok(tok_kind_t k) {
    return k == TOK_REDIR_IN || k == TOK_REDIR_OUT || k == TOK_REDIR_APP ||
           k == TOK_REDIR_ERR || k == TOK_REDIR_ERR_APP ||
           k == TOK_DUP_ERR_TO_OUT || k == TOK_DUP_OUT_TO_ERR;
}

static int eval_script(const char *script) {
    const char *cur = script;
    int run_next = 1;

    while (*cur && !g_should_exit) {
        tok_kind_t sep = TOK_EOF;
        int ntok = next_pipeline_tokens(&cur, &sep);
        int pos = 0;

        if (ntok > 0) {
            int negated = 0;
            if (g_tokens[pos].kind == TOK_WORD &&
                strcmp(g_tokens[pos].text, "!") == 0) {
                negated = 1;
                pos++;
            }

            int nstages = 0;
            memset(g_stages, 0, sizeof(g_stages));
            int syntax_err = 0;

            while (pos < ntok && nstages < SH_MAX_STAGES) {
                stage_t *st = &g_stages[nstages];
                st->argc = 0;
                st->nredirs = 0;

                while (pos < ntok) {
                    tok_kind_t k = g_tokens[pos].kind;
                    if (k == TOK_WORD) {
                        if (st->argc < SH_MAX_ARGS - 1) {
                            st->argv[st->argc++] = g_tokens[pos].text;
                            st->argv[st->argc] = NULL;
                        }
                        pos++;
                    } else if (is_redir_tok(k)) {
                        if (st->nredirs >= SH_MAX_REDIRS) {
                            syntax_err = 1;
                            break;
                        }
                        redir_t *rd = &st->redirs[st->nredirs++];
                        rd->kind = k;
                        rd->target[0] = '\0';
                        pos++;
                        if (k != TOK_DUP_ERR_TO_OUT &&
                            k != TOK_DUP_OUT_TO_ERR) {
                            if (pos >= ntok || g_tokens[pos].kind != TOK_WORD) {
                                fprintf(stderr,
                                        "sh: missing redirection target\n");
                                syntax_err = 1;
                                break;
                            }
                            (void)strlcpy(rd->target, g_tokens[pos].text,
                                          sizeof(rd->target));
                            pos++;
                        }
                    } else {
                        break;
                    }
                }

                if (syntax_err) {
                    break;
                }
                if (st->argc > 0 || st->nredirs > 0) {
                    nstages++;
                }
                if (pos < ntok && g_tokens[pos].kind == TOK_PIPE) {
                    pos++;
                    continue;
                }
                break;
            }

            if (syntax_err) {
                g_last_status = 2;
                break;
            }

            if (run_next && nstages > 0) {
                g_last_status = exec_pipeline(nstages, negated);
                if (g_errexit && g_last_status != 0 && !g_should_exit) {
                    break;
                }
            }
        }

        if (sep == TOK_AND) {
            run_next = (g_last_status == 0);
        } else if (sep == TOK_OR) {
            run_next = (g_last_status != 0);
        } else {
            run_next = 1;
        }
    }
    return g_should_exit ? g_exit_code : g_last_status;
}

int main(int argc, char **argv) {
    if (!getenv("PATH")) {
        (void)setenv("PATH", "/boot:/bin", 1);
    }

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        return eval_script(argv[2]);
    }

    FILE *in = stdin;
    int close_in = 0;
    if (argc >= 2) {
        in = fopen(argv[1], "r");
        if (!in) {
            perror(argv[1]);
            return 127;
        }
        close_in = 1;
    }

    int interactive = (!close_in && isatty(STDIN_FILENO));
    g_interactive = interactive;
    if (interactive) {
        (void)signal(SIGINT, SIG_IGN);
        (void)signal(SIGQUIT, SIG_IGN);
        (void)setpgid(0, 0);
        g_shell_pgid = getpgrp();
        (void)tcsetpgrp(STDIN_FILENO, g_shell_pgid);
    }
    char line[SH_MAX_LINE];
    while (!g_should_exit) {
        if (interactive) {
            (void)fputs("$ ", stdout);
            (void)fflush(stdout);
        }
        if (!fgets(line, (int)sizeof(line), in)) {
            break;
        }
        (void)eval_script(line);
    }

    if (close_in) {
        (void)fclose(in);
    }
    return g_should_exit ? g_exit_code : g_last_status;
}
