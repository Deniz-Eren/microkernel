/**
 * @file     diff.c
 * @brief    Ring 3 diff utility targeting POSIX.1-2024 XCU (-u, -c, -e, -r)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0021 */

enum {
    FMT_NORMAL = 0,
    FMT_UNIFIED = 1,
    FMT_CONTEXT = 2,
    FMT_ED = 3,
};

enum {
    OP_EQ = 0,
    OP_DEL = 1,
    OP_INS = 2,
};

typedef struct {
    char **lines;
    size_t count;
    size_t cap;
} line_buf_t;

typedef struct {
    uint8_t op;
    size_t a_idx;
    size_t b_idx;
} edit_t;

static int g_fmt = FMT_NORMAL;
static size_t g_ctx = 3u;
static int g_ign_space_change;
static int g_ign_all_space;
static int g_ign_case;
static int g_brief;
static int g_recursive;
static int g_new_file;

static void free_lines(line_buf_t *lb) {
    for (size_t i = 0u; i < lb->count; i++) {
        free(lb->lines[i]);
    }
    free(lb->lines);
    memset(lb, 0, sizeof(*lb));
}

static int read_file_lines(const char *path, line_buf_t *lb) {
    memset(lb, 0, sizeof(*lb));
    if (!path) {
        return 0;
    }
    FILE *fp = (strcmp(path, "-") == 0) ? stdin : fopen(path, "r");
    if (!fp) {
        return -1;
    }
    char *line = NULL;
    size_t lcap = 0u;
    ssize_t n;
    int rc = 0;
    while ((n = getline(&line, &lcap, fp)) >= 0) {
        size_t len = (size_t)n;
        if (len > 0u && line[len - 1u] == '\n') {
            len--;
        }
        if (lb->count >= lb->cap) {
            size_t ncap = (lb->cap == 0u) ? 64u : (lb->cap * 2u);
            char **nl = (char **)realloc(lb->lines, ncap * sizeof(char *));
            if (!nl) {
                rc = -1;
                break;
            }
            lb->lines = nl;
            lb->cap = ncap;
        }
        char *copy = (char *)malloc(len + 1u);
        if (!copy) {
            rc = -1;
            break;
        }
        if (len > 0u) {
            memcpy(copy, line, len);
        }
        copy[len] = '\0';
        lb->lines[lb->count++] = copy;
    }
    free(line);
    if (fp != stdin) {
        (void)fclose(fp);
    }
    if (rc != 0) {
        free_lines(lb);
    }
    return rc;
}

static int lines_match(const char *a, const char *b) {
    if (!g_ign_space_change && !g_ign_all_space && !g_ign_case) {
        return strcmp(a, b) == 0;
    }
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (*pa != '\0' || *pb != '\0') {
        if (g_ign_all_space) {
            while (*pa == ' ' || *pa == '\t') {
                pa++;
            }
            while (*pb == ' ' || *pb == '\t') {
                pb++;
            }
        } else if (g_ign_space_change) {
            int sa = (*pa == ' ' || *pa == '\t');
            int sb = (*pb == ' ' || *pb == '\t');
            if (sa && sb) {
                while (*pa == ' ' || *pa == '\t') {
                    pa++;
                }
                while (*pb == ' ' || *pb == '\t') {
                    pb++;
                }
                continue;
            }
            if ((sa && *pb == '\0') || (sb && *pa == '\0')) {
                while (*pa == ' ' || *pa == '\t') {
                    pa++;
                }
                while (*pb == ' ' || *pb == '\t') {
                    pb++;
                }
                continue;
            }
        }
        if (*pa == '\0' || *pb == '\0') {
            return (*pa == *pb);
        }
        unsigned char ca = g_ign_case ? (unsigned char)tolower(*pa) : *pa;
        unsigned char cb = g_ign_case ? (unsigned char)tolower(*pb) : *pb;
        if (ca != cb) {
            return 0;
        }
        pa++;
        pb++;
    }
    return 1;
}

