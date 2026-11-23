/**
 * @file     xargs.c
 * @brief    Ring 3 xargs utility targeting POSIX.1-2024 XCU (-0, -n, -I, -t)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: COMP_LIBC_0018 */

struct xvec {
    char **items;
    size_t len;
    size_t cap;
};

static int xvec_push(struct xvec *v, char *s) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2u : 32u;
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

static char *replace_all(const char *tmpl, const char *needle,
                         const char *val) {
    size_t nlen = strlen(needle);
    if (nlen == 0u) {
        return strdup(tmpl);
    }
    size_t vlen = strlen(val);
    size_t cap = strlen(tmpl) + 64u;
    char *out = (char *)malloc(cap);
    if (!out) {
        return NULL;
    }
    size_t olen = 0u;
    const char *p = tmpl;
    while (*p != '\0') {
        const char *hit = strstr(p, needle);
        size_t seg = hit ? (size_t)(hit - p) : strlen(p);
        size_t need = olen + seg + (hit ? vlen : 0u) + 1u;
        if (need > cap) {
            cap = need * 2u;
            char *no = (char *)realloc(out, cap);
            if (!no) {
                free(out);
                return NULL;
            }
            out = no;
        }
        memcpy(out + olen, p, seg);
        olen += seg;
        if (hit) {
            memcpy(out + olen, val, vlen);
            olen += vlen;
            p = hit + nlen;
        } else {
            break;
        }
    }
    out[olen] = '\0';
    return out;
}

static int run_cmd(char **argv, int trace) {
    if (trace) {
        for (int i = 0; argv[i] != NULL; i++) {
            if (i > 0) {
                fputc(' ', stderr);
            }
            fputs(argv[i], stderr);
        }
        fputc('\n', stderr);
    }
    pid_t pid = fork();
    if (pid < 0) {
        perror("xargs: fork");
        return 1;
    }
    if (pid == 0) {
        (void)execvp(argv[0], argv);
        int err = errno;
        perror(argv[0]);
        _exit(err == ENOENT ? 127 : 126);
    }
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    if (WIFEXITED(st)) {
        int code = WEXITSTATUS(st);
        if (code == 0) {
            return 0;
        }
        if (code == 126 || code == 127 || code == 255) {
            return code;
        }
        return 123;
    }
    return 125;
}

static void xvec_push_dup(struct xvec *v, const char *s) {
    char *dup = strdup(s);
    if (!dup) {
        return;
    }
    if (xvec_push(v, dup) != 0) {
        free(dup);
    }
}

static int grow_buf(char **buf, size_t *cap, size_t need) {
    if (need <= *cap) {
        return 0;
    }
    size_t ncap = (*cap) * 2u;
    char *nb = (char *)realloc(*buf, ncap);
    if (!nb) {
        return -1;
    }
    *buf = nb;
    *cap = ncap;
    return 0;
}

static void read_tokens(struct xvec *out, int nul_mode, const char *eof_str) {
    size_t cap = 64u;
    size_t len = 0u;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        return;
    }
    if (nul_mode) {
        int c;
        while ((c = getchar()) != EOF) {
            if (c == '\0') {
                buf[len] = '\0';
                xvec_push_dup(out, buf);
                len = 0u;
            } else if (grow_buf(&buf, &cap, len + 2u) == 0) {
                buf[len++] = (char)c;
            }
        }
        if (len > 0u) {
            buf[len] = '\0';
            xvec_push_dup(out, buf);
        }
        free(buf);
        return;
    }

    int in_tok = 0;
    int quote = 0;
    int c;
    while ((c = getchar()) != EOF) {
        if (quote) {
            if (c == quote) {
                quote = 0;
            } else if (grow_buf(&buf, &cap, len + 2u) == 0) {
                buf[len++] = (char)c;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            in_tok = 1;
            continue;
        }
        if (c == '\\') {
            int nc = getchar();
            if (nc != EOF && grow_buf(&buf, &cap, len + 2u) == 0) {
                buf[len++] = (char)nc;
                in_tok = 1;
            }
            continue;
        }
        if (isspace((unsigned char)c)) {
            if (in_tok) {
                buf[len] = '\0';
                if (eof_str && strcmp(buf, eof_str) == 0) {
                    len = 0u;
                    in_tok = 0;
                    break;
                }
                xvec_push_dup(out, buf);
                len = 0u;
                in_tok = 0;
            }
            continue;
        }
        if (grow_buf(&buf, &cap, len + 2u) == 0) {
            buf[len++] = (char)c;
            in_tok = 1;
        }
    }
    if (in_tok) {
        buf[len] = '\0';
        if (!eof_str || strcmp(buf, eof_str) != 0) {
            xvec_push_dup(out, buf);
        }
    }
    free(buf);
}

