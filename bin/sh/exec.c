/**
 * @file     exec.c
 * @brief    POSIX.1-2024 shell AST executor, pipelines, redirections, and loops
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sh.h"

/* need:: COMP_LIBC_0017 COMP_PROC_0010 */

struct saved_fds {
    int orig[SH_MAX_REDIRS];
    int copy[SH_MAX_REDIRS];
    int count;
};

static int save_fd_once(struct saved_fds *sf, int fd) {
    for (int i = 0; i < sf->count; i++) {
        if (sf->orig[i] == fd) {
            return 0;
        }
    }
    if (sf->count >= SH_MAX_REDIRS) {
        return -1;
    }
    int dupfd = fcntl(fd, F_DUPFD_CLOEXEC, 10);
    sf->orig[sf->count] = fd;
    sf->copy[sf->count] = dupfd; /* -1 means fd was closed */
    sf->count++;
    return 0;
}

static void restore_fds(const struct saved_fds *sf) {
    (void)fflush(stdout);
    (void)fflush(stderr);
    for (int i = sf->count - 1; i >= 0; i--) {
        if (sf->copy[i] >= 0) {
            (void)dup2(sf->copy[i], sf->orig[i]);
            (void)close(sf->copy[i]);
        } else {
            (void)close(sf->orig[i]);
        }
    }
}

static int apply_heredoc(const sh_redir_t *r) {
    int err = 0;
    char *body = r->quoted
                     ? strdup(r->target ? r->target : "")
                     : sh_expand_heredoc(r->target ? r->target : "", &err);
    if (!body || err) {
        free(body);
        return -1;
    }
    size_t len = strlen(body);
    int pfd[2] = {-1, -1};
    if (len <= 3072u && pipe2(pfd, O_CLOEXEC) == 0) {
        size_t off = 0u;
        while (off < len) {
            ssize_t w = write(pfd[1], body + off, len - off);
            if (w <= 0) {
                break;
            }
            off += (size_t)w;
        }
        (void)close(pfd[1]);
        free(body);
        if (pfd[0] != r->fd) {
            if (dup2(pfd[0], r->fd) < 0) {
                (void)close(pfd[0]);
                return -1;
            }
            (void)close(pfd[0]);
        }
        return 0;
    }
    if (pfd[0] >= 0) {
        (void)close(pfd[0]);
        (void)close(pfd[1]);
    }
    char path[64];
    (void)snprintf(path, sizeof(path), "/tmp/.sh_hd_%ld", (long)getpid());
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600u);
    if (fd < 0) {
        free(body);
        return -1;
    }
    (void)unlink(path);
    size_t off = 0u;
    while (off < len) {
        ssize_t w = write(fd, body + off, len - off);
        if (w <= 0) {
            break;
        }
        off += (size_t)w;
    }
    free(body);
    (void)lseek(fd, 0, SEEK_SET);
    if (fd != r->fd) {
        if (dup2(fd, r->fd) < 0) {
            (void)close(fd);
            return -1;
        }
        (void)close(fd);
    }
    return 0;
}

int sh_apply_redirs(const sh_redir_t *redirs, size_t nredirs) {
    for (size_t i = 0u; i < nredirs; i++) {
        const sh_redir_t *r = &redirs[i];
        if (r->kind == T_HEREDOC || r->kind == T_HEREDOC_STRIP) {
            if (apply_heredoc(r) != 0) {
                return -1;
            }
            continue;
        }
        int err = 0;
        char *tgt = sh_expand_single(r->target, &err);
        if (!tgt || err) {
            free(tgt);
            return -1;
        }
        if (r->kind == T_DUP_IN || r->kind == T_DUP_OUT) {
            if (strcmp(tgt, "-") == 0) {
                (void)close(r->fd);
                free(tgt);
                continue;
            }
            char *end = NULL;
            long src_fd = strtol(tgt, &end, 10);
            if (!end || *end != '\0' || src_fd < 0) {
                fprintf(stderr, "sh: %s: bad file descriptor\n", tgt);
                free(tgt);
                return -1;
            }
            free(tgt);
            if (dup2((int)src_fd, r->fd) < 0) {
                perror("dup2");
                return -1;
            }
            continue;
        }

        int fl = O_RDONLY;
        if (r->kind == T_REDIR_OUT || r->kind == T_REDIR_CLOBBER) {
            fl = O_WRONLY | O_CREAT | O_TRUNC;
        } else if (r->kind == T_REDIR_APP) {
            fl = O_WRONLY | O_CREAT | O_APPEND;
        } else if (r->kind == T_REDIR_RDWR) {
            fl = O_RDWR | O_CREAT;
        }
        int fd = open(tgt, fl, (mode_t)0666u);
        if (fd < 0) {
            perror(tgt);
            free(tgt);
            return -1;
        }
        free(tgt);
        if (fd != r->fd) {
            if (dup2(fd, r->fd) < 0) {
                perror("dup2");
                (void)close(fd);
                return -1;
            }
            (void)close(fd);
        }
    }
    return 0;
}

