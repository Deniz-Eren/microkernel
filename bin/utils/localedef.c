/**
 * @file     localedef.c
 * @brief    Ring 3 localedef utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

static int is_valid_category(const char *s) {
    return strcmp(s, "LC_CTYPE") == 0 || strcmp(s, "LC_COLLATE") == 0 ||
           strcmp(s, "LC_MONETARY") == 0 || strcmp(s, "LC_NUMERIC") == 0 ||
           strcmp(s, "LC_TIME") == 0 || strcmp(s, "LC_MESSAGES") == 0;
}

int main(int argc, char **argv) {
    int force_create = 0;
    const char *charmap = "UTF-8";
    const char *input_path = NULL;
    const char *codeset = "UTF-8";
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-c") == 0 || strcmp(argv[idx], "-w") == 0) {
            if (argv[idx][1] == 'c') {
                force_create = 1;
            }
            idx++;
        } else if (strcmp(argv[idx], "-f") == 0 && idx + 1 < argc) {
            charmap = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-i") == 0 && idx + 1 < argc) {
            input_path = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-u") == 0 && idx + 1 < argc) {
            codeset = argv[idx + 1];
            idx += 2;
        } else {
            fprintf(stderr, "localedef: invalid option '%s'\n", argv[idx]);
            return 4;
        }
    }
    if (idx >= argc) {
        fprintf(stderr, "usage: localedef [-c] [-f charmap] [-i inputfile] "
                        "[-u codeset] name\n");
        return 4;
    }
    const char *target = argv[idx];

    FILE *in = stdin;
    if (input_path) {
        in = fopen(input_path, "r");
        if (!in) {
            perror(input_path);
            return 4;
        }
    }

    char comment_ch = '#';
    char cur_cat[32] = {0};
    int seen_cats = 0;
    int warnings = 0;
    char line[512];

    while (fgets(line, (int)sizeof(line), in) != NULL) {
        char *p = line;
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0' || *p == comment_ch) {
            continue;
        }
        size_t len = strlen(p);
        while (len > 0u && (p[len - 1u] == '\n' || p[len - 1u] == '\r' ||
                            p[len - 1u] == ' ' || p[len - 1u] == '\t')) {
            p[--len] = '\0';
        }
        if (strncmp(p, "comment_char ", 13u) == 0 && p[13] != '\0') {
            comment_ch = p[13];
            continue;
        }
        if (strncmp(p, "escape_char ", 12u) == 0) {
            continue;
        }
        if (cur_cat[0] == '\0') {
            if (is_valid_category(p)) {
                (void)strlcpy(cur_cat, p, sizeof(cur_cat));
                seen_cats++;
            } else {
                warnings++;
            }
        } else if (strncmp(p, "END ", 4u) == 0) {
            const char *ec = p + 4;
            while (*ec == ' ' || *ec == '\t') {
                ec++;
            }
            if (strcmp(ec, cur_cat) != 0) {
                fprintf(stderr, "localedef: mismatched END %s (expected %s)\n",
                        ec, cur_cat);
                if (in != stdin) {
                    (void)fclose(in);
                }
                return 4;
            }
            cur_cat[0] = '\0';
        }
    }
    if (in != stdin) {
        (void)fclose(in);
    }
    if (cur_cat[0] != '\0') {
        fprintf(stderr, "localedef: unterminated category %s\n", cur_cat);
        return 4;
    }

    char out_path[PATH_MAX];
    if (strchr(target, '/') != NULL) {
        (void)strlcpy(out_path, target, sizeof(out_path));
    } else {
        (void)snprintf(out_path, sizeof(out_path), "/tmp/localedef_%s", target);
    }

    FILE *out = fopen(out_path, "w");
    if (!out) {
        perror(out_path);
        return 4;
    }
    fprintf(out, "MKLOC01\nname=%s\ncharmap=%s\ncodeset=%s\ncategories=%d\n",
            target, charmap, codeset, seen_cats);
    if (fclose(out) != 0) {
        perror(out_path);
        return 4;
    }
    if (warnings > 0 && !force_create) {
        return 1;
    }
    return warnings > 0 ? 1 : 0;
}
