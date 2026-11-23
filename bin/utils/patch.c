/**
 * @file     patch.c
 * @brief    Ring 3 patch utility targeting POSIX.1-2024 XCU (-p, -i, -o, -R)
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

/* need:: COMP_LIBC_0021 */

typedef struct {
    char **lines;
    size_t count;
    size_t cap;
} vec_t;

static void vec_free(vec_t *v) {
    for (size_t i = 0u; i < v->count; i++) {
        free(v->lines[i]);
    }
    free(v->lines);
    memset(v, 0, sizeof(*v));
}

static int vec_push(vec_t *v, const char *s, size_t len) {
    if (v->count >= v->cap) {
        size_t old_cap = v->cap;
        size_t ncap = (v->cap == 0u) ? 64u : (v->cap * 2u);
        char **nl = (char **)realloc(v->lines, ncap * sizeof(char *));
        if (!nl) {
            return -1;
        }
        memset(nl + old_cap, 0, (ncap - old_cap) * sizeof(char *));
        v->lines = nl;
        v->cap = ncap;
    }
    char *copy = (char *)calloc(1u, len + 1u);
    if (!copy) {
        return -1;
    }
    if (len > 0u) {
        memcpy(copy, s, len);
    }
    copy[len] = '\0';
    v->lines[v->count++] = copy;
    return 0;
}

static int read_all_lines(FILE *fp, vec_t *v) {
    memset(v, 0, sizeof(*v));
    char *line = NULL;
    size_t cap = 0u;
    ssize_t n;
    while ((n = getline(&line, &cap, fp)) >= 0) {
        size_t len = (size_t)n;
        if (len > 0u && line[len - 1u] == '\n') {
            len--;
        }
        if (vec_push(v, line, len) != 0) {
            free(line);
            vec_free(v);
            return -1;
        }
    }
    free(line);
    return 0;
}

static int read_file_vec(const char *path, vec_t *v) {
    memset(v, 0, sizeof(*v));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }
    int rc = read_all_lines(fp, v);
    (void)fclose(fp);
    return rc;
}

static int write_file_vec(const char *path, const vec_t *v) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return -1;
    }
    for (size_t i = 0u; i < v->count; i++) {
        (void)fputs(v->lines[i], fp);
        (void)fputc('\n', fp);
    }
    return fclose(fp) == 0 ? 0 : -1;
}

