/**
 * @file     sort.c
 * @brief    Ring 3 sort utility targeting POSIX.1-2024 XCU
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

/* need:: COMP_LIBC_0020 */

#define MAX_KEYS 16

typedef struct {
    int f_start; /* 1-based */
    int c_start; /* 1-based, 0 = default */
    int f_end;   /* 1-based, 0 = end of line */
    int c_end;   /* 1-based, 0 = end of field */
    int b_start;
    int b_end;
    int fold;
    int numeric;
    int dict;
    int ignore_np;
    int reverse;
    int has_opts;
} sort_key_t;

typedef struct {
    char *text;
    size_t len;
    size_t idx;
} sort_line_t;

static int g_reverse;
static int g_numeric;
static int g_fold;
static int g_blanks;
static int g_dict;
static int g_ignore_np;
static int g_unique;
static int g_delim = -1;
static sort_key_t g_keys[MAX_KEYS];
static int g_nkeys;

static int parse_keydef(const char *spec, sort_key_t *k) {
    memset(k, 0, sizeof(*k));
    const char *p = spec;
    char *end = NULL;
    long fs = strtol(p, &end, 10);
    if (end == p || fs <= 0) {
        return -1;
    }
    k->f_start = (int)fs;
    p = end;
    if (*p == '.') {
        p++;
        long cs = strtol(p, &end, 10);
        if (end == p || cs <= 0) {
            return -1;
        }
        k->c_start = (int)cs;
        p = end;
    }
    while (*p != '\0' && *p != ',') {
        k->has_opts = 1;
        if (*p == 'b') {
            k->b_start = 1;
        } else if (*p == 'd') {
            k->dict = 1;
        } else if (*p == 'f') {
            k->fold = 1;
        } else if (*p == 'i') {
            k->ignore_np = 1;
        } else if (*p == 'n') {
            k->numeric = 1;
        } else if (*p == 'r') {
            k->reverse = 1;
        } else {
            return -1;
        }
        p++;
    }
    if (*p == ',') {
        p++;
        long fe = strtol(p, &end, 10);
        if (end == p || fe <= 0) {
            return -1;
        }
        k->f_end = (int)fe;
        p = end;
        if (*p == '.') {
            p++;
            long ce = strtol(p, &end, 10);
            if (end == p || ce < 0) {
                return -1;
            }
            k->c_end = (int)ce;
            p = end;
        }
        while (*p != '\0') {
            k->has_opts = 1;
            if (*p == 'b') {
                k->b_end = 1;
            } else if (*p == 'd') {
                k->dict = 1;
            } else if (*p == 'f') {
                k->fold = 1;
            } else if (*p == 'i') {
                k->ignore_np = 1;
            } else if (*p == 'n') {
                k->numeric = 1;
            } else if (*p == 'r') {
                k->reverse = 1;
            } else {
                return -1;
            }
            p++;
        }
    }
    return 0;
}

static const char *find_field_start(const char *s, size_t len, int field_no) {
    const char *p = s;
    const char *end = s + len;
    int cur = 1;
    if (g_delim >= 0) {
        while (cur < field_no && p < end) {
            if ((unsigned char)*p == (unsigned char)g_delim) {
                cur++;
            }
            p++;
        }
        return (cur == field_no) ? p : end;
    }
    while (cur < field_no && p < end) {
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        while (p < end && *p != ' ' && *p != '\t') {
            p++;
        }
        cur++;
    }
    return (cur == field_no) ? p : end;
}

static const char *find_field_end(const char *s, size_t len, int field_no) {
    const char *p = find_field_start(s, len, field_no);
    const char *end = s + len;
    if (g_delim >= 0) {
        while (p < end && (unsigned char)*p != (unsigned char)g_delim) {
            p++;
        }
        return p;
    }
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    while (p < end && *p != ' ' && *p != '\t') {
        p++;
    }
    return p;
}

static void extract_key(const sort_line_t *line, const sort_key_t *k,
                        const char **sp, size_t *lp) {
    const char *end = line->text + line->len;
    const char *s = find_field_start(line->text, line->len, k->f_start);
    int b_st = k->has_opts ? k->b_start : g_blanks;
    if (b_st || (g_delim < 0 && k->c_start == 0)) {
        while (s < end && (*s == ' ' || *s == '\t')) {
            s++;
        }
    }
    if (k->c_start > 1) {
        size_t adv = (size_t)(k->c_start - 1);
        s = (s + adv < end) ? (s + adv) : end;
    }
    const char *e = end;
    if (k->f_end > 0) {
        if (k->c_end > 0) {
            const char *fs = find_field_start(line->text, line->len, k->f_end);
            int b_en = k->has_opts ? k->b_end : g_blanks;
            if (b_en) {
                while (fs < end && (*fs == ' ' || *fs == '\t')) {
                    fs++;
                }
            }
            size_t adv = (size_t)k->c_end;
            e = (fs + adv < end) ? (fs + adv) : end;
        } else {
            e = find_field_end(line->text, line->len, k->f_end);
        }
    }
    if (e < s) {
        e = s;
    }
    *sp = s;
    *lp = (size_t)(e - s);
}