static int apply_saved_redirs(const sh_redir_t *redirs, size_t nredirs,
                              struct saved_fds *sf) {
    sf->count = 0;
    for (size_t i = 0u; i < nredirs; i++) {
        (void)save_fd_once(sf, redirs[i].fd);
    }
    if (sh_apply_redirs(redirs, nredirs) != 0) {
        restore_fds(sf);
        return -1;
    }
    return 0;
}

static int should_stop(void) {
    return g_sh.should_exit || g_sh.returning || g_sh.break_count > 0 ||
           g_sh.continue_count > 0;
}

static int exec_function(sh_func_t *fn, int argc, char **argv) {
    char **saved_args = g_sh.pos_args;
    int saved_argc = g_sh.pos_argc;
    g_sh.pos_args = NULL;
    g_sh.pos_argc = 0;
    if (argc > 1) {
        g_sh.pos_args = (char **)calloc((size_t)(argc - 1), sizeof(char *));
        if (g_sh.pos_args) {
            for (int i = 1; i < argc; i++) {
                g_sh.pos_args[i - 1] = strdup(argv[i]);
            }
            g_sh.pos_argc = argc - 1;
        }
    }
    g_sh.func_depth++;
    int rc = sh_exec_node(fn->body);
    g_sh.func_depth--;
    if (g_sh.returning) {
        rc = g_sh.return_code;
        g_sh.returning = 0;
    }
    for (int i = 0; i < g_sh.pos_argc; i++) {
        free(g_sh.pos_args[i]);
    }
    free(g_sh.pos_args);
    g_sh.pos_args = saved_args;
    g_sh.pos_argc = saved_argc;
    return rc;
}

int sh_exec_simple_argv(int argc, char **argv, int bypass_funcs,
                        int default_path) {
    if (argc <= 0) {
        return 0;
    }
    if (!bypass_funcs) {
        sh_func_t *fn = sh_func_find(argv[0]);
        if (fn && !sh_is_special_builtin(argv[0])) {
            return exec_function(fn, argc, argv);
        }
    }
    if (sh_is_builtin(argv[0])) {
        return sh_run_builtin(argc, argv);
    }

    char *resolved = NULL;
    if (strchr(argv[0], '/') != NULL) {
        resolved = strdup(argv[0]);
    } else if (!default_path) {
        const char *cached = sh_hash_lookup(argv[0]);
        if (cached) {
            resolved = strdup(cached);
        }
    }
    if (!resolved) {
        resolved = sh_find_in_path(argv[0], default_path);
        if (resolved && !default_path && strchr(argv[0], '/') == NULL) {
            sh_hash_store(argv[0], resolved);
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(resolved);
        return 1;
    }
    if (pid == 0) {
        if (g_sh.interactive) {
            (void)setpgid(0, getpid());
            (void)signal(SIGINT, SIG_DFL);
            (void)signal(SIGQUIT, SIG_DFL);
        }
        if (resolved) {
            (void)execv(resolved, argv);
        } else {
            (void)execvp(argv[0], argv);
        }
        int err = errno;
        perror(argv[0]);
        _exit(err == ENOENT ? 127 : 126);
    }
    free(resolved);
    if (g_sh.interactive) {
        (void)setpgid(pid, pid);
        (void)tcsetpgrp(STDIN_FILENO, pid);
    }
    int wst = 0;
    while (waitpid(pid, &wst, 0) < 0 && errno == EINTR) {
    }
    if (g_sh.interactive && g_sh.shell_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, g_sh.shell_pgid);
    }
    if (WIFEXITED(wst)) {
        return WEXITSTATUS(wst);
    }
    if (WIFSIGNALED(wst)) {
        return 128 + WTERMSIG(wst);
    }
    return 1;
}