static int strip_path(const char *raw, int strip, char *out, size_t cap) {
    if (!raw) {
        return -1;
    }
    const char *p = raw;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    const char *end = p;
    while (*end != '\0' && *end != '\t' && *end != '\n' && *end != '\r') {
        end++;
    }
    while (end > p && end[-1] == ' ') {
        end--;
    }
    if (strip < 0) {
        const char *slash = NULL;
        for (const char *q = p; q < end; q++) {
            if (*q == '/') {
                slash = q;
            }
        }
        if (slash) {
            p = slash + 1;
        }
    } else {
        int s = strip;
        while (s > 0 && p < end) {
            while (p < end && *p != '/') {
                p++;
            }
            while (p < end && *p == '/') {
                p++;
            }
            s--;
        }
    }
    size_t len = (size_t)(end - p);
    if (len == 0u || len >= cap) {
        return -1;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

static int splice_lines(vec_t *doc, size_t pos, size_t del_cnt,
                        const vec_t *ins) {
    if (pos > doc->count || del_cnt > doc->count - pos) {
        return -1;
    }
    size_t new_cnt = doc->count - del_cnt + ins->count;
    char **arr = NULL;
    if (new_cnt > 0u) {
        arr = (char **)calloc(new_cnt, sizeof(char *));
        if (!arr) {
            return -1;
        }
    }
    size_t k = 0u;
    for (size_t i = 0u; i < pos; i++) {
        arr[k++] = doc->lines[i];
    }
    for (size_t i = 0u; i < del_cnt; i++) {
        free(doc->lines[pos + i]);
    }
    for (size_t i = 0u; i < ins->count; i++) {
        arr[k] = strdup(ins->lines[i]);
        if (!arr[k]) {
            for (size_t j = pos; j < k; j++) {
                free(arr[j]);
            }
            free(arr);
            return -1;
        }
        k++;
    }
    for (size_t i = pos + del_cnt; i < doc->count; i++) {
        arr[k++] = doc->lines[i];
    }
    free(doc->lines);
    doc->lines = arr;
    doc->count = new_cnt;
    doc->cap = new_cnt;
    return 0;
}

static int match_at(const vec_t *doc, size_t pos, const vec_t *old_lines) {
    if (pos + old_lines->count > doc->count) {
        return 0;
    }
    for (size_t i = 0u; i < old_lines->count; i++) {
        if (strcmp(doc->lines[pos + i], old_lines->lines[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

static int find_hunk_pos(const vec_t *doc, size_t hint, const vec_t *old_lines,
                         size_t *out_pos) {
    if (old_lines->count == 0u) {
        *out_pos = (hint <= doc->count) ? hint : doc->count;
        return 0;
    }
    if (hint <= doc->count && match_at(doc, hint, old_lines)) {
        *out_pos = hint;
        return 0;
    }
    for (size_t delta = 1u; delta <= doc->count; delta++) {
        if (hint >= delta && match_at(doc, hint - delta, old_lines)) {
            *out_pos = hint - delta;
            return 0;
        }
        if (hint + delta <= doc->count &&
            match_at(doc, hint + delta, old_lines)) {
            *out_pos = hint + delta;
            return 0;
        }
    }
    return -1;
}

static int parse_unified_hdr(const char *s, size_t *o_start, size_t *o_cnt,
                             size_t *n_start, size_t *n_cnt) {
    if (strncmp(s, "@@ -", 4u) != 0) {
        return -1;
    }
    const char *p = s + 4;
    char *end = NULL;
    *o_start = (size_t)strtoul(p, &end, 10);
    if (end == p) {
        return -1;
    }
    p = end;
    if (*p == ',') {
        p++;
        *o_cnt = (size_t)strtoul(p, &end, 10);
        p = end;
    } else {
        *o_cnt = 1u;
    }
    if (strncmp(p, " +", 2u) != 0) {
        return -1;
    }
    p += 2;
    *n_start = (size_t)strtoul(p, &end, 10);
    if (end == p) {
        return -1;
    }
    p = end;
    if (*p == ',') {
        p++;
        *n_cnt = (size_t)strtoul(p, &end, 10);
    } else {
        *n_cnt = 1u;
    }
    return 0;
}

static int apply_unified(vec_t *doc, const vec_t *patch, size_t start_idx,
                         int reverse) {
    long offset = 0;
    size_t i = start_idx;
    while (i < patch->count) {
        if (strncmp(patch->lines[i], "--- ", 4u) == 0 &&
            i + 1u < patch->count &&
            strncmp(patch->lines[i + 1u], "+++ ", 4u) == 0) {
            break;
        }
        size_t os = 0u;
        size_t oc = 0u;
        size_t ns = 0u;
        size_t nc = 0u;
        if (parse_unified_hdr(patch->lines[i], &os, &oc, &ns, &nc) != 0) {
            i++;
            continue;
        }
        i++;
        vec_t old_v = {0};
        vec_t new_v = {0};
        size_t seen_o = 0u;
        size_t seen_n = 0u;
        while (i < patch->count && (seen_o < oc || seen_n < nc)) {
            const char *ln = patch->lines[i];
            if (ln[0] == '\\') {
                i++;
                continue;
            }
            if (ln[0] == ' ' || ln[0] == '\0') {
                const char *txt = (ln[0] == ' ') ? (ln + 1) : "";
                if (vec_push(&old_v, txt, strlen(txt)) != 0 ||
                    vec_push(&new_v, txt, strlen(txt)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    return -1;
                }
                seen_o++;
                seen_n++;
                i++;
            } else if (ln[0] == '-') {
                if (vec_push(&old_v, ln + 1, strlen(ln + 1)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    return -1;
                }
                seen_o++;
                i++;
            } else if (ln[0] == '+') {
                if (vec_push(&new_v, ln + 1, strlen(ln + 1)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    return -1;
                }
                seen_n++;
                i++;
            } else {
                break;
            }
        }
        const vec_t *want = reverse ? &new_v : &old_v;
        const vec_t *repl = reverse ? &old_v : &new_v;
        size_t base_ln = reverse ? ns : os;
        long hint_l = (base_ln > 0u ? (long)(base_ln - 1u) : 0L) + offset;
        size_t hint = (hint_l < 0L) ? 0u : (size_t)hint_l;
        size_t pos = 0u;
        if (find_hunk_pos(doc, hint, want, &pos) != 0 ||
            splice_lines(doc, pos, want->count, repl) != 0) {
            vec_free(&old_v);
            vec_free(&new_v);
            return -1;
        }
        offset += (long)repl->count - (long)want->count;
        vec_free(&old_v);
        vec_free(&new_v);
    }
    return 0;
}

static int parse_normal_cmd(const char *s, size_t *a1, size_t *a2, char *cmd,
                            size_t *b1, size_t *b2) {
    if (!isdigit((unsigned char)s[0])) {
        return -1;
    }
    char *end = NULL;
    *a1 = (size_t)strtoul(s, &end, 10);
    *a2 = *a1;
    if (*end == ',') {
        *a2 = (size_t)strtoul(end + 1, &end, 10);
    }
    if (*end != 'a' && *end != 'c' && *end != 'd') {
        return -1;
    }
    *cmd = *end++;
    if (!isdigit((unsigned char)*end)) {
        return -1;
    }
    *b1 = (size_t)strtoul(end, &end, 10);
    *b2 = *b1;
    if (*end == ',') {
        *b2 = (size_t)strtoul(end + 1, &end, 10);
    }
    return (*end == '\0') ? 0 : -1;
}

static int apply_normal(vec_t *doc, const vec_t *patch, int reverse) {
    long offset = 0;
    size_t i = 0u;
    while (i < patch->count) {
        size_t a1 = 0u;
        size_t a2 = 0u;
        size_t b1 = 0u;
        size_t b2 = 0u;
        char cmd = 0;
        if (parse_normal_cmd(patch->lines[i], &a1, &a2, &cmd, &b1, &b2) != 0) {
            i++;
            continue;
        }
        i++;
        vec_t old_v = {0};
        vec_t new_v = {0};
        while (i < patch->count && strncmp(patch->lines[i], "< ", 2u) == 0) {
            if (vec_push(&old_v, patch->lines[i] + 2,
                         strlen(patch->lines[i] + 2)) != 0) {
                vec_free(&old_v);
                vec_free(&new_v);
                return -1;
            }
            i++;
        }
        if (i < patch->count && strcmp(patch->lines[i], "---") == 0) {
            i++;
        }
        while (i < patch->count && strncmp(patch->lines[i], "> ", 2u) == 0) {
            if (vec_push(&new_v, patch->lines[i] + 2,
                         strlen(patch->lines[i] + 2)) != 0) {
                vec_free(&old_v);
                vec_free(&new_v);
                return -1;
            }
            i++;
        }

        const vec_t *want = reverse ? &new_v : &old_v;
        const vec_t *repl = reverse ? &old_v : &new_v;
        size_t pos = 0u;
        long hint_l = 0;
        if (!reverse) {
            hint_l = (cmd == 'a') ? ((long)a1 + offset)
                                  : ((long)(a1 > 0u ? a1 - 1u : 0u) + offset);
        } else {
            hint_l = (cmd == 'd') ? ((long)b1 + offset)
                                  : ((long)(b1 > 0u ? b1 - 1u : 0u) + offset);
        }
        size_t hint = (hint_l < 0L) ? 0u : (size_t)hint_l;
        if (find_hunk_pos(doc, hint, want, &pos) != 0 ||
            splice_lines(doc, pos, want->count, repl) != 0) {
            vec_free(&old_v);
            vec_free(&new_v);
            return -1;
        }
        offset += (long)repl->count - (long)want->count;
        vec_free(&old_v);
        vec_free(&new_v);
    }
    return 0;
}

static int apply_context(vec_t *doc, const vec_t *patch, size_t start_idx,
                         int reverse) {
    long offset = 0;
    size_t i = start_idx;
    while (i < patch->count) {
        if (strcmp(patch->lines[i], "***************") != 0) {
            i++;
            continue;
        }
        i++;
        if (i >= patch->count || strncmp(patch->lines[i], "*** ", 4u) != 0) {
            return -1;
        }
        size_t o_start = (size_t)strtoul(patch->lines[i] + 4, NULL, 10);
        i++;
        vec_t old_v = {0};
        vec_t new_v = {0};
        vec_t old_ctx = {0};
        vec_t new_ctx = {0};
        while (i < patch->count && strncmp(patch->lines[i], "--- ", 4u) != 0) {
            const char *ln = patch->lines[i];
            if (strlen(ln) >= 2u &&
                (ln[0] == ' ' || ln[0] == '-' || ln[0] == '!') &&
                ln[1] == ' ') {
                if (vec_push(&old_v, ln + 2, strlen(ln + 2)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    vec_free(&old_ctx);
                    vec_free(&new_ctx);
                    return -1;
                }
                if (ln[0] == ' ' &&
                    vec_push(&old_ctx, ln + 2, strlen(ln + 2)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    vec_free(&old_ctx);
                    vec_free(&new_ctx);
                    return -1;
                }
            }
            i++;
        }
        if (i >= patch->count || strncmp(patch->lines[i], "--- ", 4u) != 0) {
            vec_free(&old_v);
            vec_free(&new_v);
            vec_free(&old_ctx);
            vec_free(&new_ctx);
            return -1;
        }
        size_t n_start = (size_t)strtoul(patch->lines[i] + 4, NULL, 10);
        i++;
        while (i < patch->count &&
               strcmp(patch->lines[i], "***************") != 0) {
            const char *ln = patch->lines[i];
            if (strlen(ln) >= 2u &&
                (ln[0] == ' ' || ln[0] == '+' || ln[0] == '!') &&
                ln[1] == ' ') {
                if (vec_push(&new_v, ln + 2, strlen(ln + 2)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    vec_free(&old_ctx);
                    vec_free(&new_ctx);
                    return -1;
                }
                if (ln[0] == ' ' &&
                    vec_push(&new_ctx, ln + 2, strlen(ln + 2)) != 0) {
                    vec_free(&old_v);
                    vec_free(&new_v);
                    vec_free(&old_ctx);
                    vec_free(&new_ctx);
                    return -1;
                }
            } else {
                break;
            }
            i++;
        }
        const vec_t *eff_old = (old_v.count > 0u) ? &old_v : &new_ctx;
        const vec_t *eff_new = (new_v.count > 0u) ? &new_v : &old_ctx;
        const vec_t *want = reverse ? eff_new : eff_old;
        const vec_t *repl = reverse ? eff_old : eff_new;
        size_t base_ln = reverse ? n_start : o_start;
        long hint_l = (base_ln > 0u ? (long)(base_ln - 1u) : 0L) + offset;
        size_t hint = (hint_l < 0L) ? 0u : (size_t)hint_l;
        size_t pos = 0u;
        if (find_hunk_pos(doc, hint, want, &pos) != 0 ||
            splice_lines(doc, pos, want->count, repl) != 0) {
            vec_free(&old_v);
            vec_free(&new_v);
            vec_free(&old_ctx);
            vec_free(&new_ctx);
            return -1;
        }
        offset += (long)repl->count - (long)want->count;
        vec_free(&old_v);
        vec_free(&new_v);
        vec_free(&old_ctx);
        vec_free(&new_ctx);
    }
    return 0;
}

int main(int argc, char **argv) {
    int strip = -1;
    int reverse = 0;
    int backup = 0;
    const char *patch_path = NULL;
    const char *out_path = NULL;
    const char *chdir_path = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "bcCd:ei:lNnNo:p:r:Ru")) != -1) {
        switch (opt) {
        case 'b':
            backup = 1;
            break;
        case 'd':
            chdir_path = optarg;
            break;
        case 'i':
            patch_path = optarg;
            break;
        case 'o':
            out_path = optarg;
            break;
        case 'p':
            strip = (int)strtol(optarg, NULL, 10);
            break;
        case 'R':
            reverse = 1;
            break;
        case 'c':
        case 'e':
        case 'l':
        case 'n':
        case 'N':
        case 'r':
        case 'u':
            break;
        default:
            return 2;
        }
    }

    if (chdir_path && chdir(chdir_path) != 0) {
        perror(chdir_path);
        return 2;
    }

    FILE *pfp = stdin;
    if (patch_path && strcmp(patch_path, "-") != 0) {
        pfp = fopen(patch_path, "r");
        if (!pfp) {
            perror(patch_path);
            return 2;
        }
    }
    vec_t patch = {0};
    if (read_all_lines(pfp, &patch) != 0) {
        if (pfp != stdin) {
            (void)fclose(pfp);
        }
        return 2;
    }
    if (pfp != stdin) {
        (void)fclose(pfp);
    }

    /* Detect patch format (unified, context, or normal) and target filename. */
    int kind = 0; /* 0 = normal, 1 = unified, 2 = context */
    size_t hdr_idx = 0u;
    char target[256] = {0};
    for (size_t i = 0u; i + 1u < patch.count; i++) {
        if (strncmp(patch.lines[i], "--- ", 4u) == 0 &&
            strncmp(patch.lines[i + 1u], "+++ ", 4u) == 0) {
            kind = 1;
            hdr_idx = i + 2u;
            (void)strip_path(patch.lines[i] + 4, strip, target, sizeof(target));
            break;
        }
        if (strncmp(patch.lines[i], "*** ", 4u) == 0 &&
            strncmp(patch.lines[i + 1u], "--- ", 4u) == 0) {
            kind = 2;
            hdr_idx = i + 2u;
            (void)strip_path(patch.lines[i] + 4, strip, target, sizeof(target));
            break;
        }
    }

    const char *in_file = (optind < argc) ? argv[optind] : target;
    if (!in_file || in_file[0] == '\0') {
        (void)fprintf(stderr, "patch: missing target file\n");
        vec_free(&patch);
        return 2;
    }

    vec_t doc = {0};
    if (read_file_vec(in_file, &doc) != 0) {
        perror(in_file);
        vec_free(&patch);
        return 2;
    }

    if (backup && !out_path) {
        char bak[300];
        (void)snprintf(bak, sizeof(bak), "%s.orig", in_file);
        (void)write_file_vec(bak, &doc);
    }

    int rc = 0;
    if (kind == 1) {
        rc = apply_unified(&doc, &patch, hdr_idx, reverse);
    } else if (kind == 2) {
        rc = apply_context(&doc, &patch, hdr_idx, reverse);
    } else {
        rc = apply_normal(&doc, &patch, reverse);
    }

    if (rc != 0) {
        (void)fprintf(stderr, "patch: failed to apply hunk to %s\n", in_file);
        vec_free(&doc);
        vec_free(&patch);
        return 1;
    }

    const char *dest = out_path ? out_path : in_file;
    if (write_file_vec(dest, &doc) != 0) {
        vec_free(&doc);
        vec_free(&patch);
        return 2;
    }

    vec_free(&doc);
    vec_free(&patch);
    return 0;
}
