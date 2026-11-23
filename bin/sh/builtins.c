/**
 * @file     builtins.c
 * @brief    POSIX.1-2024 shell state, variables, traps, and built-in utilities
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
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sh.h"

/* need:: COMP_LIBC_0017 COMP_PROC_0010 */

volatile sig_atomic_t g_sh_sig_pending[SH_MAX_TRAPS];

int sh_is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

int sh_is_name_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

int sh_is_assignment(const char *w) {
    if (!w || !sh_is_name_start(w[0])) {
        return 0;
    }
    for (size_t i = 1u; w[i] != '\0'; i++) {
        if (w[i] == '=') {
            return 1;
        }
        if (!sh_is_name_char(w[i])) {
            return 0;
        }
    }
    return 0;
}

const char *sh_var_get(const char *name) {
    if (!name || name[0] == '\0') {
        return NULL;
    }
    for (sh_var_t *v = g_sh.vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            return v->val;
        }
    }
    return getenv(name);
}

int sh_var_set(const char *name, const char *val, int do_export,
               int do_readonly) {
    if (!name || !sh_is_name_start(name[0])) {
        return -1;
    }
    if (strcmp(name, "OPTIND") == 0) {
        g_sh.getopts_charoff = 0;
    }
    for (sh_var_t *v = g_sh.vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            if (v->readonly && val) {
                fprintf(stderr, "sh: %s: readonly variable\n", name);
                return -1;
            }
            if (val) {
                char *nv = strdup(val);
                if (!nv) {
                    return -1;
                }
                free(v->val);
                v->val = nv;
            }
            if (do_export) {
                v->exported = 1;
            }
            if (do_readonly) {
                v->readonly = 1;
            }
            if (v->exported) {
                (void)setenv(v->name, v->val, 1);
            }
            return 0;
        }
    }
    sh_var_t *v = (sh_var_t *)calloc(1u, sizeof(*v));
    if (!v) {
        return -1;
    }
    const char *init = val ? val : getenv(name);
    v->name = strdup(name);
    v->val = strdup(init ? init : "");
    v->exported = do_export || (getenv(name) != NULL);
    v->readonly = do_readonly;
    v->next = g_sh.vars;
    g_sh.vars = v;
    if (v->exported) {
        (void)setenv(v->name, v->val, 1);
    }
    return 0;
}

int sh_var_unset(const char *name) {
    if (!name || name[0] == '\0') {
        return 0;
    }
    sh_var_t **pp = &g_sh.vars;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            if ((*pp)->readonly) {
                fprintf(stderr, "sh: %s: readonly variable\n", name);
                return -1;
            }
            sh_var_t *dead = *pp;
            *pp = dead->next;
            free(dead->name);
            free(dead->val);
            free(dead);
            break;
        }
        pp = &(*pp)->next;
    }
    (void)unsetenv(name);
    return 0;
}

void sh_set_positional(int argc, char **argv) {
    for (int i = 0; i < g_sh.pos_argc; i++) {
        free(g_sh.pos_args[i]);
    }
    free(g_sh.pos_args);
    g_sh.pos_args = NULL;
    g_sh.pos_argc = 0;
    if (argc > 0) {
        g_sh.pos_args = (char **)calloc((size_t)argc, sizeof(char *));
        if (g_sh.pos_args) {
            for (int i = 0; i < argc; i++) {
                g_sh.pos_args[i] = strdup(argv[i]);
            }
            g_sh.pos_argc = argc;
        }
    }
}

/* ---- Aliases ---- */

const char *sh_alias_get(const char *name) {
    for (sh_alias_t *a = g_sh.aliases; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            return a->val;
        }
    }
    return NULL;
}

void sh_alias_set(const char *name, const char *val) {
    for (sh_alias_t *a = g_sh.aliases; a; a = a->next) {
        if (strcmp(a->name, name) == 0) {
            free(a->val);
            a->val = strdup(val);
            return;
        }
    }
    sh_alias_t *a = (sh_alias_t *)calloc(1u, sizeof(*a));
    if (!a) {
        return;
    }
    a->name = strdup(name);
    a->val = strdup(val);
    a->next = g_sh.aliases;
    g_sh.aliases = a;
}