static int cmp_span(const char *a, size_t alen, const char *b, size_t blen,
                    int num, int fold, int dict, int ign_np, int blanks) {
    if (blanks) {
        while (alen > 0u && (*a == ' ' || *a == '\t')) {
            a++;
            alen--;
        }
        while (blen > 0u && (*b == ' ' || *b == '\t')) {
            b++;
            blen--;
        }
    }
    if (num) {
        char abuf[64];
        char bbuf[64];
        size_t ac = alen < sizeof(abuf) - 1u ? alen : sizeof(abuf) - 1u;
        size_t bc = blen < sizeof(bbuf) - 1u ? blen : sizeof(bbuf) - 1u;
        memcpy(abuf, a, ac);
        abuf[ac] = '\0';
        memcpy(bbuf, b, bc);
        bbuf[bc] = '\0';
        double da = strtod(abuf, NULL);
        double db = strtod(bbuf, NULL);
        if (da < db) {
            return -1;
        }
        if (da > db) {
            return 1;
        }
        return 0;
    }
    size_t i = 0u;
    size_t j = 0u;
    while (i < alen || j < blen) {
        while (i < alen) {
            unsigned char ca = (unsigned char)a[i];
            if (dict && !isalnum(ca) && ca != ' ' && ca != '\t') {
                i++;
                continue;
            }
            if (ign_np && !isprint(ca)) {
                i++;
                continue;
            }
            break;
        }
        while (j < blen) {
            unsigned char cb = (unsigned char)b[j];
            if (dict && !isalnum(cb) && cb != ' ' && cb != '\t') {
                j++;
                continue;
            }
            if (ign_np && !isprint(cb)) {
                j++;
                continue;
            }
            break;
        }
        if (i >= alen && j >= blen) {
            return 0;
        }
        if (i >= alen) {
            return -1;
        }
        if (j >= blen) {
            return 1;
        }
        unsigned char ca = (unsigned char)a[i++];
        unsigned char cb = (unsigned char)b[j++];
        if (fold) {
            ca = (unsigned char)toupper(ca);
            cb = (unsigned char)toupper(cb);
        }
        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
    }
    return 0;
}

static int cmp_keys_only(const sort_line_t *a, const sort_line_t *b) {
    if (g_nkeys > 0) {
        for (int i = 0; i < g_nkeys; i++) {
            const sort_key_t *k = &g_keys[i];
            const char *sa = NULL;
            const char *sb = NULL;
            size_t la = 0u;
            size_t lb = 0u;
            extract_key(a, k, &sa, &la);
            extract_key(b, k, &sb, &lb);
            int num = k->has_opts ? k->numeric : g_numeric;
            int fold = k->has_opts ? k->fold : g_fold;
            int dict = k->has_opts ? k->dict : g_dict;
            int ign = k->has_opts ? k->ignore_np : g_ignore_np;
            int rev = k->has_opts ? k->reverse : g_reverse;
            int r = cmp_span(sa, la, sb, lb, num, fold, dict, ign, 0);
            if (r != 0) {
                return rev ? -r : r;
            }
        }
        return 0;
    }
    int r = cmp_span(a->text, a->len, b->text, b->len, g_numeric, g_fold,
                     g_dict, g_ignore_np, g_blanks);
    return g_reverse ? -r : r;
}

static int cmp_lines(const void *pa, const void *pb) {
    const sort_line_t *a = (const sort_line_t *)pa;
    const sort_line_t *b = (const sort_line_t *)pb;
    int r = cmp_keys_only(a, b);
    if (r != 0) {
        return r;
    }
    if (!g_unique) {
        size_t mn = a->len < b->len ? a->len : b->len;
        int mr = memcmp(a->text, b->text, mn);
        if (mr != 0) {
            return g_reverse ? -mr : mr;
        }
        if (a->len != b->len) {
            int lr = (a->len < b->len) ? -1 : 1;
            return g_reverse ? -lr : lr;
        }
    }
    return (a->idx < b->idx) ? -1 : (a->idx > b->idx ? 1 : 0);
}

