/**
 * @file     grep.c
 * @brief    Ring 3 grep utility targeting POSIX.1-2024 XCU over <regex.h>
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* need:: COMP_LIBC_0018 */

struct pat_item {
    char *raw;
    regex_t re;
    int compiled;
};

struct grep_opts {
    int ere;
    int fixed;
    int count_only;
    int list_files;
    int quiet;
    int line_num;
    int icase;
    int invert;
    int whole_line;
    int no_msg;
    struct pat_item *pats;
    size_t npats;
    size_t pat_cap;
};

static int add_pattern(struct grep_opts *o, const char *s, size_t len) {
    if (o->npats == o->pat_cap) {
        size_t ncap = o->pat_cap ? o->pat_cap * 2u : 16u;
        struct pat_item *np =
            (struct pat_item *)reallocarray(o->pats, ncap, sizeof(*np));
        if (!np) {
            return -1;
        }
        o->pats = np;
        o->pat_cap = ncap;
    }
    char *copy = strndup(s, len);
    if (!copy) {
        return -1;
    }
    o->pats[o->npats].raw = copy;
    o->pats[o->npats].compiled = 0;
    o->npats++;
    return 0;
}

static int add_pattern_lines(struct grep_opts *o, const char *s) {
    const char *p = s;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (add_pattern(o, p, len) != 0) {
            return -1;
        }
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
    return 0;
}

static int add_pattern_file(struct grep_opts *o, const char *path) {
    FILE *fp = (strcmp(path, "-") == 0) ? stdin : fopen(path, "r");
    if (!fp) {
        if (!o->no_msg) {
            perror(path);
        }
        return -1;
    }
    char *line = NULL;
    size_t cap = 0u;
    ssize_t n;
    while ((n = getline(&line, &cap, fp)) >= 0) {
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        if (add_pattern(o, line, (size_t)n) != 0) {
            free(line);
            if (fp != stdin) {
                (void)fclose(fp);
            }
            return -1;
        }
    }
    free(line);
    if (fp != stdin) {
        (void)fclose(fp);
    }
    return 0;
}

static int compile_patterns(struct grep_opts *o) {
    if (o->fixed) {
        return 0;
    }
    int cflags = (o->ere ? REG_EXTENDED : 0) | (o->icase ? REG_ICASE : 0) |
                 (o->whole_line ? 0 : REG_NOSUB);
    for (size_t i = 0u; i < o->npats; i++) {
        int rc = regcomp(&o->pats[i].re, o->pats[i].raw, cflags);
        if (rc != 0) {
            char errbuf[128];
            (void)regerror(rc, &o->pats[i].re, errbuf, sizeof(errbuf));
            fprintf(stderr, "grep: %s: %s\n", o->pats[i].raw, errbuf);
            return -1;
        }
        o->pats[i].compiled = 1;
    }
    return 0;
}

static int match_fixed_one(const char *line, size_t llen, const char *pat,
                           int icase, int whole) {
    size_t plen = strlen(pat);
    if (whole) {
        if (llen != plen) {
            return 0;
        }
        return icase ? (strncasecmp(line, pat, plen) == 0)
                     : (memcmp(line, pat, plen) == 0);
    }
    if (plen == 0u) {
        return 1;
    }
    if (plen > llen) {
        return 0;
    }
    for (size_t i = 0u; i + plen <= llen; i++) {
        if (icase) {
            if (strncasecmp(line + i, pat, plen) == 0) {
                return 1;
            }
        } else if (memcmp(line + i, pat, plen) == 0) {
            return 1;
        }
    }
    return 0;
}

static int line_matches(const struct grep_opts *o, const char *line,
                        size_t llen) {
    int matched = 0;
    for (size_t i = 0u; i < o->npats; i++) {
        if (o->fixed) {
            if (match_fixed_one(line, llen, o->pats[i].raw, o->icase,
                                o->whole_line)) {
                matched = 1;
                break;
            }
        } else {
            regmatch_t pm[1];
            if (regexec(&o->pats[i].re, line, o->whole_line ? 1u : 0u,
                        o->whole_line ? pm : NULL, 0) == 0) {
                if (!o->whole_line ||
                    (pm[0].rm_so == 0 && (size_t)pm[0].rm_eo == llen)) {
                    matched = 1;
                    break;
                }
            }
        }
    }
    return o->invert ? !matched : matched;
}