int sh_alias_unset(const char *name) {
    sh_alias_t **pp = &g_sh.aliases;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            sh_alias_t *dead = *pp;
            *pp = dead->next;
            free(dead->name);
            free(dead->val);
            free(dead);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

void sh_alias_clear(void) {
    while (g_sh.aliases) {
        sh_alias_t *dead = g_sh.aliases;
        g_sh.aliases = dead->next;
        free(dead->name);
        free(dead->val);
        free(dead);
    }
}

/* ---- Functions ---- */

sh_func_t *sh_func_find(const char *name) {
    for (sh_func_t *f = g_sh.funcs; f; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

void sh_func_set(const char *name, sh_node_t *body) {
    for (sh_func_t *f = g_sh.funcs; f; f = f->next) {
        if (strcmp(f->name, name) == 0) {
            sh_node_free(f->body);
            f->body = body;
            return;
        }
    }
    sh_func_t *f = (sh_func_t *)calloc(1u, sizeof(*f));
    if (!f) {
        sh_node_free(body);
        return;
    }
    f->name = strdup(name);
    f->body = body;
    f->next = g_sh.funcs;
    g_sh.funcs = f;
}

void sh_func_unset(const char *name) {
    sh_func_t **pp = &g_sh.funcs;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            sh_func_t *dead = *pp;
            *pp = dead->next;
            free(dead->name);
            sh_node_free(dead->body);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

/* ---- PATH hash table ---- */

const char *sh_hash_lookup(const char *name) {
    for (sh_hash_t *h = g_sh.hashes; h; h = h->next) {
        if (strcmp(h->name, name) == 0) {
            h->hits++;
            return h->path;
        }
    }
    return NULL;
}

void sh_hash_store(const char *name, const char *path) {
    for (sh_hash_t *h = g_sh.hashes; h; h = h->next) {
        if (strcmp(h->name, name) == 0) {
            free(h->path);
            h->path = strdup(path);
            return;
        }
    }
    sh_hash_t *h = (sh_hash_t *)calloc(1u, sizeof(*h));
    if (!h) {
        return;
    }
    h->name = strdup(name);
    h->path = strdup(path);
    h->hits = 0u;
    h->next = g_sh.hashes;
    g_sh.hashes = h;
}

void sh_hash_clear(void) {
    while (g_sh.hashes) {
        sh_hash_t *dead = g_sh.hashes;
        g_sh.hashes = dead->next;
        free(dead->name);
        free(dead->path);
        free(dead);
    }
}

char *sh_find_in_path(const char *cmd, int use_default_path) {
    if (!cmd || cmd[0] == '\0') {
        return NULL;
    }
    struct stat st;
    if (strchr(cmd, '/') != NULL) {
        if (stat(cmd, &st) == 0 && S_ISREG(st.st_mode)) {
            return strdup(cmd);
        }
        return NULL;
    }
    const char *path = use_default_path ? "/boot:/bin" : sh_var_get("PATH");
    if (!path || path[0] == '\0') {
        path = "/boot:/bin";
    }
    const char *p = path;
    while (*p != '\0') {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        char cand[PATH_MAX];
        if (len == 0u) {
            (void)strlcpy(cand, cmd, sizeof(cand));
        } else {
            if (len + strlen(cmd) + 2u < sizeof(cand)) {
                memcpy(cand, p, len);
                cand[len] = '/';
                (void)strlcpy(cand + len + 1u, cmd, sizeof(cand) - len - 1u);
            } else {
                cand[0] = '\0';
            }
        }
        if (cand[0] != '\0' && stat(cand, &st) == 0 && S_ISREG(st.st_mode)) {
            return strdup(cand);
        }
        if (!colon) {
            break;
        }
        p = colon + 1;
    }
    return NULL;
}

/* ---- Signal traps ---- */

static void sh_trap_handler(int sig) {
    if (sig > 0 && sig < SH_MAX_TRAPS) {
        g_sh_sig_pending[sig] = 1;
    }
}

int sh_parse_sig(const char *spec) {
    if (!spec || spec[0] == '\0') {
        return -1;
    }
    if (strcasecmp(spec, "EXIT") == 0 || strcmp(spec, "0") == 0) {
        return 0;
    }
    if (isdigit((unsigned char)spec[0])) {
        char *end = NULL;
        long v = strtol(spec, &end, 10);
        if (end && *end == '\0' && v >= 0 && v < SH_MAX_TRAPS) {
            return (int)v;
        }
        return -1;
    }
    if (strncasecmp(spec, "SIG", 3u) == 0) {
        spec += 3;
    }
    if (strcasecmp(spec, "HUP") == 0) {
        return SIGHUP;
    }
    if (strcasecmp(spec, "INT") == 0) {
        return SIGINT;
    }
    if (strcasecmp(spec, "QUIT") == 0) {
        return SIGQUIT;
    }
    if (strcasecmp(spec, "TERM") == 0) {
        return SIGTERM;
    }
    if (strcasecmp(spec, "ALRM") == 0) {
        return SIGALRM;
    }
    if (strcasecmp(spec, "USR1") == 0) {
        return SIGUSR1;
    }
    if (strcasecmp(spec, "USR2") == 0) {
        return SIGUSR2;
    }
    if (strcasecmp(spec, "PIPE") == 0) {
        return SIGPIPE;
    }
    if (strcasecmp(spec, "CHLD") == 0) {
        return SIGCHLD;
    }
    return -1;
}

const char *sh_sig_name(int sig) {
    switch (sig) {
    case 0:
        return "EXIT";
    case SIGHUP:
        return "HUP";
    case SIGINT:
        return "INT";
    case SIGQUIT:
        return "QUIT";
    case SIGTERM:
        return "TERM";
    case SIGALRM:
        return "ALRM";
    case SIGUSR1:
        return "USR1";
    case SIGUSR2:
        return "USR2";
    case SIGPIPE:
        return "PIPE";
    case SIGCHLD:
        return "CHLD";
    default:
        return "UNKNOWN";
    }
}

void sh_check_traps(void) {
    for (int s = 1; s < SH_MAX_TRAPS; s++) {
        if (g_sh_sig_pending[s]) {
            g_sh_sig_pending[s] = 0;
            if (g_sh.traps[s] && g_sh.traps[s][0] != '\0') {
                int saved = g_sh.last_status;
                char *action = strdup(g_sh.traps[s]);
                if (action) {
                    (void)sh_eval_string(action);
                    free(action);
                }
                g_sh.last_status = saved;
            }
        }
    }
}

void sh_run_exit_trap(void) {
    if (g_sh.in_exit_trap) {
        return;
    }
    if (g_sh.traps[0] && g_sh.traps[0][0] != '\0') {
        g_sh.in_exit_trap = 1;
        char *action = strdup(g_sh.traps[0]);
        int saved_exit = g_sh.should_exit;
        int saved_code = g_sh.exit_code;
        g_sh.should_exit = 0;
        if (action) {
            (void)sh_eval_string(action);
            free(action);
        }
        g_sh.should_exit = saved_exit;
        g_sh.exit_code = saved_code;
    }
}

/* ---- Built-in implementations ---- */

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
            delim = argv[i + 1][0];
            i += 2;
        } else {
            fprintf(stderr, "read: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    sh_buf_t line = {0};
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
            (void)sh_buf_push(&line, next);
            continue;
        }
        (void)sh_buf_push(&line, ch);
    }
    char *s = line.data ? line.data : (char *)"";
    int nvars = argc - i;
    if (nvars <= 0) {
        (void)sh_var_set("REPLY", s, 0, 0);
        sh_buf_free(&line);
        return got_delim ? 0 : 1;
    }
    const char *ifs = sh_var_get("IFS");
    if (!ifs) {
        ifs = " \t\n";
    }
    char *p = s;
    for (int v = 0; v < nvars; v++) {
        while (*p != '\0' && *p != delim &&
               strchr(ifs, (unsigned char)*p) != NULL &&
               isspace((unsigned char)*p)) {
            p++;
        }
        if (v == nvars - 1) {
            size_t rem = strlen(p);
            while (rem > 0u && p[rem - 1u] != delim &&
                   strchr(ifs, (unsigned char)p[rem - 1u]) != NULL &&
                   isspace((unsigned char)p[rem - 1u])) {
                p[--rem] = '\0';
            }
            (void)sh_var_set(argv[i + v], p, 0, 0);
        } else {
            char *start = p;
            while (*p != '\0' && strchr(ifs, (unsigned char)*p) == NULL) {
                p++;
            }
            if (*p != '\0') {
                *p++ = '\0';
            }
            (void)sh_var_set(argv[i + v], start, 0, 0);
        }
    }
    sh_buf_free(&line);
    return got_delim ? 0 : 1;
}

static int builtin_set(int argc, char **argv) {
    if (argc == 1) {
        for (sh_var_t *v = g_sh.vars; v; v = v->next) {
            printf("%s='%s'\n", v->name, v->val);
        }
        return 0;
    }
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "--") == 0) {
            sh_set_positional(argc - (i + 1), &argv[i + 1]);
            return 0;
        }
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "+o") == 0) {
            int on = (argv[i][0] == '-');
            if (i + 1 >= argc) {
                printf("pipefail\t%s\nerrexit\t\t%s\nnounset\t\t%s\n"
                       "noglob\t\t%s\nallexport\t%s\n",
                       g_sh.opt_pipefail ? "on" : "off",
                       g_sh.opt_errexit ? "on" : "off",
                       g_sh.opt_nounset ? "on" : "off",
                       g_sh.opt_noglob ? "on" : "off",
                       g_sh.opt_allexport ? "on" : "off");
                return 0;
            }
            i++;
            if (strcmp(argv[i], "pipefail") == 0) {
                g_sh.opt_pipefail = on;
            } else if (strcmp(argv[i], "errexit") == 0) {
                g_sh.opt_errexit = on;
            } else if (strcmp(argv[i], "nounset") == 0) {
                g_sh.opt_nounset = on;
            } else if (strcmp(argv[i], "noglob") == 0) {
                g_sh.opt_noglob = on;
            } else if (strcmp(argv[i], "allexport") == 0) {
                g_sh.opt_allexport = on;
            } else {
                fprintf(stderr, "set: unknown option %s\n", argv[i]);
                return 2;
            }
            i++;
            continue;
        }
        if ((argv[i][0] == '-' || argv[i][0] == '+') && argv[i][1] != '\0') {
            int on = (argv[i][0] == '-');
            for (const char *p = argv[i] + 1; *p != '\0'; p++) {
                if (*p == 'e') {
                    g_sh.opt_errexit = on;
                } else if (*p == 'u') {
                    g_sh.opt_nounset = on;
                } else if (*p == 'f') {
                    g_sh.opt_noglob = on;
                } else if (*p == 'a') {
                    g_sh.opt_allexport = on;
                } else {
                    fprintf(stderr, "set: unknown option %c%c\n",
                            on ? '-' : '+', *p);
                    return 2;
                }
            }
            i++;
            continue;
        }
        sh_set_positional(argc - i, &argv[i]);
        return 0;
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
        for (const char *p = argv[idx] + 1; *p != '\0'; p++) {
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
        for (size_t i = 0u; i < sizeof(items) / sizeof(items[0]); i++) {
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

static int builtin_wait(int argc, char **argv) {
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0) {
        i++;
    }
    if (i >= argc) {
        int wst = 0;
        pid_t pid;
        while ((pid = waitpid((pid_t)-1, &wst, 0)) > 0 ||
               (pid < 0 && errno == EINTR)) {
        }
        return (pid < 0 && errno != ECHILD) ? 1 : 0;
    }
    int status = 0;
    for (; i < argc; i++) {
        char *end = NULL;
        errno = 0;
        long target = strtol(argv[i], &end, 10);
        if (errno != 0 || end == argv[i] || *end != '\0' || target <= 0) {
            fprintf(stderr, "wait: %s: invalid pid\n", argv[i]);
            status = 127;
            continue;
        }
        int wst = 0;
        pid_t pid;
        while ((pid = waitpid((pid_t)target, &wst, 0)) < 0 && errno == EINTR) {
        }
        if (pid < 0) {
            status = 127;
        } else if (WIFEXITED(wst)) {
            status = WEXITSTATUS(wst);
        } else if (WIFSIGNALED(wst)) {
            status = 128 + WTERMSIG(wst);
        }
    }
    return status;
}

static int builtin_dot(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, ".: filename argument required\n");
        return 2;
    }
    char *path = sh_find_in_path(argv[1], 0);
    if (!path) {
        fprintf(stderr, ".: %s: not found\n", argv[1]);
        return 1;
    }
    FILE *fp = fopen(path, "r");
    free(path);
    if (!fp) {
        perror(argv[1]);
        return 1;
    }
    sh_buf_t src = {0};
    char chunk[256];
    for (;;) {
        size_t n = fread(chunk, 1u, sizeof(chunk), fp);
        if (n == 0u) {
            break;
        }
        (void)sh_buf_putn(&src, chunk, n);
    }
    (void)fclose(fp);
    g_sh.dot_depth++;
    int rc = sh_eval_string(src.data ? src.data : "");
    g_sh.dot_depth--;
    if (g_sh.returning) {
        rc = g_sh.return_code;
        g_sh.returning = 0;
    }
    sh_buf_free(&src);
    return rc;
}

