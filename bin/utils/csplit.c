/**
 * @file     csplit.c
 * @brief    Ring 3 csplit utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0023 */

typedef struct {
    char **lines;
    size_t *lens;
    size_t count;
    size_t cap;
} doc_t;

static void doc_free(doc_t *d) {
    for (size_t i = 0u; i < d->count; i++) {
        free(d->lines[i]);
    }
    free(d->lines);
    free(d->lens);
    memset(d, 0, sizeof(*d));
}

static int doc_read(FILE *fp, doc_t *d) {
    memset(d, 0, sizeof(*d));
    char *line = NULL;
    size_t lcap = 0u;
    ssize_t n;
    while ((n = getline(&line, &lcap, fp)) >= 0) {
        if (d->count >= d->cap) {
            size_t old_cap = d->cap;
            size_t ncap = (d->cap == 0u) ? 64u : (d->cap * 2u);
            char **nl = (char **)realloc(d->lines, ncap * sizeof(char *));
            if (!nl) {
                free(line);
                doc_free(d);
                return -1;
            }
            memset(nl + old_cap, 0, (ncap - old_cap) * sizeof(char *));
            d->lines = nl;
            size_t *nlen = (size_t *)realloc(d->lens, ncap * sizeof(size_t));
            if (!nlen) {
                free(line);
                doc_free(d);
                return -1;
            }
            memset(nlen + old_cap, 0, (ncap - old_cap) * sizeof(size_t));
            d->lens = nlen;
            d->cap = ncap;
        }
        char *copy = (char *)calloc(1u, (size_t)n + 1u);
        if (!copy) {
            free(line);
            doc_free(d);
            return -1;
        }
        memcpy(copy, line, (size_t)n);
        d->lines[d->count] = copy;
        d->lens[d->count] = (size_t)n;
        d->count++;
    }
    free(line);
    return 0;
}

static int emit_chunk(const doc_t *d, size_t from, size_t to,
                      const char *prefix, int width, unsigned long idx,
                      int silent, char created[][256], size_t *ncreated) {
    char path[256];
    (void)snprintf(path, sizeof(path), "%s%0*lu", prefix, width, idx);
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return -1;
    }
    if (*ncreated < 128u) {
        strncpy(created[*ncreated], path, 255u);
        created[*ncreated][255] = '\0';
        (*ncreated)++;
    }
    size_t bytes = 0u;
    for (size_t i = from; i < to && i < d->count; i++) {
        if (d->lens[i] > 0u) {
            if (fwrite(d->lines[i], 1u, d->lens[i], fp) != d->lens[i]) {
                (void)fclose(fp);
                return -1;
            }
            bytes += d->lens[i];
        }
    }
    if (fclose(fp) != 0) {
        return -1;
    }
    if (!silent) {
        (void)printf("%zu\n", bytes);
    }
    return 0;
}

static int parse_regex_arg(const char *arg, char **pat_out, long *off_out,
                           int *skip_out) {
    char delim = arg[0];
    *skip_out = (delim == '%');
    const char *p = arg + 1;
    const char *end = NULL;
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p += 2;
            continue;
        }
        if (*p == delim) {
            end = p;
            break;
        }
        p++;
    }
    if (!end) {
        return -1;
    }
    size_t plen = (size_t)(end - (arg + 1));
    char *pat = (char *)calloc(1u, plen + 1u);
    if (!pat) {
        return -1;
    }
    memcpy(pat, arg + 1, plen);
    *pat_out = pat;
    *off_out = 0L;
    if (end[1] != '\0') {
        char *oe = NULL;
        *off_out = strtol(end + 1, &oe, 10);
        if (oe == end + 1 || *oe != '\0') {
            free(pat);
            return -1;
        }
    }
    return 0;
}

