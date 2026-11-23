/**
 * @file     main.c
 * @brief    CLI driver for Ring 3 POSIX.1-2024 XCU awk utility
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
#include <unistd.h>
#include "awk.h"

/* need:: COMP_LIBC_0024 */

extern char **environ;

static int is_var_assignment(const char *s, const char **eq_out) {
    if (!s || (!isalpha((unsigned char)s[0]) && s[0] != '_')) {
        return 0;
    }
    const char *p = s + 1;
    while (isalnum((unsigned char)*p) || *p == '_') {
        p++;
    }
    if (*p != '=') {
        return 0;
    }
    *eq_out = p;
    return 1;
}

static void apply_assignment(awk_vm_t *vm, const char *s, const char *eq) {
    if (!s || !eq || eq <= s) {
        return;
    }
    size_t nlen = (size_t)(eq - s);
    char *name = malloc(nlen + 1);
    if (!name) {
        return;
    }
    (void)memcpy(name, s, nlen);
    name[nlen] = '\0';
    char *val = awk_unescape(eq + 1);
    awk_var_t *v = vm_get_var(vm, name);
    if (v) {
        val_free(&v->val);
        v->val = val_numstr(val ? val : "");
    }
    free(val);
    free(name);
}

static char *append_str(char *dst, const char *src) {
    size_t dl = dst ? strlen(dst) : 0;
    size_t sl = strlen(src);
    char *nb = realloc(dst, dl + sl + 2);
    if (!nb) {
        free(dst);
        return NULL;
    }
    if (dl > 0) {
        nb[dl++] = '\n';
    }
    (void)memcpy(nb + dl, src, sl + 1);
    return nb;
}

static char *read_file_all(const char *path) {
    FILE *fp = (strcmp(path, "-") == 0) ? stdin : fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }
    size_t cap = 512;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        if (fp != stdin) {
            (void)fclose(fp);
        }
        return NULL;
    }
    for (;;) {
        size_t nr = fread(buf + len, 1, cap - len - 1, fp);
        len += nr;
        if (nr == 0) {
            break;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                if (fp != stdin) {
                    (void)fclose(fp);
                }
                return NULL;
            }
            buf = nb;
        }
    }
    buf[len] = '\0';
    if (fp != stdin) {
        (void)fclose(fp);
    }
    return buf;
}

static int has_input_rules(const prog_t *prog) {
    for (size_t i = 0; i < prog->nrules; i++) {
        if (prog->rules[i].kind != RULE_BEGIN) {
            return 1;
        }
    }
    return 0;
}

static int run_stream(awk_vm_t *vm, FILE *fp, const char *name) {
    vm->cur_in = fp;
    vm_set_str(vm, "FILENAME", name);
    vm_set_num(vm, "FNR", 0.0);
    double fnr = 0.0;
    awk_var_t *nr_var = vm_get_var(vm, "NR");
    double nr = nr_var ? val_to_num(&nr_var->val) : 0.0;

    char *rec = NULL;
    while (!vm->exiting) {
        int rr = vm_read_record(vm, fp, &rec);
        if (rr <= 0) {
            break;
        }
        nr += 1.0;
        fnr += 1.0;
        vm_set_num(vm, "NR", nr);
        vm_set_num(vm, "FNR", fnr);
        vm_run_record(vm, rec);
        free(rec);
        rec = NULL;
    }
    vm->cur_in = NULL;
    return 0;
}