static int exec_simple(const sh_node_t *n) {
    sh_vec_t words = {0};
    if (sh_expand_words(&n->u.simple.words, &words) != 0) {
        return 1;
    }

    /* Pure assignments (and optional redirections) with no command word. */
    if (words.len == 0u) {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            sh_vec_free(&words);
            return 1;
        }
        int rc = 0;
        for (size_t i = 0u; i < n->u.simple.assigns.len; i++) {
            const char *raw = n->u.simple.assigns.items[i];
            const char *eq = strchr(raw, '=');
            if (!eq) {
                continue;
            }
            char *nm = strndup(raw, (size_t)(eq - raw));
            int err = 0;
            char *val = sh_expand_single(eq + 1, &err);
            if (!nm || err ||
                sh_var_set(nm, val ? val : "", g_sh.opt_allexport, 0) != 0) {
                rc = 1;
            }
            free(nm);
            free(val);
        }
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        sh_vec_free(&words);
        return rc;
    }

    /* Special case: bare 'exec' with redirections applies them permanently. */
    if (strcmp(words.items[0], "exec") == 0 && words.len == 1u) {
        int rc = (sh_apply_redirs(n->redirs, n->nredirs) == 0) ? 0 : 1;
        sh_vec_free(&words);
        return rc;
    }

    /* Ensure NULL terminator for argv. */
    (void)sh_vec_push(&words, NULL);
    int argc = (int)(words.len - 1u);
    char **argv = words.items;

    sh_func_t *fn = sh_func_find(argv[0]);
    int is_bi = sh_is_builtin(argv[0]);

    if (is_bi || fn) {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            sh_vec_free(&words);
            return 1;
        }
        /* Save/apply per-command variable prefixes for non-special built-ins.
         */
        int persist = sh_is_special_builtin(argv[0]);
        char *saved_names[16];
        char *saved_vals[16];
        int saved_present[16];
        size_t nsaved = 0u;
        for (size_t i = 0u; i < n->u.simple.assigns.len; i++) {
            const char *raw = n->u.simple.assigns.items[i];
            const char *eq = strchr(raw, '=');
            if (!eq) {
                continue;
            }
            char *nm = strndup(raw, (size_t)(eq - raw));
            int err = 0;
            char *val = sh_expand_single(eq + 1, &err);
            if (!persist && nsaved < 16u && nm) {
                const char *old = sh_var_get(nm);
                saved_names[nsaved] = strdup(nm);
                saved_present[nsaved] = (old != NULL);
                saved_vals[nsaved] = old ? strdup(old) : NULL;
                nsaved++;
            }
            if (nm && !err) {
                (void)sh_var_set(nm, val ? val : "", 1, 0);
            }
            free(nm);
            free(val);
        }

        int rc = (fn && !persist) ? exec_function(fn, argc, argv)
                                  : sh_run_builtin(argc, argv);

        for (size_t i = 0u; i < nsaved; i++) {
            if (saved_present[i]) {
                (void)sh_var_set(saved_names[i], saved_vals[i], 0, 0);
            } else {
                (void)sh_var_unset(saved_names[i]);
            }
            free(saved_names[i]);
            free(saved_vals[i]);
        }
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        sh_vec_free(&words);
        return rc;
    }

    /* External command: fork, apply redirs + temporary env assigns, exec. */
    char *resolved = NULL;
    if (strchr(argv[0], '/') != NULL) {
        resolved = strdup(argv[0]);
    } else {
        const char *cached = sh_hash_lookup(argv[0]);
        if (cached) {
            resolved = strdup(cached);
        } else {
            resolved = sh_find_in_path(argv[0], 0);
            if (resolved) {
                sh_hash_store(argv[0], resolved);
            }
        }
    }

    /* Expand per-command assignments before fork so side effects stay clean. */
    sh_vec_t env_names = {0};
    sh_vec_t env_vals = {0};
    for (size_t i = 0u; i < n->u.simple.assigns.len; i++) {
        const char *raw = n->u.simple.assigns.items[i];
        const char *eq = strchr(raw, '=');
        if (!eq) {
            continue;
        }
        int err = 0;
        char *val = sh_expand_single(eq + 1, &err);
        if (!err) {
            (void)sh_vec_push(&env_names, strndup(raw, (size_t)(eq - raw)));
            (void)sh_vec_push(&env_vals, val ? val : strdup(""));
        } else {
            free(val);
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(resolved);
        sh_vec_free(&env_names);
        sh_vec_free(&env_vals);
        sh_vec_free(&words);
        return 1;
    }
    if (pid == 0) {
        if (g_sh.interactive) {
            (void)setpgid(0, getpid());
            (void)signal(SIGINT, SIG_DFL);
            (void)signal(SIGQUIT, SIG_DFL);
        }
        if (sh_apply_redirs(n->redirs, n->nredirs) != 0) {
            _exit(1);
        }
        for (size_t i = 0u; i < env_names.len; i++) {
            (void)setenv(env_names.items[i], env_vals.items[i], 1);
        }
        if (resolved) {
            (void)execv(resolved, argv);
        } else {
            (void)execvp(argv[0], argv);
        }
        int err = errno;
        perror(argv[0]);
        _exit(err == ENOENT ? 127 : 126);
    }

    free(resolved);
    sh_vec_free(&env_names);
    sh_vec_free(&env_vals);
    sh_vec_free(&words);

    if (g_sh.interactive) {
        (void)setpgid(pid, pid);
        (void)tcsetpgrp(STDIN_FILENO, pid);
    }
    int wst = 0;
    while (waitpid(pid, &wst, 0) < 0 && errno == EINTR) {
    }
    if (g_sh.interactive && g_sh.shell_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, g_sh.shell_pgid);
    }
    if (WIFEXITED(wst)) {
        return WEXITSTATUS(wst);
    }
    if (WIFSIGNALED(wst)) {
        return 128 + WTERMSIG(wst);
    }
    return 1;
}