static int build_edits(const line_buf_t *a, const line_buf_t *b,
                       edit_t **out_edits, size_t *out_n) {
    size_t pre = 0u;
    while (pre < a->count && pre < b->count &&
           lines_match(a->lines[pre], b->lines[pre])) {
        pre++;
    }
    size_t a_rem = a->count - pre;
    size_t b_rem = b->count - pre;
    size_t suf = 0u;
    while (suf < a_rem && suf < b_rem &&
           lines_match(a->lines[a->count - 1u - suf],
                       b->lines[b->count - 1u - suf])) {
        suf++;
    }

    size_t m = a_rem - suf;
    size_t n = b_rem - suf;
    size_t max_ed = a->count + b->count + 1u;
    edit_t *eds = (edit_t *)calloc(max_ed, sizeof(edit_t));
    if (!eds) {
        return -1;
    }
    size_t ne = 0u;

    for (size_t i = 0u; i < pre; i++) {
        eds[ne++] = (edit_t){OP_EQ, i, i};
    }

    if (m == 0u) {
        for (size_t j = 0u; j < n; j++) {
            eds[ne++] = (edit_t){OP_INS, pre, pre + j};
        }
    } else if (n == 0u) {
        for (size_t i = 0u; i < m; i++) {
            eds[ne++] = (edit_t){OP_DEL, pre + i, pre};
        }
    } else {
        size_t cols = n + 1u;
        uint32_t *dp = (uint32_t *)calloc((m + 1u) * cols, sizeof(uint32_t));
        if (!dp) {
            free(eds);
            return -1;
        }
        for (size_t i = m; i > 0u; i--) {
            for (size_t j = n; j > 0u; j--) {
                if (lines_match(a->lines[pre + i - 1u],
                                b->lines[pre + j - 1u])) {
                    dp[(i - 1u) * cols + (j - 1u)] = dp[i * cols + j] + 1u;
                } else {
                    uint32_t down = dp[i * cols + (j - 1u)];
                    uint32_t right = dp[(i - 1u) * cols + j];
                    dp[(i - 1u) * cols + (j - 1u)] =
                        (down >= right) ? down : right;
                }
            }
        }
        size_t i = 0u;
        size_t j = 0u;
        while (i < m || j < n) {
            if (i < m && j < n &&
                lines_match(a->lines[pre + i], b->lines[pre + j])) {
                eds[ne++] = (edit_t){OP_EQ, pre + i, pre + j};
                i++;
                j++;
            } else if (i < m && (j == n || dp[(i + 1u) * cols + j] >=
                                               dp[i * cols + (j + 1u)])) {
                eds[ne++] = (edit_t){OP_DEL, pre + i, pre + j};
                i++;
            } else {
                eds[ne++] = (edit_t){OP_INS, pre + i, pre + j};
                j++;
            }
        }
        free(dp);
    }

    for (size_t k = 0u; k < suf; k++) {
        eds[ne++] = (edit_t){OP_EQ, a->count - suf + k, b->count - suf + k};
    }

    *out_edits = eds;
    *out_n = ne;
    return 0;
}

static void print_range(size_t start1, size_t count) {
    if (count <= 1u) {
        (void)printf("%zu", start1);
    } else {
        (void)printf("%zu,%zu", start1, start1 + count - 1u);
    }
}