int main(int argc, char **argv) {
    const char *fs_arg = NULL;
    char *prog_src = NULL;
    const char **v_assigns = NULL;
    size_t nv = 0;
    int opt = 0;

    while ((opt = getopt(argc, argv, "F:f:v:")) != -1) {
        switch (opt) {
        case 'F':
            fs_arg = optarg;
            break;
        case 'f': {
            char *txt = read_file_all(optarg);
            if (!txt) {
                free(prog_src);
                free(v_assigns);
                return 1;
            }
            prog_src = append_str(prog_src, txt);
            free(txt);
            if (!prog_src) {
                free(v_assigns);
                return 1;
            }
            break;
        }
        case 'v': {
            const char **na = realloc(v_assigns, (nv + 1) * sizeof(*na));
            if (!na) {
                free(prog_src);
                free(v_assigns);
                return 1;
            }
            v_assigns = na;
            v_assigns[nv++] = optarg;
            break;
        }
        default:
            (void)fprintf(stderr,
                          "usage: awk [-F ere] [-v var=val] [-f progfile | "
                          "'program'] [file...]\n");
            free(prog_src);
            free(v_assigns);
            return 1;
        }
    }

    if (!prog_src) {
        if (optind >= argc) {
            (void)fprintf(stderr, "awk: missing program\n");
            free(v_assigns);
            return 1;
        }
        prog_src = strdup(argv[optind++]);
        if (!prog_src) {
            free(v_assigns);
            return 1;
        }
    }

    prog_t prog;
    if (parse_program(prog_src, &prog) != 0) {
        free(prog_src);
        free(v_assigns);
        return 1;
    }
    free(prog_src);

    awk_vm_t vm;
    vm_init(&vm, &prog);

    if (fs_arg) {
        char *fs = awk_unescape(fs_arg);
        vm_set_str(&vm, "FS", fs ? fs : fs_arg);
        free(fs);
    }

    /* Populate ENVIRON */
    awk_var_t *env_arr = vm_get_var(&vm, "ENVIRON");
    if (env_arr) {
        env_arr->is_arr = 1;
        if (environ) {
            for (size_t i = 0; environ[i] != NULL; i++) {
                const char *eq = strchr(environ[i], '=');
                if (!eq) {
                    continue;
                }
                size_t kl = (size_t)(eq - environ[i]);
                char *k = malloc(kl + 1);
                if (k) {
                    (void)memcpy(k, environ[i], kl);
                    k[kl] = '\0';
                    vm_arr_set(env_arr, k, val_str(eq + 1));
                    free(k);
                }
            }
        }
    }

    /* Populate ARGC / ARGV */
    int awk_argc = 1 + (argc - optind);
    vm_set_num(&vm, "ARGC", (double)awk_argc);
    awk_var_t *argv_arr = vm_get_var(&vm, "ARGV");
    if (argv_arr) {
        argv_arr->is_arr = 1;
        vm_arr_set(argv_arr, "0", val_str(argv[0]));
        for (int i = optind; i < argc; i++) {
            char key[32];
            (void)snprintf(key, sizeof(key), "%d", 1 + (i - optind));
            vm_arr_set(argv_arr, key, val_str(argv[i]));
        }
    }

    /* Apply -v assignments before BEGIN */
    for (size_t i = 0; i < nv; i++) {
        const char *eq = NULL;
        if (is_var_assignment(v_assigns[i], &eq)) {
            apply_assignment(&vm, v_assigns[i], eq);
        }
    }
    free(v_assigns);

    vm_run_begin(&vm);

    int rc = 0;
    if (!vm.exiting && has_input_rules(&prog)) {
        int any_file = 0;
        for (int i = optind; i < argc && !vm.exiting; i++) {
            const char *eq = NULL;
            if (is_var_assignment(argv[i], &eq)) {
                apply_assignment(&vm, argv[i], eq);
                continue;
            }
            any_file = 1;
            FILE *fp =
                (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "rb");
            if (!fp) {
                perror(argv[i]);
                rc = 1;
                continue;
            }
            (void)run_stream(&vm, fp, argv[i]);
            if (fp != stdin) {
                (void)fclose(fp);
            }
        }
        if (!any_file && !vm.exiting) {
            (void)run_stream(&vm, stdin, "");
        }
    }

    vm_run_end(&vm);
    int exit_code = (vm.exit_code != 0) ? vm.exit_code : rc;
    vm_free(&vm);
    prog_free(&prog);
    return exit_code;
}
