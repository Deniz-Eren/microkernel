/**
 * @file     pr.c
 * @brief    Ring 3 pr print-formatting utility targeting POSIX.1-2024 XCU
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
#include <time.h>

/* need:: COMP_LIBC_0023 */

typedef struct {
    int start_page;
    int cols;
    int across;
    int double_space;
    int formfeed;
    int merge;
    int quiet;
    int omit_hdr;
    int num_lines;
    char num_sep;
    int num_width;
    int offset;
    int page_len;
    int page_width;
    int use_sep;
    char col_sep;
    const char *header;
} pr_opt_t;

static char *pr_getline(FILE *fp) {
    size_t cap = 128;
    size_t len = 0;
    int ch = fgetc(fp);
    if (ch == EOF) {
        return NULL;
    }
    char *buf = malloc(cap);
    if (!buf) {
        return NULL;
    }
    while (ch != EOF && ch != '\n') {
        if (len + 2 > cap) {
            size_t ncap = cap * 2;
            char *nb = realloc(buf, ncap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
            cap = ncap;
        }
        buf[len++] = (char)ch;
        ch = fgetc(fp);
    }
    buf[len] = '\0';
    return buf;
}

static void print_spaces(int n) {
    for (int i = 0; i < n; i++) {
        (void)putchar(' ');
    }
}

static void print_header(const pr_opt_t *o, const char *title, int page) {
    if (o->omit_hdr) {
        return;
    }
    char datebuf[64];
    time_t now = time(NULL);
    struct tm tm_buf;
    if (gmtime_r(&now, &tm_buf)) {
        (void)strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M", &tm_buf);
    } else {
        (void)snprintf(datebuf, sizeof(datebuf), "1970-01-01 00:00");
    }
    (void)putchar('\n');
    (void)putchar('\n');
    print_spaces(o->offset);
    (void)printf("%s  %s  Page %d\n\n\n", datebuf, title ? title : "", page);
}

static void print_trailer(const pr_opt_t *o, int emitted_body, int body_max) {
    if (o->omit_hdr) {
        return;
    }
    if (o->formfeed) {
        (void)putchar('\f');
        return;
    }
    int pad = (body_max - emitted_body) + 5;
    for (int i = 0; i < pad; i++) {
        (void)putchar('\n');
    }
}

static void print_cell(const char *s, int col_w, int is_last,
                       const pr_opt_t *o) {
    const char *text = s ? s : "";
    if (o->use_sep) {
        (void)fputs(text, stdout);
        if (!is_last) {
            (void)putchar(o->col_sep);
        }
        return;
    }
    size_t len = strlen(text);
    int limit = (col_w > 0) ? col_w : (int)len;
    int shown = ((int)len > limit) ? limit : (int)len;
    if (shown > 0) {
        (void)fwrite(text, 1, (size_t)shown, stdout);
    }
    if (!is_last && shown < limit) {
        print_spaces(limit - shown);
    }
}

static int format_single_file(FILE *fp, const char *name, const pr_opt_t *o) {
    const char *title = o->header ? o->header : name;
    int body_lines = o->omit_hdr ? o->page_len : (o->page_len - 10);
    if (body_lines <= 0) {
        body_lines = 1;
    }
    int step = o->double_space ? 2 : 1;
    int rows_per_page = (body_lines + step - 1) / step;
    if (rows_per_page <= 0) {
        rows_per_page = 1;
    }
    int cols = (o->cols > 0) ? o->cols : 1;
    int col_w = (cols > 1) ? (o->page_width / cols) : o->page_width;
    if (col_w < 1) {
        col_w = 1;
    }

    int page = 1;
    unsigned long line_no = 1;

    for (;;) {
        size_t need = (size_t)rows_per_page * (size_t)cols;
        char **lines = calloc(need, sizeof(*lines));
        if (!lines) {
            return -1;
        }
        size_t count = 0;
        while (count < need) {
            char *ln = pr_getline(fp);
            if (!ln) {
                break;
            }
            lines[count++] = ln;
        }
        if (count == 0) {
            free(lines);
            break;
        }

        if (page >= o->start_page) {
            print_header(o, title, page);
            int emitted = 0;
            int rows = o->across
                           ? (int)((count + (size_t)cols - 1) / (size_t)cols)
                           : ((count < (size_t)rows_per_page) ? (int)count
                                                              : rows_per_page);
            for (int r = 0; r < rows; r++) {
                print_spaces(o->offset);
                for (int c = 0; c < cols; c++) {
                    size_t idx = o->across
                                     ? ((size_t)r * (size_t)cols + (size_t)c)
                                     : ((size_t)c * (size_t)rows + (size_t)r);
                    if (idx >= count) {
                        continue;
                    }
                    if (o->num_lines && (cols == 1 || c == 0)) {
                        (void)printf("%*lu%c", o->num_width, line_no + idx,
                                     o->num_sep);
                    }
                    int is_last =
                        (c + 1 == cols) || (o->across && idx + 1 >= count) ||
                        (!o->across &&
                         ((size_t)(c + 1) * (size_t)rows + (size_t)r) >= count);
                    if (cols == 1) {
                        (void)fputs(lines[idx], stdout);
                    } else {
                        print_cell(lines[idx], col_w, is_last, o);
                    }
                }
                (void)putchar('\n');
                emitted++;
                if (o->double_space) {
                    (void)putchar('\n');
                    emitted++;
                }
            }
            print_trailer(o, emitted, body_lines);
        }

        line_no += count;
        for (size_t i = 0; i < count; i++) {
            free(lines[i]);
        }
        free(lines);
        page++;
    }
    return 0;
}

static int format_merge_files(FILE **fps, int nfiles, const pr_opt_t *o) {
    const char *title = o->header ? o->header : "";
    int body_lines = o->omit_hdr ? o->page_len : (o->page_len - 10);
    if (body_lines <= 0) {
        body_lines = 1;
    }
    int step = o->double_space ? 2 : 1;
    int rows_per_page = (body_lines + step - 1) / step;
    if (rows_per_page <= 0) {
        rows_per_page = 1;
    }
    int col_w = (nfiles > 0) ? (o->page_width / nfiles) : o->page_width;
    if (col_w < 1) {
        col_w = 1;
    }

    int page = 1;
    unsigned long line_no = 1;
    char **row = calloc((size_t)nfiles, sizeof(*row));
    if (!row) {
        return -1;
    }

    for (;;) {
        int emitted = 0;
        int page_started = 0;
        for (int r = 0; r < rows_per_page; r++) {
            int any = 0;
            for (int f = 0; f < nfiles; f++) {
                row[f] = fps[f] ? pr_getline(fps[f]) : NULL;
                if (row[f]) {
                    any = 1;
                }
            }
            if (!any) {
                break;
            }
            if (page >= o->start_page) {
                if (!page_started) {
                    print_header(o, title, page);
                    page_started = 1;
                }
                print_spaces(o->offset);
                if (o->num_lines) {
                    (void)printf("%*lu%c", o->num_width, line_no, o->num_sep);
                }
                for (int f = 0; f < nfiles; f++) {
                    print_cell(row[f], col_w, (f + 1 == nfiles), o);
                }
                (void)putchar('\n');
                emitted++;
                if (o->double_space) {
                    (void)putchar('\n');
                    emitted++;
                }
            }
            for (int f = 0; f < nfiles; f++) {
                free(row[f]);
                row[f] = NULL;
            }
            line_no++;
        }
        if (emitted == 0 && !page_started) {
            break;
        }
        if (page >= o->start_page && page_started) {
            print_trailer(o, emitted, body_lines);
        }
        page++;
    }

    free(row);
    return 0;
}

static int parse_int_pos(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0' || v <= 0) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv) {
    pr_opt_t o = {
        .start_page = 1,
        .cols = 1,
        .num_sep = '\t',
        .num_width = 5,
        .page_len = 66,
        .page_width = 72,
        .col_sep = '\t',
    };

    int argi = 1;
    while (argi < argc) {
        const char *a = argv[argi];
        if (strcmp(a, "--") == 0) {
            argi++;
            break;
        }
        if (a[0] == '+' && isdigit((unsigned char)a[1])) {
            if (parse_int_pos(a + 1, &o.start_page) != 0) {
                (void)fprintf(stderr, "pr: invalid page: %s\n", a);
                return 1;
            }
            argi++;
            continue;
        }
        if (a[0] != '-' || a[1] == '\0') {
            break;
        }
        if (isdigit((unsigned char)a[1])) {
            if (parse_int_pos(a + 1, &o.cols) != 0) {
                (void)fprintf(stderr, "pr: invalid column count: %s\n", a);
                return 1;
            }
            argi++;
            continue;
        }
        const char *p = a + 1;
        while (*p) {
            char flag = *p++;
            switch (flag) {
            case 'a':
                o.across = 1;
                break;
            case 'd':
                o.double_space = 1;
                break;
            case 'F':
            case 'f':
                o.formfeed = 1;
                break;
            case 'm':
                o.merge = 1;
                break;
            case 'r':
                o.quiet = 1;
                break;
            case 't':
                o.omit_hdr = 1;
                break;
            case 'h': {
                const char *val = (*p != '\0') ? p : NULL;
                if (!val) {
                    if (argi + 1 >= argc) {
                        (void)fprintf(stderr, "pr: -h requires an argument\n");
                        return 1;
                    }
                    val = argv[++argi];
                }
                o.header = val;
                p = "";
                break;
            }
            case 'l':
            case 'w':
            case 'o': {
                const char *val = (*p != '\0') ? p : NULL;
                if (!val) {
                    if (argi + 1 >= argc) {
                        (void)fprintf(stderr, "pr: -%c requires an argument\n",
                                      flag);
                        return 1;
                    }
                    val = argv[++argi];
                }
                char *end = NULL;
                long v = strtol(val, &end, 10);
                if (!end || *end != '\0' || v < 0 || (flag != 'o' && v == 0)) {
                    (void)fprintf(stderr, "pr: invalid -%c value: %s\n", flag,
                                  val);
                    return 1;
                }
                if (flag == 'l') {
                    o.page_len = (int)v;
                } else if (flag == 'w') {
                    o.page_width = (int)v;
                } else {
                    o.offset = (int)v;
                }
                p = "";
                break;
            }
            case 'n':
                o.num_lines = 1;
                if (*p != '\0') {
                    if (!isdigit((unsigned char)*p)) {
                        o.num_sep = *p++;
                    }
                    if (*p != '\0') {
                        if (parse_int_pos(p, &o.num_width) != 0) {
                            (void)fprintf(stderr, "pr: invalid -n width: %s\n",
                                          p);
                            return 1;
                        }
                    }
                    p = "";
                }
                break;
            case 's':
                o.use_sep = 1;
                if (*p != '\0') {
                    o.col_sep = *p;
                    p = "";
                } else {
                    o.col_sep = '\t';
                }
                break;
            case 'e':
            case 'i':
                p = "";
                break;
            default:
                (void)fprintf(stderr, "pr: unsupported option: -%c\n", flag);
                return 1;
            }
        }
        argi++;
    }

    if (o.page_len <= 10) {
        o.omit_hdr = 1;
    }

    int nfiles = argc - argi;
    if (nfiles <= 0) {
        return (format_single_file(stdin, "", &o) == 0) ? 0 : 1;
    }

    if (o.merge) {
        FILE **fps = calloc((size_t)nfiles, sizeof(*fps));
        if (!fps) {
            return 1;
        }
        int rc = 0;
        for (int i = 0; i < nfiles; i++) {
            const char *path = argv[argi + i];
            if (strcmp(path, "-") == 0) {
                fps[i] = stdin;
            } else {
                fps[i] = fopen(path, "rb");
                if (!fps[i]) {
                    if (!o.quiet) {
                        perror(path);
                    }
                    rc = 1;
                }
            }
        }
        if (format_merge_files(fps, nfiles, &o) != 0) {
            rc = 1;
        }
        for (int i = 0; i < nfiles; i++) {
            if (fps[i] && fps[i] != stdin) {
                (void)fclose(fps[i]);
            }
        }
        free(fps);
        return rc;
    }

    int rc = 0;
    for (int i = argi; i < argc; i++) {
        const char *path = argv[i];
        FILE *fp = stdin;
        if (strcmp(path, "-") != 0) {
            fp = fopen(path, "rb");
            if (!fp) {
                if (!o.quiet) {
                    perror(path);
                }
                rc = 1;
                continue;
            }
        }
        if (format_single_file(fp, (fp == stdin) ? "" : path, &o) != 0) {
            rc = 1;
        }
        if (fp != stdin) {
            (void)fclose(fp);
        }
    }
    return rc;
}