static void emit_normal(const line_buf_t *a, const line_buf_t *b,
                        const edit_t *eds, size_t ne) {
    size_t i = 0u;
    while (i < ne) {
        if (eds[i].op == OP_EQ) {
            i++;
            continue;
        }
        size_t start = i;
        size_t a_cnt = 0u;
        size_t b_cnt = 0u;
        size_t a_pos = eds[start].a_idx;
        size_t b_pos = eds[start].b_idx;
        while (i < ne && eds[i].op != OP_EQ) {
            if (eds[i].op == OP_DEL) {
                a_cnt++;
            } else {
                b_cnt++;
            }
            i++;
        }
        if (a_cnt == 0u) {
            (void)printf("%zua", a_pos);
            print_range(b_pos + 1u, b_cnt);
            (void)putchar('\n');
        } else if (b_cnt == 0u) {
            print_range(a_pos + 1u, a_cnt);
            (void)printf("d%zu\n", b_pos);
        } else {
            print_range(a_pos + 1u, a_cnt);
            (void)putchar('c');
            print_range(b_pos + 1u, b_cnt);
            (void)putchar('\n');
        }
        for (size_t k = start; k < i; k++) {
            if (eds[k].op == OP_DEL) {
                (void)printf("< %s\n", a->lines[eds[k].a_idx]);
            }
        }
        if (a_cnt > 0u && b_cnt > 0u) {
            (void)puts("---");
        }
        for (size_t k = start; k < i; k++) {
            if (eds[k].op == OP_INS) {
                (void)printf("> %s\n", b->lines[eds[k].b_idx]);
            }
        }
    }
}

static void emit_ed(const line_buf_t *b, const edit_t *eds, size_t ne) {
    /* Emit blocks in reverse order so earlier line numbers remain valid. */
    size_t i = ne;
    while (i > 0u) {
        if (eds[i - 1u].op == OP_EQ) {
            i--;
            continue;
        }
        size_t end = i;
        while (i > 0u && eds[i - 1u].op != OP_EQ) {
            i--;
        }
        size_t start = i;
        size_t a_cnt = 0u;
        size_t b_cnt = 0u;
        size_t a_pos = eds[start].a_idx;
        for (size_t k = start; k < end; k++) {
            if (eds[k].op == OP_DEL) {
                a_cnt++;
            } else {
                b_cnt++;
            }
        }
        if (a_cnt == 0u) {
            (void)printf("%zua\n", a_pos);
        } else if (b_cnt == 0u) {
            print_range(a_pos + 1u, a_cnt);
            (void)puts("d");
            continue;
        } else {
            print_range(a_pos + 1u, a_cnt);
            (void)puts("c");
        }
        for (size_t k = start; k < end; k++) {
            if (eds[k].op == OP_INS) {
                (void)printf("%s\n", b->lines[eds[k].b_idx]);
            }
        }
        (void)puts(".");
    }
}

static void emit_unified(const char *la, const char *lb, const line_buf_t *a,
                         const line_buf_t *b, const edit_t *eds, size_t ne) {
    (void)printf("--- %s\n+++ %s\n", la, lb);
    size_t i = 0u;
    while (i < ne) {
        while (i < ne && eds[i].op == OP_EQ) {
            i++;
        }
        if (i >= ne) {
            break;
        }
        size_t h_start = (i > g_ctx) ? (i - g_ctx) : 0u;
        size_t h_end = i;
        for (;;) {
            while (h_end < ne && eds[h_end].op != OP_EQ) {
                h_end++;
            }
            size_t eq_run = 0u;
            while (h_end + eq_run < ne && eds[h_end + eq_run].op == OP_EQ) {
                eq_run++;
            }
            if (h_end + eq_run < ne && eq_run <= g_ctx * 2u) {
                h_end += eq_run;
                continue;
            }
            size_t tail = (eq_run < g_ctx) ? eq_run : g_ctx;
            h_end += tail;
            break;
        }

        size_t a_cnt = 0u;
        size_t b_cnt = 0u;
        for (size_t k = h_start; k < h_end; k++) {
            if (eds[k].op != OP_INS) {
                a_cnt++;
            }
            if (eds[k].op != OP_DEL) {
                b_cnt++;
            }
        }
        size_t a_start =
            (a_cnt == 0u) ? eds[h_start].a_idx : (eds[h_start].a_idx + 1u);
        size_t b_start =
            (b_cnt == 0u) ? eds[h_start].b_idx : (eds[h_start].b_idx + 1u);
        (void)printf("@@ -%zu,%zu +%zu,%zu @@\n", a_start, a_cnt, b_start,
                     b_cnt);
        for (size_t k = h_start; k < h_end; k++) {
            if (eds[k].op == OP_EQ) {
                (void)printf(" %s\n", a->lines[eds[k].a_idx]);
            } else if (eds[k].op == OP_DEL) {
                (void)printf("-%s\n", a->lines[eds[k].a_idx]);
            } else {
                (void)printf("+%s\n", b->lines[eds[k].b_idx]);
            }
        }
        i = h_end;
    }
}