int main(int argc, char **argv) {
    int nul_mode = 0;
    int max_args = 0;
    int trace = 0;
    int no_empty = 0;
    const char *repl_str = NULL;
    const char *eof_str = NULL;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-0") == 0) {
            nul_mode = 1;
            idx++;
        } else if (strcmp(argv[idx], "-t") == 0) {
            trace = 1;
            idx++;
        } else if (strcmp(argv[idx], "-r") == 0) {
            no_empty = 1;
            idx++;
        } else if (strcmp(argv[idx], "-x") == 0) {
            idx++;
        } else if ((strcmp(argv[idx], "-n") == 0 ||
                    strcmp(argv[idx], "-L") == 0) &&
                   idx + 1 < argc) {
            max_args = atoi(argv[idx + 1]);
            idx += 2;
        } else if (strcmp(argv[idx], "-I") == 0 && idx + 1 < argc) {
            repl_str = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-E") == 0 && idx + 1 < argc) {
            eof_str = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-s") == 0 && idx + 1 < argc) {
            idx += 2;
        } else {
            fprintf(stderr, "xargs: unknown option '%s'\n", argv[idx]);
            return 1;
        }
    }

    static char *default_cmd[] = {"echo"};
    char **base_argv = (idx < argc) ? &argv[idx] : default_cmd;
    int base_argc = (idx < argc) ? (argc - idx) : 1;

    struct xvec toks = {0};
    read_tokens(&toks, nul_mode, eof_str);

    int exit_rc = 0;
    if (toks.len == 0u) {
        if (!no_empty && !repl_str) {
            char **av = (char **)calloc((size_t)base_argc + 1u, sizeof(char *));
            for (int i = 0; i < base_argc; i++) {
                av[i] = base_argv[i];
            }
            exit_rc = run_cmd(av, trace);
            free(av);
        }
    } else if (repl_str) {
        for (size_t i = 0u; i < toks.len; i++) {
            char **av = (char **)calloc((size_t)base_argc + 1u, sizeof(char *));
            for (int j = 0; j < base_argc; j++) {
                av[j] = replace_all(base_argv[j], repl_str, toks.items[i]);
            }
            int rc = run_cmd(av, trace);
            if (rc > exit_rc) {
                exit_rc = rc;
            }
            for (int j = 0; j < base_argc; j++) {
                free(av[j]);
            }
            free(av);
        }
    } else {
        size_t chunk = (max_args > 0) ? (size_t)max_args : toks.len;
        for (size_t pos = 0u; pos < toks.len; pos += chunk) {
            size_t cnt = (pos + chunk <= toks.len) ? chunk : (toks.len - pos);
            char **av =
                (char **)calloc((size_t)base_argc + cnt + 1u, sizeof(char *));
            for (int j = 0; j < base_argc; j++) {
                av[j] = base_argv[j];
            }
            for (size_t j = 0u; j < cnt; j++) {
                av[(size_t)base_argc + j] = toks.items[pos + j];
            }
            int rc = run_cmd(av, trace);
            if (rc > exit_rc) {
                exit_rc = rc;
            }
            free(av);
        }
    }

    for (size_t i = 0u; i < toks.len; i++) {
        free(toks.items[i]);
    }
    free(toks.items);
    return exit_rc;
}
