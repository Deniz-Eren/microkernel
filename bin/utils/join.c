/**
 * @file     join.c
 * @brief    Ring 3 join utility targeting POSIX.1-2024 XCU
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
#include <strings.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

#define MAX_OUT_SPECS 64

typedef struct {
    int file_no; /* 0 = join field, 1 = file1, 2 = file2 */
    size_t field_no;
} out_spec_t;

typedef struct {
    char *raw;
    char **fields;
    size_t nfields;
} jline_t;

typedef struct {
    jline_t *rows;
    size_t count;
    size_t cap;
} jtable_t;

static int g_delim = -1;
static int g_ignore_case;
static size_t g_f1 = 1u;
static size_t g_f2 = 1u;
static int g_unpair1;
static int g_unpair2;
static int g_only_unpair;
static const char *g_empty_str = "";
static out_spec_t g_ospecs[MAX_OUT_SPECS];
static size_t g_nospecs;

static int split_line(const char *src, size_t len, jline_t *out) {
    memset(out, 0, sizeof(*out));
    out->raw = (char *)malloc(len + 1u);
    if (!out->raw) {
        return -1;
    }
    memcpy(out->raw, src, len);
    out->raw[len] = '\0';

    size_t cap = 16u;
    out->fields = (char **)calloc(cap, sizeof(char *));
    if (!out->fields) {
        free(out->raw);
        out->raw = NULL;
        return -1;
    }

    char *p = out->raw;
    if (g_delim >= 0) {
        for (;;) {
            if (out->nfields >= cap) {
                size_t old_cap = cap;
                cap *= 2u;
                char **nf = (char **)realloc(out->fields, cap * sizeof(char *));
                if (!nf) {
                    return -1;
                }
                memset(nf + old_cap, 0, (cap - old_cap) * sizeof(char *));
                out->fields = nf;
            }
            out->fields[out->nfields++] = p;
            char *sep = strchr(p, g_delim);
            if (!sep) {
                break;
            }
            *sep = '\0';
            p = sep + 1;
        }
        return 0;
    }

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (out->nfields >= cap) {
            size_t old_cap = cap;
            cap *= 2u;
            char **nf = (char **)realloc(out->fields, cap * sizeof(char *));
            if (!nf) {
                return -1;
            }
            memset(nf + old_cap, 0, (cap - old_cap) * sizeof(char *));
            out->fields = nf;
        }
        out->fields[out->nfields++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return 0;
}

static int read_table(const char *path, jtable_t *t) {
    memset(t, 0, sizeof(*t));
    FILE *fp = (strcmp(path, "-") == 0) ? stdin : fopen(path, "r");
    if (!fp) {
        perror(path);
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
        if (t->count >= t->cap) {
            size_t ncap = (t->cap == 0u) ? 32u : (t->cap * 2u);
            jline_t *nr = (jline_t *)realloc(t->rows, ncap * sizeof(jline_t));
            if (!nr) {
                rc = -1;
                break;
            }
            t->rows = nr;
            t->cap = ncap;
        }
        if (split_line(line, len, &t->rows[t->count]) != 0) {
            rc = -1;
            break;
        }
        t->count++;
    }
    free(line);
    if (fp != stdin) {
        (void)fclose(fp);
    }
    return rc;
}

static void free_table(jtable_t *t) {
    for (size_t i = 0u; i < t->count; i++) {
        free(t->rows[i].fields);
        free(t->rows[i].raw);
    }
    free(t->rows);
}

static const char *get_field(const jline_t *row, size_t idx1) {
    if (!row || idx1 == 0u || idx1 > row->nfields) {
        return g_empty_str;
    }
    return row->fields[idx1 - 1u];
}

static int cmp_key(const char *a, const char *b) {
    return g_ignore_case ? strcasecmp(a, b) : strcmp(a, b);
}

static void emit_row(const jline_t *r1, const jline_t *r2) {
    int sep = (g_delim >= 0) ? g_delim : ' ';
    const char *jkey = r1 ? get_field(r1, g_f1) : get_field(r2, g_f2);

    if (g_nospecs > 0u) {
        for (size_t i = 0u; i < g_nospecs; i++) {
            if (i > 0u) {
                (void)putchar(sep);
            }
            const char *val = g_empty_str;
            if (g_ospecs[i].file_no == 0) {
                val = jkey;
            } else if (g_ospecs[i].file_no == 1) {
                val = get_field(r1, g_ospecs[i].field_no);
            } else {
                val = get_field(r2, g_ospecs[i].field_no);
            }
            (void)fputs(val, stdout);
        }
        (void)putchar('\n');
        return;
    }

    (void)fputs(jkey, stdout);
    if (r1) {
        for (size_t i = 1u; i <= r1->nfields; i++) {
            if (i == g_f1) {
                continue;
            }
            (void)putchar(sep);
            (void)fputs(get_field(r1, i), stdout);
        }
    }
    if (r2) {
        for (size_t i = 1u; i <= r2->nfields; i++) {
            if (i == g_f2) {
                continue;
            }
            (void)putchar(sep);
            (void)fputs(get_field(r2, i), stdout);
        }
    }
    (void)putchar('\n');
}

static int parse_olist(const char *spec) {
    const char *p = spec;
    while (*p != '\0') {
        while (*p == ',' || *p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (g_nospecs >= MAX_OUT_SPECS) {
            return -1;
        }
        if (*p == '0' &&
            (p[1] == '\0' || p[1] == ',' || p[1] == ' ' || p[1] == '\t')) {
            g_ospecs[g_nospecs].file_no = 0;
            g_ospecs[g_nospecs].field_no = 0u;
            g_nospecs++;
            p++;
            continue;
        }
        if ((*p != '1' && *p != '2') || p[1] != '.') {
            return -1;
        }
        int fno = *p - '0';
        p += 2;
        char *end = NULL;
        unsigned long fld = strtoul(p, &end, 10);
        if (end == p || fld == 0ul) {
            return -1;
        }
        g_ospecs[g_nospecs].file_no = fno;
        g_ospecs[g_nospecs].field_no = (size_t)fld;
        g_nospecs++;
        p = end;
    }
    return 0;
}

int main(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "1:2:a:e:io:t:v:")) != -1) {
        switch (opt) {
        case '1':
            g_f1 = (size_t)strtoul(optarg, NULL, 10);
            if (g_f1 == 0u) {
                return 1;
            }
            break;
        case '2':
            g_f2 = (size_t)strtoul(optarg, NULL, 10);
            if (g_f2 == 0u) {
                return 1;
            }
            break;
        case 'a':
            if (strcmp(optarg, "1") == 0) {
                g_unpair1 = 1;
            } else if (strcmp(optarg, "2") == 0) {
                g_unpair2 = 1;
            } else {
                return 1;
            }
            break;
        case 'v':
            g_only_unpair = 1;
            if (strcmp(optarg, "1") == 0) {
                g_unpair1 = 1;
            } else if (strcmp(optarg, "2") == 0) {
                g_unpair2 = 1;
            } else {
                return 1;
            }
            break;
        case 'e':
            g_empty_str = optarg;
            break;
        case 'i':
            g_ignore_case = 1;
            break;
        case 'o':
            if (parse_olist(optarg) != 0) {
                return 1;
            }
            break;
        case 't':
            if (optarg[0] == '\0' || optarg[1] != '\0') {
                return 1;
            }
            g_delim = (unsigned char)optarg[0];
            break;
        default:
            return 1;
        }
    }

    if (argc - optind != 2) {
        (void)fprintf(stderr, "join: expected two file operands\n");
        return 1;
    }

    jtable_t t1 = {0};
    jtable_t t2 = {0};
    if (read_table(argv[optind], &t1) != 0 ||
        read_table(argv[optind + 1], &t2) != 0) {
        free_table(&t1);
        free_table(&t2);
        return 1;
    }

    size_t i = 0u;
    size_t j = 0u;
    while (i < t1.count && j < t2.count) {
        const char *k1 = get_field(&t1.rows[i], g_f1);
        const char *k2 = get_field(&t2.rows[j], g_f2);
        int c = cmp_key(k1, k2);
        if (c < 0) {
            if (g_unpair1) {
                emit_row(&t1.rows[i], NULL);
            }
            i++;
        } else if (c > 0) {
            if (g_unpair2) {
                emit_row(NULL, &t2.rows[j]);
            }
            j++;
        } else {
            size_t i_end = i + 1u;
            while (i_end < t1.count &&
                   cmp_key(get_field(&t1.rows[i_end], g_f1), k1) == 0) {
                i_end++;
            }
            size_t j_end = j + 1u;
            while (j_end < t2.count &&
                   cmp_key(get_field(&t2.rows[j_end], g_f2), k2) == 0) {
                j_end++;
            }
            if (!g_only_unpair) {
                for (size_t a = i; a < i_end; a++) {
                    for (size_t b = j; b < j_end; b++) {
                        emit_row(&t1.rows[a], &t2.rows[b]);
                    }
                }
            }
            i = i_end;
            j = j_end;
        }
    }
    while (i < t1.count) {
        if (g_unpair1) {
            emit_row(&t1.rows[i], NULL);
        }
        i++;
    }
    while (j < t2.count) {
        if (g_unpair2) {
            emit_row(NULL, &t2.rows[j]);
        }
        j++;
    }

    free_table(&t1);
    free_table(&t2);
    return 0;
}