static int builtin_getopts(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: getopts optstring name [arg...]\n");
        return 2;
    }
    const char *optstr = argv[1];
    const char *varname = argv[2];
    int silent = (optstr[0] == ':');
    int n_args = (argc > 3) ? (argc - 3) : g_sh.pos_argc;
    char **args = (argc > 3) ? &argv[3] : g_sh.pos_args;

    const char *oi_s = sh_var_get("OPTIND");
    int optind_val = (oi_s && oi_s[0] != '\0') ? atoi(oi_s) : 1;
    if (optind_val < 1) {
        optind_val = 1;
        g_sh.getopts_charoff = 0;
    }

    if (optind_val > n_args) {
        (void)sh_var_set(varname, "?", 0, 0);
        (void)sh_var_unset("OPTARG");
        return 1;
    }
    const char *arg = args[optind_val - 1];
    if (!arg || arg[0] != '-' || arg[1] == '\0') {
        (void)sh_var_set(varname, "?", 0, 0);
        (void)sh_var_unset("OPTARG");
        return 1;
    }
    if (strcmp(arg, "--") == 0) {
        char nbuf[16];
        (void)snprintf(nbuf, sizeof(nbuf), "%d", optind_val + 1);
        (void)sh_var_set("OPTIND", nbuf, 0, 0);
        (void)sh_var_set(varname, "?", 0, 0);
        (void)sh_var_unset("OPTARG");
        return 1;
    }

    int off = (g_sh.getopts_charoff > 0) ? g_sh.getopts_charoff : 1;
    char opt = arg[off];
    if (opt == '\0') {
        g_sh.getopts_charoff = 0;
        return 1;
    }

    const char *spec = strchr(silent ? optstr + 1 : optstr, opt);
    char ch_str[2] = {opt, '\0'};
    int next_ind = optind_val;
    int next_off = off + 1;
    if (arg[next_off] == '\0') {
        next_ind++;
        next_off = 0;
    }

    if (!spec || opt == ':') {
        (void)sh_var_set(varname, "?", 0, 0);
        if (silent) {
            (void)sh_var_set("OPTARG", ch_str, 0, 0);
        } else {
            (void)sh_var_unset("OPTARG");
            fprintf(stderr, "getopts: illegal option -- %c\n", opt);
        }
        char nbuf[16];
        (void)snprintf(nbuf, sizeof(nbuf), "%d", next_ind);
        (void)sh_var_set("OPTIND", nbuf, 0, 0);
        g_sh.getopts_charoff = next_off;
        return 0;
    }

    if (spec[1] == ':') {
        const char *val = NULL;
        if (arg[off + 1] != '\0') {
            val = arg + off + 1;
            next_ind = optind_val + 1;
            next_off = 0;
        } else if (optind_val < n_args) {
            val = args[optind_val];
            next_ind = optind_val + 2;
            next_off = 0;
        } else {
            next_ind = optind_val + 1;
            next_off = 0;
            if (silent) {
                (void)sh_var_set(varname, ":", 0, 0);
                (void)sh_var_set("OPTARG", ch_str, 0, 0);
            } else {
                (void)sh_var_set(varname, "?", 0, 0);
                (void)sh_var_unset("OPTARG");
                fprintf(stderr, "getopts: option requires an argument -- %c\n",
                        opt);
            }
            char nbuf[16];
            (void)snprintf(nbuf, sizeof(nbuf), "%d", next_ind);
            (void)sh_var_set("OPTIND", nbuf, 0, 0);
            g_sh.getopts_charoff = next_off;
            return 0;
        }
        (void)sh_var_set(varname, ch_str, 0, 0);
        (void)sh_var_set("OPTARG", val, 0, 0);
    } else {
        (void)sh_var_set(varname, ch_str, 0, 0);
        (void)sh_var_unset("OPTARG");
    }
    char nbuf[16];
    (void)snprintf(nbuf, sizeof(nbuf), "%d", next_ind);
    (void)sh_var_set("OPTIND", nbuf, 0, 0);
    g_sh.getopts_charoff = next_off;
    return 0;
}

