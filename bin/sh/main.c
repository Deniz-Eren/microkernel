/**
 * @file     main.c
 * @brief    Ring 3 command shell (sh) targeting POSIX.1-2024 XCU interfaces
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "sh.h"

/* need:: COMP_LIBC_0005 COMP_LIBC_0017 COMP_PROC_0010 */

sh_state_t g_sh;

int main(int argc, char **argv) {
    if (!getenv("PATH")) {
        (void)setenv("PATH", "/boot:/bin", 1);
    }
    g_sh.arg0 = (argc > 0) ? argv[0] : (char *)"sh";
    (void)sh_var_set("OPTIND", "1", 0, 0);

    int idx = 1;
    int opt_c = 0;
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-c") == 0) {
            opt_c = 1;
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-e") == 0) {
            g_sh.opt_errexit = 1;
            idx++;
        } else if (strcmp(argv[idx], "-u") == 0) {
            g_sh.opt_nounset = 1;
            idx++;
        } else if (strcmp(argv[idx], "-f") == 0) {
            g_sh.opt_noglob = 1;
            idx++;
        } else if (strcmp(argv[idx], "-s") == 0) {
            idx++;
        } else {
            break;
        }
    }

    if (opt_c) {
        if (idx >= argc) {
            fprintf(stderr, "sh: -c requires an argument\n");
            return 2;
        }
        const char *cmd = argv[idx++];
        if (idx < argc) {
            g_sh.arg0 = argv[idx++];
        }
        if (idx < argc) {
            sh_set_positional(argc - idx, &argv[idx]);
        }
        int rc = sh_eval_string(cmd);
        sh_run_exit_trap();
        return g_sh.should_exit ? g_sh.exit_code : rc;
    }

    if (idx < argc) {
        g_sh.arg0 = argv[idx];
        FILE *fp = fopen(argv[idx], "r");
        if (!fp) {
            perror(argv[idx]);
            return 127;
        }
        if (idx + 1 < argc) {
            sh_set_positional(argc - (idx + 1), &argv[idx + 1]);
        }
        sh_buf_t script = {0};
        char chunk[512];
        for (;;) {
            size_t n = fread(chunk, 1u, sizeof(chunk), fp);
            if (n == 0u) {
                break;
            }
            (void)sh_buf_putn(&script, chunk, n);
        }
        (void)fclose(fp);
        int rc = sh_eval_string(script.data ? script.data : "");
        sh_buf_free(&script);
        sh_run_exit_trap();
        return g_sh.should_exit ? g_sh.exit_code : rc;
    }

    int interactive = isatty(STDIN_FILENO);
    g_sh.interactive = interactive;
    if (interactive) {
        (void)signal(SIGINT, SIG_IGN);
        (void)signal(SIGQUIT, SIG_IGN);
        (void)setpgid(0, 0);
        g_sh.shell_pgid = getpgrp();
        (void)tcsetpgrp(STDIN_FILENO, g_sh.shell_pgid);
        if (getppid() == 0 && argc == 1 &&
            access("/boot/sshd-setup", F_OK) == 0) {
            pid_t sp = fork();
            if (sp == 0) {
                char *const sargv[] = {(char *)"/boot/sshd-setup", NULL};
                (void)execv("/boot/sshd-setup", sargv);
                _exit(127);
            }
            if (sp > 0) {
                (void)waitpid(sp, NULL, 0);
            }
        }
    } else {
        sh_buf_t script = {0};
        char chunk[512];
        for (;;) {
            size_t n = fread(chunk, 1u, sizeof(chunk), stdin);
            if (n == 0u) {
                break;
            }
            (void)sh_buf_putn(&script, chunk, n);
        }
        int rc = sh_eval_string(script.data ? script.data : "");
        sh_buf_free(&script);
        sh_run_exit_trap();
        return g_sh.should_exit ? g_sh.exit_code : rc;
    }

    char line[1024];
    while (!g_sh.should_exit) {
        (void)fputs("$ ", stdout);
        (void)fflush(stdout);
        if (!fgets(line, (int)sizeof(line), stdin)) {
            break;
        }
        (void)sh_eval_string(line);
    }
    sh_run_exit_trap();
    return g_sh.should_exit ? g_sh.exit_code : g_sh.last_status;
}