static int apply_pattern(const doc_t *d, const char *arg, size_t *cur,
                         const char *prefix, int width, unsigned long *fidx,
                         int silent, char created[][256], size_t *ncreated) {
    if (arg[0] == '/' || arg[0] == '%') {
        char *pat = NULL;
        long offset = 0L;
        int skip = 0;
        if (parse_regex_arg(arg, &pat, &offset, &skip) != 0) {
            return -1;
        }
        regex_t re;
        if (regcomp(&re, pat, REG_NOSUB) != 0) {
            free(pat);
            return -1;
        }
        free(pat);
        size_t start_search = (*cur < d->count) ? *cur : d->count;
        if (start_search<d->count && * fidx> 0ul && offset <= 0L) {
            /* Advance at least one line when matching next regex occurrence. */
            start_search++;
        }
        size_t match_idx = d->count;
        for (size_t i = start_search; i < d->count; i++) {
            /* Strip trailing newline when matching against BRE. */
            size_t ln = d->lens[i];
            if (ln > 0u && d->lines[i][ln - 1u] == '\n') {
                ln--;
            }
            char *tmp = (char *)calloc(1u, ln + 1u);
            if (!tmp) {
                regfree(&re);
                return -1;
            }
            if (ln > 0u) {
                memcpy(tmp, d->lines[i], ln);
            }
            int ok = (regexec(&re, tmp, 0u, NULL, 0) == 0);
            free(tmp);
            if (ok) {
                match_idx = i;
                break;
            }
        }
        regfree(&re);
        if (match_idx >= d->count) {
            return -2; /* no match */
        }
        long target_l = (long)match_idx + offset;
        if (target_l < (long)*cur || target_l > (long)d->count) {
            return -1;
        }
        size_t target = (size_t)target_l;
        if (!skip) {
            if (emit_chunk(d, *cur, target, prefix, width, (*fidx)++, silent,
                           created, ncreated) != 0) {
                return -1;
            }
        }
        *cur = target;
        return 0;
    }

    /* Numeric line number (1-based). */
    char *end = NULL;
    unsigned long lno = strtoul(arg, &end, 10);
    if (end == arg || *end != '\0' || lno == 0ul) {
        return -1;
    }
    size_t target = (size_t)(lno - 1ul);
    if (target < *cur || target > d->count) {
        return -2;
    }
    if (emit_chunk(d, *cur, target, prefix, width, (*fidx)++, silent, created,
                   ncreated) != 0) {
        return -1;
    }
    *cur = target;
    return 0;
}

int main(int argc, char **argv) {
    const char *prefix = "xx";
    int width = 2;
    int keep = 0;
    int silent = 0;
    int opt;
    while ((opt = getopt(argc, argv, "f:kn:s")) != -1) {
        switch (opt) {
        case 'f':
            prefix = optarg;
            break;
        case 'k':
            keep = 1;
            break;
        case 'n':
            width = (int)strtol(optarg, NULL, 10);
            if (width <= 0 || width > 10) {
                return 1;
            }
            break;
        case 's':
            silent = 1;
            break;
        default:
            return 1;
        }
    }

    if (argc - optind < 2) {
        (void)fprintf(stderr, "csplit: missing operands\n");
        return 1;
    }

    const char *in_path = argv[optind++];
    FILE *in = (strcmp(in_path, "-") == 0) ? stdin : fopen(in_path, "rb");
    if (!in) {
        perror(in_path);
        return 1;
    }
    doc_t doc = {0};
    int rd = doc_read(in, &doc);
    if (in != stdin) {
        (void)fclose(in);
    }
    if (rd != 0) {
        return 1;
    }

    static char created[128][256];
    size_t ncreated = 0u;
    size_t cur = 0u;
    unsigned long fidx = 0ul;
    int rc = 0;

    for (int i = optind; i < argc && rc == 0; i++) {
        const char *arg = argv[i];
        int rep = 1;
        int rep_star = 0;
        if (i + 1 < argc && argv[i + 1][0] == '{') {
            const char *rp = argv[i + 1] + 1;
            if (rp[0] == '*' && rp[1] == '}' && rp[2] == '\0') {
                rep_star = 1;
            } else {
                char *end = NULL;
                unsigned long rv = strtoul(rp, &end, 10);
                if (end == rp || *end != '}' || end[1] != '\0') {
                    rc = 1;
                    break;
                }
                rep = 1 + (int)rv;
            }
            i++;
        }

        if (rep_star) {
            while (cur < doc.count) {
                int ar = apply_pattern(&doc, arg, &cur, prefix, width, &fidx,
                                       silent, created, &ncreated);
                if (ar == -2) {
                    break;
                }
                if (ar != 0) {
                    rc = 1;
                    break;
                }
            }
        } else {
            for (int k = 0; k < rep; k++) {
                if (apply_pattern(&doc, arg, &cur, prefix, width, &fidx, silent,
                                  created, &ncreated) != 0) {
                    (void)fprintf(stderr, "csplit: '%s': out of range\n", arg);
                    rc = 1;
                    break;
                }
            }
        }
    }

    if (rc == 0 && cur < doc.count) {
        if (emit_chunk(&doc, cur, doc.count, prefix, width, fidx++, silent,
                       created, &ncreated) != 0) {
            rc = 1;
        }
    }

    if (rc != 0 && !keep) {
        for (size_t i = 0u; i < ncreated; i++) {
            (void)unlink(created[i]);
        }
    }
    doc_free(&doc);
    return rc;
}