static int builtin_command(int argc, char **argv) {
    int def_path = 0;
    int opt_v = 0;
    int opt_V = 0;
    int idx = 1;
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        for (const char *p = argv[idx] + 1; *p != '\0'; p++) {
            if (*p == 'p') {
                def_path = 1;
            } else if (*p == 'v') {
                opt_v = 1;
                opt_V = 0;
            } else if (*p == 'V') {
                opt_V = 1;
                opt_v = 0;
            } else {
                fprintf(stderr, "command: invalid option -%c\n", *p);
                return 2;
            }
        }
        idx++;
    }
    if (idx >= argc) {
        return 0;
    }
    if (opt_v || opt_V) {
        const char *nm = argv[idx];
        const char *al = sh_alias_get(nm);
        if (al) {
            if (opt_V) {
                printf("%s is an alias for %s\n", nm, al);
            } else {
                printf("alias %s='%s'\n", nm, al);
            }
            return 0;
        }
        if (sh_is_builtin(nm)) {
            if (opt_V) {
                printf("%s is a shell builtin\n", nm);
            } else {
                printf("%s\n", nm);
            }
            return 0;
        }
        if (sh_func_find(nm)) {
            if (opt_V) {
                printf("%s is a function\n", nm);
            } else {
                printf("%s\n", nm);
            }
            return 0;
        }
        char *path = sh_find_in_path(nm, def_path);
        if (path) {
            if (opt_V) {
                printf("%s is %s\n", nm, path);
            } else {
                printf("%s\n", path);
            }
            free(path);
            return 0;
        }
        if (opt_V) {
            fprintf(stderr, "command: %s: not found\n", nm);
        }
        return 127;
    }
    return sh_exec_simple_argv(argc - idx, &argv[idx], 1, def_path);
}