static void emit_context(const char *la, const char *lb, const line_buf_t *a,
                         const line_buf_t *b, const edit_t *eds, size_t ne) {
    (void)printf("*** %s\n--- %s\n", la, lb);
    size_t i = 0u;
    while (i < ne) {
        while (i < ne && eds[i].op == OP_EQ) {
            i++;
        }
        if (i >= ne) {
            break;
        }
        size_t h_start = (i > g_ctx) ? (i - g_ctx) : 0u;
        size_t h_end = i;
        for (;;) {
            while (h_end < ne && eds[h_end].op != OP_EQ) {
                h_end++;
            }
            size_t eq_run = 0u;
            while (h_end + eq_run < ne && eds[h_end + eq_run].op == OP_EQ) {
                eq_run++;
            }
            if (h_end + eq_run < ne && eq_run <= g_ctx * 2u) {
                h_end += eq_run;
                continue;
            }
            size_t tail = (eq_run < g_ctx) ? eq_run : g_ctx;
            h_end += tail;
            break;
        }

        size_t a_cnt = 0u;
        size_t b_cnt = 0u;
        int has_del = 0;
        int has_ins = 0;
        for (size_t k = h_start; k < h_end; k++) {
            if (eds[k].op != OP_INS) {
                a_cnt++;
            }
            if (eds[k].op != OP_DEL) {
                b_cnt++;
            }
            if (eds[k].op == OP_DEL) {
                has_del = 1;
            } else if (eds[k].op == OP_INS) {
                has_ins = 1;
            }
        }
        size_t a1 = (a_cnt == 0u) ? 0u : (eds[h_start].a_idx + 1u);
        size_t b1 = (b_cnt == 0u) ? 0u : (eds[h_start].b_idx + 1u);
        (void)puts("***************");
        (void)printf("*** ");
        print_range(a1, a_cnt);
        (void)puts(" ****");
        if (has_del) {
            size_t k = h_start;
            while (k < h_end) {
                if (eds[k].op == OP_EQ) {
                    (void)printf("  %s\n", a->lines[eds[k].a_idx]);
                    k++;
                } else {
                    size_t bs = k;
                    int d = 0;
                    int ins = 0;
                    while (k < h_end && eds[k].op != OP_EQ) {
                        if (eds[k].op == OP_DEL) {
                            d = 1;
                        } else {
                            ins = 1;
                        }
                        k++;
                    }
                    char mark = (d && ins) ? '!' : '-';
                    for (size_t p = bs; p < k; p++) {
                        if (eds[p].op == OP_DEL) {
                            (void)printf("%c %s\n", mark,
                                         a->lines[eds[p].a_idx]);
                        }
                    }
                }
            }
        }
        (void)printf("--- ");
        print_range(b1, b_cnt);
        (void)puts(" ----");
        if (has_ins) {
            size_t k = h_start;
            while (k < h_end) {
                if (eds[k].op == OP_EQ) {
                    (void)printf("  %s\n", b->lines[eds[k].b_idx]);
                    k++;
                } else {
                    size_t bs = k;
                    int d = 0;
                    int ins = 0;
                    while (k < h_end && eds[k].op != OP_EQ) {
                        if (eds[k].op == OP_DEL) {
                            d = 1;
                        } else {
                            ins = 1;
                        }
                        k++;
                    }
                    char mark = (d && ins) ? '!' : '+';
                    for (size_t p = bs; p < k; p++) {
                        if (eds[p].op == OP_INS) {
                            (void)printf("%c %s\n", mark,
                                         b->lines[eds[p].b_idx]);
                        }
                    }
                }
            }
        }
        i = h_end;
    }
}