static int grep_stream(const struct grep_opts *o, FILE *fp, const char *fname,
                       int show_fname, int *any_match) {
    char *line = NULL;
    size_t cap = 0u;
    ssize_t n;
    unsigned long lno = 0ul;
    unsigned long count = 0ul;

    while ((n = getline(&line, &cap, fp)) >= 0) {
        lno++;
        size_t llen = (size_t)n;
        if (llen > 0u && line[llen - 1u] == '\n') {
            line[--llen] = '\0';
        }
        if (line_matches(o, line, llen)) {
            *any_match = 1;
            count++;
            if (o->quiet) {
                free(line);
                return 0;
            }
            if (o->list_files) {
                printf("%s\n", fname ? fname : "(standard input)");
                free(line);
                return 0;
            }
            if (!o->count_only) {
                if (show_fname && fname) {
                    printf("%s:", fname);
                }
                if (o->line_num) {
                    printf("%lu:", lno);
                }
                puts(line);
            }
        }
    }
    free(line);
    if (o->count_only && !o->quiet && !o->list_files) {
        if (show_fname && fname) {
            printf("%s:", fname);
        }
        printf("%lu\n", count);
    }
    return 0;
}

int main(int argc, char **argv) {
    struct grep_opts o;
    memset(&o, 0, sizeof(o));
    int had_pat_opt = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        const char *p = argv[idx] + 1;
        while (*p != '\0') {
            char c = *p++;
            if (c == 'E') {
                o.ere = 1;
                o.fixed = 0;
            } else if (c == 'F') {
                o.fixed = 1;
                o.ere = 0;
            } else if (c == 'c') {
                o.count_only = 1;
            } else if (c == 'l') {
                o.list_files = 1;
            } else if (c == 'q') {
                o.quiet = 1;
            } else if (c == 'n') {
                o.line_num = 1;
            } else if (c == 'i') {
                o.icase = 1;
            } else if (c == 'v') {
                o.invert = 1;
            } else if (c == 'x') {
                o.whole_line = 1;
            } else if (c == 's') {
                o.no_msg = 1;
            } else if (c == 'e' || c == 'f') {
                const char *arg = (*p != '\0') ? p : NULL;
                if (!arg) {
                    if (idx + 1 >= argc) {
                        fprintf(stderr, "grep: -%c requires an argument\n", c);
                        return 2;
                    }
                    arg = argv[++idx];
                }
                had_pat_opt = 1;
                if (c == 'e') {
                    if (add_pattern_lines(&o, arg) != 0) {
                        return 2;
                    }
                } else {
                    if (add_pattern_file(&o, arg) != 0) {
                        return 2;
                    }
                }
                break;
            } else {
                fprintf(stderr, "grep: unknown option -%c\n", c);
                return 2;
            }
        }
        idx++;
    }

    if (!had_pat_opt) {
        if (idx >= argc) {
            fprintf(stderr, "usage: grep [-E|-F] [-cilnqsvx] [-e pat] "
                            "[-f file] [pattern] [file...]\n");
            return 2;
        }
        if (add_pattern_lines(&o, argv[idx++]) != 0) {
            return 2;
        }
    }

    if (compile_patterns(&o) != 0) {
        return 2;
    }

    int any_match = 0;
    int had_err = 0;
    int nfiles = argc - idx;

    if (nfiles <= 0) {
        (void)grep_stream(&o, stdin, NULL, 0, &any_match);
    } else {
        for (int i = idx; i < argc; i++) {
            FILE *fp = stdin;
            const char *fname = argv[i];
            if (strcmp(fname, "-") != 0) {
                fp = fopen(fname, "r");
                if (!fp) {
                    if (!o.no_msg) {
                        perror(fname);
                    }
                    had_err = 1;
                    continue;
                }
            }
            (void)grep_stream(&o, fp, fname, nfiles > 1, &any_match);
            if (fp != stdin) {
                (void)fclose(fp);
            }
            if (o.quiet && any_match) {
                break;
            }
        }
    }

    for (size_t i = 0u; i < o.npats; i++) {
        if (o.pats[i].compiled) {
            regfree(&o.pats[i].re);
        }
        free(o.pats[i].raw);
    }
    free(o.pats);

    if (o.quiet && any_match) {
        return 0;
    }
    if (had_err) {
        return 2;
    }
    return any_match ? 0 : 1;
}