static int read_stream(FILE *fp, sort_line_t **arr, size_t *count,
                       size_t *cap) {
    char *line = NULL;
    size_t lcap = 0u;
    ssize_t n;
    while ((n = getline(&line, &lcap, fp)) >= 0) {
        size_t len = (size_t)n;
        if (len > 0u && line[len - 1u] == '\n') {
            len--;
        }
        if (*count >= *cap) {
            size_t ncap = (*cap == 0u) ? 64u : (*cap * 2u);
            sort_line_t *na =
                (sort_line_t *)realloc(*arr, ncap * sizeof(sort_line_t));
            if (!na) {
                free(line);
                return -1;
            }
            *arr = na;
            *cap = ncap;
        }
        char *copy = (char *)malloc(len + 1u);
        if (!copy) {
            free(line);
            return -1;
        }
        if (len > 0u) {
            memcpy(copy, line, len);
        }
        copy[len] = '\0';
        (*arr)[*count].text = copy;
        (*arr)[*count].len = len;
        (*arr)[*count].idx = *count;
        (*count)++;
    }
    free(line);
    return 0;
}

int main(int argc, char **argv) {
    int check_only = 0;
    int check_quiet = 0;
    const char *out_path = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "bcCdfik:mno:rt:u")) != -1) {
        switch (opt) {
        case 'b':
            g_blanks = 1;
            break;
        case 'c':
            check_only = 1;
            break;
        case 'C':
            check_only = 1;
            check_quiet = 1;
            break;
        case 'd':
            g_dict = 1;
            break;
        case 'f':
            g_fold = 1;
            break;
        case 'i':
            g_ignore_np = 1;
            break;
        case 'k':
            if (g_nkeys >= MAX_KEYS ||
                parse_keydef(optarg, &g_keys[g_nkeys]) != 0) {
                (void)fprintf(stderr, "sort: invalid key '%s'\n", optarg);
                return 2;
            }
            g_nkeys++;
            break;
        case 'm':
            break;
        case 'n':
            g_numeric = 1;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'r':
            g_reverse = 1;
            break;
        case 't':
            if (optarg[0] == '\0' || optarg[1] != '\0') {
                (void)fprintf(stderr, "sort: invalid delimiter\n");
                return 2;
            }
            g_delim = (unsigned char)optarg[0];
            break;
        case 'u':
            g_unique = 1;
            break;
        default:
            return 2;
        }
    }

    sort_line_t *lines = NULL;
    size_t count = 0u;
    size_t cap = 0u;
    int rc = 0;
    if (optind >= argc) {
        if (read_stream(stdin, &lines, &count, &cap) != 0) {
            rc = 2;
        }
    } else {
        for (int i = optind; i < argc; i++) {
            FILE *fp =
                (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                rc = 2;
                break;
            }
            int rr = read_stream(fp, &lines, &count, &cap);
            if (fp != stdin) {
                (void)fclose(fp);
            }
            if (rr != 0) {
                rc = 2;
                break;
            }
        }
    }

    if (rc == 0 && check_only) {
        for (size_t i = 1u; i < count; i++) {
            int cmp = cmp_keys_only(&lines[i - 1u], &lines[i]);
            if (cmp > 0 || (g_unique && cmp == 0)) {
                if (!check_quiet) {
                    (void)fprintf(stderr, "sort: disorder on line %zu\n",
                                  i + 1u);
                }
                rc = 1;
                break;
            }
        }
    } else if (rc == 0) {
        if (count > 1u) {
            qsort(lines, count, sizeof(sort_line_t), cmp_lines);
        }
        FILE *out = stdout;
        if (out_path) {
            out = fopen(out_path, "w");
            if (!out) {
                perror(out_path);
                rc = 2;
            }
        }
        if (rc == 0) {
            for (size_t i = 0u; i < count; i++) {
                if (g_unique && i > 0u &&
                    cmp_keys_only(&lines[i - 1u], &lines[i]) == 0) {
                    continue;
                }
                if (lines[i].len > 0u) {
                    (void)fwrite(lines[i].text, 1u, lines[i].len, out);
                }
                (void)fputc('\n', out);
            }
            if (out != stdout) {
                (void)fclose(out);
            }
        }
    }

    for (size_t i = 0u; i < count; i++) {
        free(lines[i].text);
    }
    free(lines);
    return rc;
}