static int diff_files(const char *p1, const char *p2, const char *l1,
                      const char *l2) {
    line_buf_t a = {0};
    line_buf_t b = {0};
    if (read_file_lines(p1, &a) != 0) {
        perror(p1);
        return 2;
    }
    if (read_file_lines(p2, &b) != 0) {
        perror(p2);
        free_lines(&a);
        return 2;
    }

    edit_t *eds = NULL;
    size_t ne = 0u;
    if (build_edits(&a, &b, &eds, &ne) != 0) {
        free_lines(&a);
        free_lines(&b);
        return 2;
    }

    int differs = 0;
    for (size_t i = 0u; i < ne; i++) {
        if (eds[i].op != OP_EQ) {
            differs = 1;
            break;
        }
    }

    if (differs) {
        if (g_brief) {
            (void)printf("Files %s and %s differ\n", l1, l2);
        } else if (g_fmt == FMT_UNIFIED) {
            emit_unified(l1, l2, &a, &b, eds, ne);
        } else if (g_fmt == FMT_CONTEXT) {
            emit_context(l1, l2, &a, &b, eds, ne);
        } else if (g_fmt == FMT_ED) {
            emit_ed(&b, eds, ne);
        } else {
            emit_normal(&a, &b, eds, ne);
        }
    }

    free(eds);
    free_lines(&a);
    free_lines(&b);
    return differs ? 1 : 0;
}

static int cmp_str_ptr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

static int read_dir_names(const char *dir, char ***out, size_t *count) {
    *out = NULL;
    *count = 0u;
    DIR *dp = opendir(dir);
    if (!dp) {
        return -1;
    }
    size_t cap = 0u;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        if (*count >= cap) {
            size_t ncap = (cap == 0u) ? 16u : (cap * 2u);
            char **na = (char **)realloc(*out, ncap * sizeof(char *));
            if (!na) {
                (void)closedir(dp);
                return -1;
            }
            *out = na;
            cap = ncap;
        }
        (*out)[*count] = strdup(de->d_name);
        if (!(*out)[*count]) {
            (void)closedir(dp);
            return -1;
        }
        (*count)++;
    }
    (void)closedir(dp);
    if (*count > 1u) {
        qsort(*out, *count, sizeof(char *), cmp_str_ptr);
    }
    return 0;
}