static int builtin_trap(int argc, char **argv) {
    int idx = 1;
    if (idx < argc && strcmp(argv[idx], "--") == 0) {
        idx++;
    }
    if (idx >= argc) {
        for (int s = 0; s < SH_MAX_TRAPS; s++) {
            if (g_sh.traps[s]) {
                printf("trap -- '%s' %s\n", g_sh.traps[s], sh_sig_name(s));
            }
        }
        return 0;
    }
    int rem = argc - idx;
    const char *action = argv[idx];
    int reset = 0;
    if (rem == 1 && isdigit((unsigned char)action[0])) {
        reset = 1;
    } else if (strcmp(action, "-") == 0) {
        reset = 1;
        idx++;
    } else {
        idx++;
    }
    for (; idx < argc; idx++) {
        int sig = sh_parse_sig(argv[idx]);
        if (sig < 0) {
            fprintf(stderr, "trap: %s: invalid signal\n", argv[idx]);
            return 1;
        }
        free(g_sh.traps[sig]);
        g_sh.traps[sig] = NULL;
        if (reset) {
            if (sig > 0) {
                struct sigaction sa = {0};
                sa.sa_handler = SIG_DFL;
                (void)sigaction(sig, &sa, NULL);
            }
        } else {
            g_sh.traps[sig] = strdup(action);
            if (sig > 0) {
                struct sigaction sa = {0};
                sa.sa_handler = (action[0] == '\0') ? SIG_IGN : sh_trap_handler;
                (void)sigaction(sig, &sa, NULL);
            }
        }
    }
    return 0;
}