static int exec_pipeline(const sh_node_t *n) {
    size_t nst = n->u.pipe.nstages;
    if (nst == 0u) {
        return 0;
    }
    if (nst == 1u) {
        int saved_e = g_sh.opt_errexit;
        if (n->u.pipe.negated) {
            g_sh.opt_errexit = 0;
        }
        int rc = sh_exec_node(n->u.pipe.stages[0]);
        g_sh.opt_errexit = saved_e;
        return n->u.pipe.negated ? (rc == 0 ? 1 : 0) : rc;
    }
    if (nst > SH_MAX_STAGES) {
        nst = SH_MAX_STAGES;
    }

    int pipes[SH_MAX_STAGES][2];
    for (size_t i = 0u; i + 1u < nst; i++) {
        if (pipe2(pipes[i], O_CLOEXEC) != 0) {
            perror("pipe2");
            for (size_t j = 0u; j < i; j++) {
                (void)close(pipes[j][0]);
                (void)close(pipes[j][1]);
            }
            return 1;
        }
    }

    pid_t pids[SH_MAX_STAGES];
    for (size_t i = 0u; i < nst; i++) {
        pids[i] = -1;
    }
    pid_t job_pgid = 0;

    for (size_t i = 0u; i < nst; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            if (g_sh.interactive) {
                pid_t pg = (job_pgid > 0) ? job_pgid : getpid();
                (void)setpgid(0, pg);
                (void)signal(SIGINT, SIG_DFL);
                (void)signal(SIGQUIT, SIG_DFL);
            }
            if (i > 0u) {
                (void)dup2(pipes[i - 1u][0], STDIN_FILENO);
            }
            if (i + 1u < nst) {
                (void)dup2(pipes[i][1], STDOUT_FILENO);
            }
            for (size_t j = 0u; j + 1u < nst; j++) {
                (void)close(pipes[j][0]);
                (void)close(pipes[j][1]);
            }
            g_sh.interactive = 0;
            g_sh.traps[0] = NULL;
            int rc = sh_exec_node(n->u.pipe.stages[i]);
            (void)fflush(stdout);
            (void)fflush(stderr);
            _exit(rc);
        }
        pids[i] = pid;
        if (g_sh.interactive) {
            if (job_pgid == 0) {
                job_pgid = pid;
            }
            (void)setpgid(pid, job_pgid);
        }
    }

    if (g_sh.interactive && job_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, job_pgid);
    }
    for (size_t j = 0u; j + 1u < nst; j++) {
        (void)close(pipes[j][0]);
        (void)close(pipes[j][1]);
    }

    int statuses[SH_MAX_STAGES];
    for (size_t i = 0u; i < nst; i++) {
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
    if (g_sh.interactive && g_sh.shell_pgid > 0) {
        (void)tcsetpgrp(STDIN_FILENO, g_sh.shell_pgid);
    }

    int pipe_rc = 0;
    if (g_sh.opt_pipefail) {
        for (size_t i = 0u; i < nst; i++) {
            if (statuses[i] != 0) {
                pipe_rc = statuses[i];
            }
        }
    } else {
        pipe_rc = statuses[nst - 1u];
    }
    return n->u.pipe.negated ? (pipe_rc == 0 ? 1 : 0) : pipe_rc;
}

