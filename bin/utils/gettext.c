/**
 * @file     gettext.c
 * @brief    Ring 3 gettext utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

static void print_expanded(const char *s, int expand, int *suppress_nl) {
    if (!expand) {
        fputs(s, stdout);
        return;
    }
    for (const char *p = s; *p != '\0'; p++) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            if (*p == 'n') {
                putchar('\n');
            } else if (*p == 't') {
                putchar('\t');
            } else if (*p == 'r') {
                putchar('\r');
            } else if (*p == 'b') {
                putchar('\b');
            } else if (*p == 'f') {
                putchar('\f');
            } else if (*p == 'v') {
                putchar('\v');
            } else if (*p == 'a') {
                putchar('\a');
            } else if (*p == '\\') {
                putchar('\\');
            } else if (*p == 'c') {
                if (suppress_nl) {
                    *suppress_nl = 1;
                }
                return;
            } else {
                putchar('\\');
                putchar(*p);
            }
        } else {
            putchar(*p);
        }
    }
}

int main(int argc, char **argv) {
    (void)setlocale(LC_ALL, "");
    const char *domain = getenv("TEXTDOMAIN");
    const char *dir = getenv("TEXTDOMAINDIR");
    int expand = 0;
    int echo_mode = 0;
    int no_nl = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-d") == 0 && idx + 1 < argc) {
            domain = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-e") == 0) {
            expand = 1;
            idx++;
        } else if (strcmp(argv[idx], "-E") == 0) {
            expand = 0;
            idx++;
        } else if (strcmp(argv[idx], "-s") == 0) {
            echo_mode = 1;
            idx++;
        } else if (strcmp(argv[idx], "-n") == 0) {
            no_nl = 1;
            idx++;
        } else {
            fprintf(stderr, "gettext: unknown option '%s'\n", argv[idx]);
            return 1;
        }
    }

    if (idx >= argc) {
        fprintf(stderr, "usage: gettext [-d domain] [-e] [-n] [-s] "
                        "[textdomain] msgid\n");
        return 1;
    }

    if (!echo_mode && idx + 2 == argc) {
        domain = argv[idx++];
    }
    if (domain && domain[0] != '\0' && dir && dir[0] != '\0') {
        (void)bindtextdomain(domain, dir);
    }

    if (echo_mode) {
        int suppress = no_nl;
        for (int i = idx; i < argc; i++) {
            if (i > idx) {
                putchar(' ');
            }
            const char *tr = (domain && domain[0] != '\0')
                                 ? dgettext(domain, argv[i])
                                 : argv[i];
            print_expanded(tr, expand, &suppress);
            if (suppress && expand) {
                break;
            }
        }
        if (!suppress) {
            putchar('\n');
        }
        return 0;
    }

    const char *msgid = argv[idx];
    const char *tr =
        (domain && domain[0] != '\0') ? dgettext(domain, msgid) : msgid;
    print_expanded(tr, expand, NULL);
    return 0;
}