static void print_ticks(clock_t ticks, long hz) {
    unsigned long ms = (hz > 0) ? (unsigned long)((ticks * 1000L) / hz) : 0ul;
    unsigned long mins = ms / 60000ul;
    unsigned long secs = (ms % 60000ul) / 1000ul;
    unsigned long rem = ms % 1000ul;
    printf("%lum%lu.%03lus", mins, secs, rem);
}

int sh_is_special_builtin(const char *cmd) {
    if (!cmd) {
        return 0;
    }
    return strcmp(cmd, ":") == 0 || strcmp(cmd, ".") == 0 ||
           strcmp(cmd, "break") == 0 || strcmp(cmd, "continue") == 0 ||
           strcmp(cmd, "eval") == 0 || strcmp(cmd, "exec") == 0 ||
           strcmp(cmd, "exit") == 0 || strcmp(cmd, "export") == 0 ||
           strcmp(cmd, "readonly") == 0 || strcmp(cmd, "return") == 0 ||
           strcmp(cmd, "set") == 0 || strcmp(cmd, "shift") == 0 ||
           strcmp(cmd, "times") == 0 || strcmp(cmd, "trap") == 0 ||
           strcmp(cmd, "unset") == 0;
}

int sh_is_builtin(const char *cmd) {
    if (!cmd) {
        return 0;
    }
    return sh_is_special_builtin(cmd) || strcmp(cmd, "alias") == 0 ||
           strcmp(cmd, "cd") == 0 || strcmp(cmd, "command") == 0 ||
           strcmp(cmd, "echo") == 0 || strcmp(cmd, "false") == 0 ||
           strcmp(cmd, "getopts") == 0 || strcmp(cmd, "hash") == 0 ||
           strcmp(cmd, "pwd") == 0 || strcmp(cmd, "read") == 0 ||
           strcmp(cmd, "true") == 0 || strcmp(cmd, "ulimit") == 0 ||
           strcmp(cmd, "umask") == 0 || strcmp(cmd, "unalias") == 0 ||
           strcmp(cmd, "wait") == 0;
}

