/**
 * @file     locale.c
 * @brief    Ring 3 locale utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

static const char *const k_cats[] = {
    "LC_CTYPE",   "LC_NUMERIC",  "LC_TIME",
    "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES",
};

static void print_env_summary(void) {
    const char *lang = getenv("LANG");
    const char *lc_all = getenv("LC_ALL");
    const char *def = (lang && lang[0] != '\0') ? lang : "C";
    printf("LANG=%s\n", lang ? lang : "");
    for (size_t i = 0u; i < sizeof(k_cats) / sizeof(k_cats[0]); i++) {
        const char *v = getenv(k_cats[i]);
        if (lc_all && lc_all[0] != '\0') {
            printf("%s=\"%s\"\n", k_cats[i], lc_all);
        } else if (v && v[0] != '\0') {
            printf("%s=%s\n", k_cats[i], v);
        } else {
            printf("%s=\"%s\"\n", k_cats[i], def);
        }
    }
    printf("LC_ALL=%s\n", lc_all ? lc_all : "");
}

static void print_kv(int show_k, const char *key, const char *val, int quote) {
    if (show_k) {
        if (quote) {
            printf("%s=\"%s\"\n", key, val);
        } else {
            printf("%s=%s\n", key, val);
        }
    } else {
        printf("%s\n", val);
    }
}

static int print_keyword(const char *name, int show_c, int show_k) {
    const struct lconv *lc = localeconv();
    if (strcmp(name, "charmap") == 0) {
        if (show_c) {
            puts("LC_CTYPE");
        }
        print_kv(show_k, "charmap", nl_langinfo(CODESET), 1);
        return 0;
    }
    if (strcmp(name, "LC_CTYPE") == 0) {
        if (show_c) {
            puts("LC_CTYPE");
        }
        print_kv(show_k, "charmap", nl_langinfo(CODESET), 1);
        return 0;
    }
    if (strcmp(name, "decimal_point") == 0 || strcmp(name, "LC_NUMERIC") == 0) {
        if (show_c) {
            puts("LC_NUMERIC");
        }
        print_kv(show_k, "decimal_point", lc->decimal_point, 1);
        if (strcmp(name, "LC_NUMERIC") == 0) {
            print_kv(show_k, "thousands_sep", lc->thousands_sep, 1);
        }
        return 0;
    }
    if (strcmp(name, "thousands_sep") == 0) {
        if (show_c) {
            puts("LC_NUMERIC");
        }
        print_kv(show_k, "thousands_sep", lc->thousands_sep, 1);
        return 0;
    }
    if (strcmp(name, "yesexpr") == 0 || strcmp(name, "LC_MESSAGES") == 0) {
        if (show_c) {
            puts("LC_MESSAGES");
        }
        print_kv(show_k, "yesexpr", nl_langinfo(YESEXPR), 1);
        if (strcmp(name, "LC_MESSAGES") == 0) {
            print_kv(show_k, "noexpr", nl_langinfo(NOEXPR), 1);
        }
        return 0;
    }
    if (strcmp(name, "noexpr") == 0) {
        if (show_c) {
            puts("LC_MESSAGES");
        }
        print_kv(show_k, "noexpr", nl_langinfo(NOEXPR), 1);
        return 0;
    }
    if (strcmp(name, "d_t_fmt") == 0 || strcmp(name, "LC_TIME") == 0) {
        if (show_c) {
            puts("LC_TIME");
        }
        print_kv(show_k, "d_t_fmt", nl_langinfo(D_T_FMT), 1);
        if (strcmp(name, "LC_TIME") == 0) {
            print_kv(show_k, "d_fmt", nl_langinfo(D_FMT), 1);
            print_kv(show_k, "t_fmt", nl_langinfo(T_FMT), 1);
        }
        return 0;
    }
    if (strcmp(name, "d_fmt") == 0) {
        if (show_c) {
            puts("LC_TIME");
        }
        print_kv(show_k, "d_fmt", nl_langinfo(D_FMT), 1);
        return 0;
    }
    if (strcmp(name, "t_fmt") == 0) {
        if (show_c) {
            puts("LC_TIME");
        }
        print_kv(show_k, "t_fmt", nl_langinfo(T_FMT), 1);
        return 0;
    }
    if (strcmp(name, "currency_symbol") == 0 ||
        strcmp(name, "LC_MONETARY") == 0) {
        if (show_c) {
            puts("LC_MONETARY");
        }
        print_kv(show_k, "currency_symbol", lc->currency_symbol, 1);
        if (strcmp(name, "LC_MONETARY") == 0) {
            print_kv(show_k, "int_curr_symbol", lc->int_curr_symbol, 1);
        }
        return 0;
    }
    if (strcmp(name, "LC_COLLATE") == 0) {
        if (show_c) {
            puts("LC_COLLATE");
        }
        return 0;
    }
    fprintf(stderr, "locale: unknown keyword '%s'\n", name);
    return 1;
}

int main(int argc, char **argv) {
    (void)setlocale(LC_ALL, "");
    int opt_a = 0;
    int opt_m = 0;
    int opt_c = 0;
    int opt_k = 0;
    int idx = 1;
    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        for (const char *p = argv[idx] + 1; *p != '\0'; p++) {
            if (*p == 'a') {
                opt_a = 1;
            } else if (*p == 'm') {
                opt_m = 1;
            } else if (*p == 'c') {
                opt_c = 1;
            } else if (*p == 'k') {
                opt_k = 1;
            } else {
                fprintf(stderr, "locale: unknown option '-%c'\n", *p);
                return 1;
            }
        }
        idx++;
    }
    if (opt_a) {
        puts("C");
        puts("POSIX");
        puts("C.UTF-8");
        return 0;
    }
    if (opt_m) {
        puts("UTF-8");
        puts("US-ASCII");
        puts("ISO-8859-1");
        puts("UTF-16");
        puts("UTF-16LE");
        puts("UTF-16BE");
        puts("UTF-32");
        puts("UTF-32LE");
        puts("UTF-32BE");
        return 0;
    }
    if (idx >= argc) {
        print_env_summary();
        return 0;
    }
    int rc = 0;
    for (; idx < argc; idx++) {
        if (print_keyword(argv[idx], opt_c, opt_k) != 0) {
            rc = 1;
        }
    }
    return rc;
}