static int diff_dirs(const char *d1, const char *d2) {
    char **n1 = NULL;
    char **n2 = NULL;
    size_t c1 = 0u;
    size_t c2 = 0u;
    if (read_dir_names(d1, &n1, &c1) != 0 ||
        read_dir_names(d2, &n2, &c2) != 0) {
        for (size_t i = 0u; i < c1; i++) {
            free(n1[i]);
        }
        for (size_t i = 0u; i < c2; i++) {
            free(n2[i]);
        }
        free(n1);
        free(n2);
        return 2;
    }

    int rc = 0;
    size_t i = 0u;
    size_t j = 0u;
    while (i < c1 || j < c2) {
        int cmp = 0;
        if (j >= c2) {
            cmp = -1;
        } else if (i >= c1) {
            cmp = 1;
        } else {
            cmp = strcmp(n1[i], n2[j]);
        }
        char p1[512];
        char p2[512];
        if (cmp < 0) {
            if (g_new_file) {
                (void)snprintf(p1, sizeof(p1), "%s/%s", d1, n1[i]);
                (void)snprintf(p2, sizeof(p2), "%s/%s", d2, n1[i]);
                int r = diff_files(p1, NULL, p1, p2);
                if (r > rc) {
                    rc = r;
                }
            } else {
                (void)printf("Only in %s: %s\n", d1, n1[i]);
                if (rc == 0) {
                    rc = 1;
                }
            }
            i++;
        } else if (cmp > 0) {
            if (g_new_file) {
                (void)snprintf(p1, sizeof(p1), "%s/%s", d1, n2[j]);
                (void)snprintf(p2, sizeof(p2), "%s/%s", d2, n2[j]);
                int r = diff_files(NULL, p2, p1, p2);
                if (r > rc) {
                    rc = r;
                }
            } else {
                (void)printf("Only in %s: %s\n", d2, n2[j]);
                if (rc == 0) {
                    rc = 1;
                }
            }
            j++;
        } else {
            (void)snprintf(p1, sizeof(p1), "%s/%s", d1, n1[i]);
            (void)snprintf(p2, sizeof(p2), "%s/%s", d2, n2[j]);
            struct stat st1 = {0};
            struct stat st2 = {0};
            if (stat(p1, &st1) == 0 && stat(p2, &st2) == 0) {
                int r = 0;
                if (S_ISDIR(st1.st_mode) && S_ISDIR(st2.st_mode)) {
                    if (g_recursive) {
                        r = diff_dirs(p1, p2);
                    }
                } else {
                    r = diff_files(p1, p2, p1, p2);
                }
                if (r > rc) {
                    rc = r;
                }
            }
            i++;
            j++;
        }
    }

    for (size_t k = 0u; k < c1; k++) {
        free(n1[k]);
    }
    for (size_t k = 0u; k < c2; k++) {
        free(n2[k]);
    }
    free(n1);
    free(n2);
    return rc;
}

int main(int argc, char **argv) {
    const char *labels[2] = {NULL, NULL};
    int nlabels = 0;
    int opt;
    while ((opt = getopt(argc, argv, "bcC:eiL:NqruU:w")) != -1) {
        switch (opt) {
        case 'b':
            g_ign_space_change = 1;
            break;
        case 'c':
            g_fmt = FMT_CONTEXT;
            g_ctx = 3u;
            break;
        case 'C':
            g_fmt = FMT_CONTEXT;
            g_ctx = (size_t)strtoul(optarg, NULL, 10);
            break;
        case 'e':
            g_fmt = FMT_ED;
            break;
        case 'i':
            g_ign_case = 1;
            break;
        case 'L':
            if (nlabels < 2) {
                labels[nlabels++] = optarg;
            }
            break;
        case 'N':
            g_new_file = 1;
            break;
        case 'q':
            g_brief = 1;
            break;
        case 'r':
            g_recursive = 1;
            break;
        case 'u':
            g_fmt = FMT_UNIFIED;
            g_ctx = 3u;
            break;
        case 'U':
            g_fmt = FMT_UNIFIED;
            g_ctx = (size_t)strtoul(optarg, NULL, 10);
            break;
        case 'w':
            g_ign_all_space = 1;
            break;
        default:
            return 2;
        }
    }

    if (argc - optind != 2) {
        (void)fprintf(stderr, "diff: expected two operands\n");
        return 2;
    }

    const char *p1 = argv[optind];
    const char *p2 = argv[optind + 1];
    struct stat st1 = {0};
    struct stat st2 = {0};
    if (strcmp(p1, "-") != 0 && strcmp(p2, "-") != 0 && stat(p1, &st1) == 0 &&
        stat(p2, &st2) == 0 && S_ISDIR(st1.st_mode) && S_ISDIR(st2.st_mode)) {
        return diff_dirs(p1, p2);
    }

    const char *l1 = labels[0] ? labels[0] : p1;
    const char *l2 = labels[1] ? labels[1] : p2;
    return diff_files(p1, p2, l1, l2);
}