int sh_run_builtin(int argc, char **argv) {
    const char *cmd = argv[0];
    if (strcmp(cmd, ":") == 0 || strcmp(cmd, "true") == 0) {
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
        int idx = 1;
        while (idx < argc &&
               (strcmp(argv[idx], "-L") == 0 || strcmp(argv[idx], "-P") == 0)) {
            idx++;
        }
        if (idx < argc && strcmp(argv[idx], "--") == 0) {
            idx++;
        }
        const char *dir = (idx < argc) ? argv[idx] : sh_var_get("HOME");
        int print_dir = 0;
        if (dir && strcmp(dir, "-") == 0) {
            dir = sh_var_get("OLDPWD");
            print_dir = 1;
        }
        if (!dir || dir[0] == '\0') {
            dir = "/";
        }
        char oldcwd[PATH_MAX] = {0};
        (void)getcwd(oldcwd, sizeof(oldcwd));
        if (chdir(dir) != 0) {
            perror("cd");
            return 1;
        }
        if (oldcwd[0] != '\0') {
            (void)sh_var_set("OLDPWD", oldcwd, 1, 0);
        }
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            (void)sh_var_set("PWD", cwd, 1, 0);
            if (print_dir) {
                puts(cwd);
            }
        }
        return 0;
    }
    if (strcmp(cmd, "export") == 0 || strcmp(cmd, "readonly") == 0) {
        int is_ro = (cmd[0] == 'r');
        int idx = 1;
        if (idx < argc && strcmp(argv[idx], "-p") == 0) {
            idx++;
        }
        if (idx >= argc) {
            for (sh_var_t *v = g_sh.vars; v; v = v->next) {
                if ((is_ro && v->readonly) || (!is_ro && v->exported)) {
                    printf("%s %s='%s'\n", cmd, v->name, v->val);
                }
            }
            return 0;
        }
        int rc = 0;
        for (; idx < argc; idx++) {
            char *eq = strchr(argv[idx], '=');
            if (eq) {
                *eq = '\0';
                if (sh_var_set(argv[idx], eq + 1, !is_ro, is_ro) != 0) {
                    rc = 1;
                }
                *eq = '=';
            } else if (sh_var_set(argv[idx], NULL, !is_ro, is_ro) != 0) {
                rc = 1;
            }
        }
        return rc;
    }
    if (strcmp(cmd, "unset") == 0) {
        int unset_fn = 0;
        int idx = 1;
        if (idx < argc && strcmp(argv[idx], "-f") == 0) {
            unset_fn = 1;
            idx++;
        } else if (idx < argc && strcmp(argv[idx], "-v") == 0) {
            idx++;
        }
        int rc = 0;
        for (; idx < argc; idx++) {
            if (unset_fn) {
                sh_func_unset(argv[idx]);
            } else if (sh_var_unset(argv[idx]) != 0) {
                rc = 1;
            }
        }
        return rc;
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
    if (strcmp(cmd, ".") == 0) {
        return builtin_dot(argc, argv);
    }
    if (strcmp(cmd, "eval") == 0) {
        int idx = 1;
        if (idx < argc && strcmp(argv[idx], "--") == 0) {
            idx++;
        }
        if (idx >= argc) {
            return 0;
        }
        sh_buf_t b = {0};
        for (; idx < argc; idx++) {
            if (b.len > 0u) {
                (void)sh_buf_push(&b, ' ');
            }
            (void)sh_buf_puts(&b, argv[idx]);
        }
        int rc = sh_eval_string(b.data ? b.data : "");
        sh_buf_free(&b);
        return rc;
    }
    if (strcmp(cmd, "alias") == 0) {
        if (argc == 1) {
            for (sh_alias_t *a = g_sh.aliases; a; a = a->next) {
                printf("%s='%s'\n", a->name, a->val);
            }
            return 0;
        }
        int rc = 0;
        for (int i = 1; i < argc; i++) {
            char *eq = strchr(argv[i], '=');
            if (eq) {
                *eq = '\0';
                sh_alias_set(argv[i], eq + 1);
                *eq = '=';
            } else {
                const char *v = sh_alias_get(argv[i]);
                if (v) {
                    printf("%s='%s'\n", argv[i], v);
                } else {
                    fprintf(stderr, "alias: %s: not found\n", argv[i]);
                    rc = 1;
                }
            }
        }
        return rc;
    }
    if (strcmp(cmd, "unalias") == 0) {
        if (argc >= 2 && strcmp(argv[1], "-a") == 0) {
            sh_alias_clear();
            return 0;
        }
        int rc = 0;
        for (int i = 1; i < argc; i++) {
            if (sh_alias_unset(argv[i]) != 0) {
                fprintf(stderr, "unalias: %s: not found\n", argv[i]);
                rc = 1;
            }
        }
        return rc;
    }
    if (strcmp(cmd, "command") == 0) {
        return builtin_command(argc, argv);
    }
    if (strcmp(cmd, "getopts") == 0) {
        return builtin_getopts(argc, argv);
    }
    if (strcmp(cmd, "hash") == 0) {
        if (argc == 1) {
            for (sh_hash_t *h = g_sh.hashes; h; h = h->next) {
                printf("%u\t%s\n", h->hits, h->path);
            }
            return 0;
        }
        int idx = 1;
        if (strcmp(argv[idx], "-r") == 0) {
            sh_hash_clear();
            idx++;
        }
        int rc = 0;
        for (; idx < argc; idx++) {
            char *p = sh_find_in_path(argv[idx], 0);
            if (p) {
                sh_hash_store(argv[idx], p);
                free(p);
            } else {
                fprintf(stderr, "hash: %s: not found\n", argv[idx]);
                rc = 1;
            }
        }
        return rc;
    }
    if (strcmp(cmd, "shift") == 0) {
        int n = (argc > 1) ? atoi(argv[1]) : 1;
        if (n < 0 || n > g_sh.pos_argc) {
            fprintf(stderr, "shift: count %d out of range\n", n);
            return 1;
        }
        for (int i = 0; i < n; i++) {
            free(g_sh.pos_args[i]);
        }
        int rem = g_sh.pos_argc - n;
        for (int i = 0; i < rem; i++) {
            g_sh.pos_args[i] = g_sh.pos_args[i + n];
        }
        g_sh.pos_argc = rem;
        return 0;
    }
    if (strcmp(cmd, "return") == 0) {
        int code = (argc > 1) ? atoi(argv[1]) : g_sh.last_status;
        if (g_sh.func_depth > 0 || g_sh.dot_depth > 0) {
            g_sh.returning = 1;
            g_sh.return_code = code;
        } else {
            g_sh.should_exit = 1;
            g_sh.exit_code = code;
        }
        return code;
    }
    if (strcmp(cmd, "break") == 0) {
        int n = (argc > 1) ? atoi(argv[1]) : 1;
        if (n < 1) {
            n = 1;
        }
        if (g_sh.loop_depth > 0) {
            g_sh.break_count = (n > g_sh.loop_depth) ? g_sh.loop_depth : n;
        }
        return 0;
    }
    if (strcmp(cmd, "continue") == 0) {
        int n = (argc > 1) ? atoi(argv[1]) : 1;
        if (n < 1) {
            n = 1;
        }
        if (g_sh.loop_depth > 0) {
            g_sh.continue_count = (n > g_sh.loop_depth) ? g_sh.loop_depth : n;
        }
        return 0;
    }
    if (strcmp(cmd, "times") == 0) {
        struct tms t = {0};
        (void)times(&t);
        long hz = sysconf(_SC_CLK_TCK);
        print_ticks(t.tms_utime, hz);
        putchar(' ');
        print_ticks(t.tms_stime, hz);
        putchar('\n');
        print_ticks(t.tms_cutime, hz);
        putchar(' ');
        print_ticks(t.tms_cstime, hz);
        putchar('\n');
        (void)fflush(stdout);
        return 0;
    }
    if (strcmp(cmd, "trap") == 0) {
        return builtin_trap(argc, argv);
    }
    if (strcmp(cmd, "exit") == 0) {
        g_sh.should_exit = 1;
        g_sh.exit_code = (argc > 1) ? atoi(argv[1]) : g_sh.last_status;
        return g_sh.exit_code;
    }
    if (strcmp(cmd, "exec") == 0) {
        if (argc <= 1) {
            return 0;
        }
        int idx = 1;
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
        }
        if (idx >= argc) {
            return 0;
        }
        (void)execvp(argv[idx], &argv[idx]);
        int err = errno;
        perror(argv[idx]);
        _exit(err == ENOENT ? 127 : 126);
    }
    return 127;
}
