/**
 * @file     ngettext.c
 * @brief    Ring 3 ngettext utility targeting POSIX.1-2024 XCU
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

static void print_expanded(const char *s, int expand) {
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
            } else if (*p == '\\') {
                putchar('\\');
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
        } else {
            fprintf(stderr, "ngettext: unknown option '%s'\n", argv[idx]);
            return 1;
        }
    }

    int rem = argc - idx;
    if (rem != 3 && rem != 4) {
        fprintf(stderr, "usage: ngettext [-d domain] [-e] [textdomain] "
                        "msgid msgid-plural count\n");
        return 1;
    }
    if (rem == 4) {
        domain = argv[idx++];
    }
    const char *msgid1 = argv[idx];
    const char *msgid2 = argv[idx + 1];
    char *end = NULL;
    unsigned long n = strtoul(argv[idx + 2], &end, 10);
    if (!end || *end != '\0') {
        fprintf(stderr, "ngettext: invalid count '%s'\n", argv[idx + 2]);
        return 1;
    }

    if (domain && domain[0] != '\0' && dir && dir[0] != '\0') {
        (void)bindtextdomain(domain, dir);
    }
    const char *tr = (domain && domain[0] != '\0')
                         ? dngettext(domain, msgid1, msgid2, n)
                         : ((n == 1ul) ? msgid1 : msgid2);
    print_expanded(tr, expand);
    return 0;
}