static int exec_cond_node(const sh_node_t *n) {
    int saved_e = g_sh.opt_errexit;
    g_sh.opt_errexit = 0;
    int rc = sh_exec_node(n);
    g_sh.opt_errexit = saved_e;
    return rc;
}

int sh_exec_node(const sh_node_t *n) {
    if (!n || should_stop()) {
        return g_sh.last_status;
    }
    sh_check_traps();

    int rc = 0;
    switch (n->kind) {
    case N_SIMPLE:
        rc = exec_simple(n);
        g_sh.last_status = rc;
        if (g_sh.opt_errexit && rc != 0 && !should_stop()) {
            g_sh.should_exit = 1;
            g_sh.exit_code = rc;
        }
        break;
    case N_PIPELINE:
        rc = exec_pipeline(n);
        g_sh.last_status = rc;
        if (g_sh.opt_errexit && !n->u.pipe.negated && rc != 0 &&
            !should_stop()) {
            g_sh.should_exit = 1;
            g_sh.exit_code = rc;
        }
        break;
    case N_AND_OR: {
        int lrc = exec_cond_node(n->u.and_or.left);
        g_sh.last_status = lrc;
        if (should_stop()) {
            rc = lrc;
            break;
        }
        if ((!n->u.and_or.is_or && lrc == 0) ||
            (n->u.and_or.is_or && lrc != 0)) {
            rc = sh_exec_node(n->u.and_or.right);
            g_sh.last_status = rc;
        } else {
            rc = lrc;
        }
        break;
    }
    case N_LIST:
        rc = g_sh.last_status;
        for (size_t i = 0u; i < n->u.list.nitems; i++) {
            if (should_stop()) {
                break;
            }
            if (n->u.list.bg[i]) {
                pid_t pid = fork();
                if (pid == 0) {
                    g_sh.interactive = 0;
                    g_sh.traps[0] = NULL;
                    int brc = sh_exec_node(n->u.list.items[i]);
                    (void)fflush(stdout);
                    (void)fflush(stderr);
                    _exit(brc);
                }
                if (pid > 0) {
                    g_sh.last_bg_pid = pid;
                    rc = 0;
                    g_sh.last_status = 0;
                } else {
                    rc = 1;
                    g_sh.last_status = 1;
                }
            } else {
                rc = sh_exec_node(n->u.list.items[i]);
                g_sh.last_status = rc;
            }
            sh_check_traps();
        }
        break;
    case N_SUBSHELL: {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            rc = 1;
            break;
        }
        if (pid == 0) {
            g_sh.interactive = 0;
            g_sh.traps[0] = NULL;
            if (n->nredirs > 0u &&
                sh_apply_redirs(n->redirs, n->nredirs) != 0) {
                _exit(1);
            }
            int src = sh_exec_node(n->u.group.body);
            sh_run_exit_trap();
            (void)fflush(stdout);
            (void)fflush(stderr);
            _exit(g_sh.should_exit ? g_sh.exit_code : src);
        }
        int wst = 0;
        while (waitpid(pid, &wst, 0) < 0 && errno == EINTR) {
        }
        rc = WIFEXITED(wst) ? WEXITSTATUS(wst)
                            : (WIFSIGNALED(wst) ? 128 + WTERMSIG(wst) : 1);
        g_sh.last_status = rc;
        break;
    }
    case N_BRACE: {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            rc = 1;
            g_sh.last_status = 1;
            break;
        }
        rc = sh_exec_node(n->u.group.body);
        g_sh.last_status = rc;
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        break;
    }
    case N_IF: {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            rc = 1;
            g_sh.last_status = 1;
            break;
        }
        int matched = 0;
        rc = 0;
        for (size_t i = 0u; i < n->u.if_cmd.nbranches; i++) {
            if (exec_cond_node(n->u.if_cmd.conds[i]) == 0) {
                rc = sh_exec_node(n->u.if_cmd.bodies[i]);
                matched = 1;
                break;
            }
            if (should_stop()) {
                break;
            }
        }
        if (!matched && !should_stop() && n->u.if_cmd.else_body) {
            rc = sh_exec_node(n->u.if_cmd.else_body);
        }
        g_sh.last_status = rc;
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        break;
    }
    case N_FOR: {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            rc = 1;
            g_sh.last_status = 1;
            break;
        }
        sh_vec_t vals = {0};
        if (n->u.for_cmd.has_in) {
            if (sh_expand_words(&n->u.for_cmd.words, &vals) != 0) {
                if (n->nredirs > 0u) {
                    restore_fds(&sf);
                }
                return 1;
            }
        } else {
            for (int i = 0; i < g_sh.pos_argc; i++) {
                (void)sh_vec_push(&vals, strdup(g_sh.pos_args[i]));
            }
        }
        g_sh.loop_depth++;
        rc = 0;
        for (size_t i = 0u; i < vals.len; i++) {
            (void)sh_var_set(n->u.for_cmd.var_name, vals.items[i],
                             g_sh.opt_allexport, 0);
            rc = sh_exec_node(n->u.for_cmd.body);
            g_sh.last_status = rc;
            if (g_sh.break_count > 0) {
                g_sh.break_count--;
                break;
            }
            if (g_sh.continue_count > 0) {
                g_sh.continue_count--;
                if (g_sh.continue_count > 0) {
                    break;
                }
                continue;
            }
            if (g_sh.should_exit || g_sh.returning) {
                break;
            }
        }
        g_sh.loop_depth--;
        sh_vec_free(&vals);
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        break;
    }
    case N_WHILE:
    case N_UNTIL: {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            rc = 1;
            g_sh.last_status = 1;
            break;
        }
        g_sh.loop_depth++;
        rc = 0;
        for (;;) {
            int crc = exec_cond_node(n->u.loop.cond);
            if (should_stop()) {
                break;
            }
            if ((n->kind == N_WHILE && crc != 0) ||
                (n->kind == N_UNTIL && crc == 0)) {
                break;
            }
            rc = sh_exec_node(n->u.loop.body);
            g_sh.last_status = rc;
            if (g_sh.break_count > 0) {
                g_sh.break_count--;
                break;
            }
            if (g_sh.continue_count > 0) {
                g_sh.continue_count--;
                if (g_sh.continue_count > 0) {
                    break;
                }
                continue;
            }
            if (g_sh.should_exit || g_sh.returning) {
                break;
            }
        }
        g_sh.loop_depth--;
        g_sh.last_status = rc;
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        break;
    }
    case N_CASE: {
        struct saved_fds sf = {.count = 0};
        if (n->nredirs > 0u &&
            apply_saved_redirs(n->redirs, n->nredirs, &sf) != 0) {
            rc = 1;
            g_sh.last_status = 1;
            break;
        }
        int err = 0;
        char *word = sh_expand_single(n->u.case_cmd.word, &err);
        rc = 0;
        if (!err && word) {
            int matched = 0;
            for (size_t i = 0u; !matched && i < n->u.case_cmd.nitems; i++) {
                const sh_case_item_t *ci = &n->u.case_cmd.items[i];
                for (size_t j = 0u; j < ci->patterns.len; j++) {
                    int perr = 0;
                    char *pat = sh_expand_single(ci->patterns.items[j], &perr);
                    int ok = (!perr && pat && fnmatch(pat, word, 0) == 0);
                    free(pat);
                    if (ok) {
                        rc = ci->body ? sh_exec_node(ci->body) : 0;
                        matched = 1;
                        break;
                    }
                }
            }
        } else {
            rc = 1;
        }
        free(word);
        g_sh.last_status = rc;
        if (n->nredirs > 0u) {
            restore_fds(&sf);
        }
        break;
    }
    case N_FUNCDEF:
        sh_func_set(n->u.funcdef.name, sh_node_clone(n->u.funcdef.body));
        rc = 0;
        g_sh.last_status = 0;
        break;
    }
    sh_check_traps();
    return rc;
}

int sh_eval_string(const char *src) {
    sh_tokvec_t tv = {0};
    if (sh_lex(src, &tv) != 0) {
        g_sh.last_status = 2;
        return 2;
    }
    int perr = 0;
    sh_node_t *root = sh_parse(&tv, &perr);
    sh_tokvec_free(&tv);
    if (perr || !root) {
        sh_node_free(root);
        g_sh.last_status = 2;
        return 2;
    }
    int rc = sh_exec_node(root);
    sh_node_free(root);
    return g_sh.should_exit ? g_sh.exit_code : rc;
}
