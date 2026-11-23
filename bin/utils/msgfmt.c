/**
 * @file     msgfmt.c
 * @brief    Ring 3 msgfmt utility compiling .po files into binary .mo catalogs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0015 */

#define MO_MAGIC_LE 0x950412DEu
#define MAX_PLURALS 8

struct strbuf {
    char *data;
    size_t len;
    size_t cap;
};

static int sb_push(struct strbuf *sb, char c) {
    if (sb->len + 2u > sb->cap) {
        size_t ncap = sb->cap ? sb->cap * 2u : 64u;
        char *nd = (char *)realloc(sb->data, ncap);
        if (!nd) {
            return -1;
        }
        sb->data = nd;
        sb->cap = ncap;
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
    return 0;
}

static void sb_reset(struct strbuf *sb) {
    sb->len = 0u;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

struct mo_pair {
    char *orig;
    size_t orig_len;
    char *trans;
    size_t trans_len;
};

struct mo_table {
    struct mo_pair *items;
    size_t len;
    size_t cap;
};

static int parse_quoted(const char *p, struct strbuf *dst) {
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++;
    while (*p != '\0' && *p != '"') {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            char c = *p;
            if (c == 'n') {
                c = '\n';
            } else if (c == 't') {
                c = '\t';
            } else if (c == 'r') {
                c = '\r';
            } else if (c == 'b') {
                c = '\b';
            } else if (c == 'f') {
                c = '\f';
            } else if (c == 'v') {
                c = '\v';
            } else if (c == 'a') {
                c = '\a';
            } else if (c >= '0' && c <= '7') {
                unsigned v = (unsigned)(c - '0');
                for (int k = 0; k < 2 && p[1] >= '0' && p[1] <= '7'; k++) {
                    p++;
                    v = v * 8u + (unsigned)(*p - '0');
                }
                c = (char)(v & 0xFFu);
            }
            if (sb_push(dst, c) != 0) {
                return -1;
            }
            p++;
        } else {
            if (sb_push(dst, *p++) != 0) {
                return -1;
            }
        }
    }
    return (*p == '"') ? 0 : -1;
}

static int table_add(struct mo_table *tbl, const char *orig, size_t orig_len,
                     const char *trans, size_t trans_len) {
    for (size_t i = 0u; i < tbl->len; i++) {
        if (tbl->items[i].orig_len == orig_len &&
            memcmp(tbl->items[i].orig, orig, orig_len) == 0) {
            char *nt = (char *)malloc(trans_len + 1u);
            if (!nt) {
                return -1;
            }
            memcpy(nt, trans, trans_len);
            nt[trans_len] = '\0';
            free(tbl->items[i].trans);
            tbl->items[i].trans = nt;
            tbl->items[i].trans_len = trans_len;
            return 0;
        }
    }
    if (tbl->len == tbl->cap) {
        size_t ncap = tbl->cap ? tbl->cap * 2u : 32u;
        struct mo_pair *ni =
            (struct mo_pair *)reallocarray(tbl->items, ncap, sizeof(*ni));
        if (!ni) {
            return -1;
        }
        tbl->items = ni;
        tbl->cap = ncap;
    }
    char *no = (char *)malloc(orig_len + 1u);
    char *nt = (char *)malloc(trans_len + 1u);
    if (!no || !nt) {
        free(no);
        free(nt);
        return -1;
    }
    memcpy(no, orig, orig_len);
    no[orig_len] = '\0';
    memcpy(nt, trans, trans_len);
    nt[trans_len] = '\0';
    tbl->items[tbl->len].orig = no;
    tbl->items[tbl->len].orig_len = orig_len;
    tbl->items[tbl->len].trans = nt;
    tbl->items[tbl->len].trans_len = trans_len;
    tbl->len++;
    return 0;
}

struct po_entry {
    int has_id;
    int has_ctx;
    int has_plural;
    int has_str;
    int plural_count;
    struct strbuf ctx;
    struct strbuf id;
    struct strbuf id_plural;
    struct strbuf str[MAX_PLURALS];
};

static int flush_entry(struct po_entry *e, struct mo_table *tbl) {
    if (!e->has_id || !e->has_str) {
        return 0;
    }
    /* Skip untranslated non-header entries. */
    if (e->id.len > 0u && e->str[0].len == 0u && !e->has_plural) {
        return 0;
    }
    struct strbuf key = {0};
    if (e->has_ctx) {
        for (size_t i = 0u; i < e->ctx.len; i++) {
            (void)sb_push(&key, e->ctx.data[i]);
        }
        (void)sb_push(&key, '\x04');
    }
    for (size_t i = 0u; i < e->id.len; i++) {
        (void)sb_push(&key, e->id.data[i]);
    }
    if (e->has_plural) {
        (void)sb_push(&key, '\0');
        for (size_t i = 0u; i < e->id_plural.len; i++) {
            (void)sb_push(&key, e->id_plural.data[i]);
        }
    }

    struct strbuf val = {0};
    int nforms = e->has_plural ? e->plural_count : 1;
    if (nforms < 1) {
        nforms = 1;
    }
    for (int f = 0; f < nforms; f++) {
        if (f > 0) {
            (void)sb_push(&val, '\0');
        }
        for (size_t i = 0u; i < e->str[f].len; i++) {
            (void)sb_push(&val, e->str[f].data[i]);
        }
    }

    int rc = table_add(tbl, key.data ? key.data : "", key.len,
                       val.data ? val.data : "", val.len);
    free(key.data);
    free(val.data);
    return rc;
}

static void reset_entry(struct po_entry *e) {
    e->has_id = 0;
    e->has_ctx = 0;
    e->has_plural = 0;
    e->has_str = 0;
    e->plural_count = 0;
    sb_reset(&e->ctx);
    sb_reset(&e->id);
    sb_reset(&e->id_plural);
    for (int i = 0; i < MAX_PLURALS; i++) {
        sb_reset(&e->str[i]);
    }
}

static void free_entry(struct po_entry *e) {
    free(e->ctx.data);
    free(e->id.data);
    free(e->id_plural.data);
    for (int i = 0; i < MAX_PLURALS; i++) {
        free(e->str[i].data);
    }
}

static int parse_po_file(FILE *fp, struct mo_table *tbl) {
    struct po_entry e;
    memset(&e, 0, sizeof(e));
    struct strbuf *cur = NULL;
    char line[2048];

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *p = line;
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            if (flush_entry(&e, tbl) != 0) {
                free_entry(&e);
                return -1;
            }
            reset_entry(&e);
            cur = NULL;
            continue;
        }
        if (*p == '#') {
            continue;
        }
        if (strncmp(p, "msgctxt", 7u) == 0 && isspace((unsigned char)p[7])) {
            if (e.has_id) {
                if (flush_entry(&e, tbl) != 0) {
                    free_entry(&e);
                    return -1;
                }
                reset_entry(&e);
            }
            e.has_ctx = 1;
            sb_reset(&e.ctx);
            cur = &e.ctx;
            if (parse_quoted(p + 7, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (strncmp(p, "msgid_plural", 12u) == 0 &&
                   isspace((unsigned char)p[12])) {
            e.has_plural = 1;
            sb_reset(&e.id_plural);
            cur = &e.id_plural;
            if (parse_quoted(p + 12, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (strncmp(p, "msgid", 5u) == 0 &&
                   isspace((unsigned char)p[5])) {
            if (e.has_id) {
                if (flush_entry(&e, tbl) != 0) {
                    free_entry(&e);
                    return -1;
                }
                reset_entry(&e);
            }
            e.has_id = 1;
            sb_reset(&e.id);
            cur = &e.id;
            if (parse_quoted(p + 5, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (strncmp(p, "msgstr[", 7u) == 0) {
            char *end = NULL;
            unsigned long idx = strtoul(p + 7, &end, 10);
            if (!end || *end != ']' || idx >= (unsigned long)MAX_PLURALS) {
                free_entry(&e);
                return -1;
            }
            e.has_str = 1;
            if ((int)idx + 1 > e.plural_count) {
                e.plural_count = (int)idx + 1;
            }
            sb_reset(&e.str[idx]);
            cur = &e.str[idx];
            if (parse_quoted(end + 1, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (strncmp(p, "msgstr", 6u) == 0 &&
                   isspace((unsigned char)p[6])) {
            e.has_str = 1;
            if (e.plural_count < 1) {
                e.plural_count = 1;
            }
            sb_reset(&e.str[0]);
            cur = &e.str[0];
            if (parse_quoted(p + 6, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (*p == '"' && cur != NULL) {
            if (parse_quoted(p, cur) != 0) {
                free_entry(&e);
                return -1;
            }
        } else if (strncmp(p, "domain", 6u) == 0 &&
                   isspace((unsigned char)p[6])) {
            continue;
        }
    }
    int rc = flush_entry(&e, tbl);
    free_entry(&e);
    return rc;
}

static int cmp_mo_pair(const void *a, const void *b) {
    const struct mo_pair *pa = (const struct mo_pair *)a;
    const struct mo_pair *pb = (const struct mo_pair *)b;
    size_t ml = (pa->orig_len < pb->orig_len) ? pa->orig_len : pb->orig_len;
    int c = memcmp(pa->orig, pb->orig, ml);
    if (c != 0) {
        return c;
    }
    if (pa->orig_len < pb->orig_len) {
        return -1;
    }
    if (pa->orig_len > pb->orig_len) {
        return 1;
    }
    return 0;
}

static int write_u32(FILE *fp, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xFFu),
        (uint8_t)((v >> 8) & 0xFFu),
        (uint8_t)((v >> 16) & 0xFFu),
        (uint8_t)((v >> 24) & 0xFFu),
    };
    return fwrite(b, 1u, 4u, fp) == 4u ? 0 : -1;
}

static int write_mo(const char *path, struct mo_table *tbl) {
    if (tbl->len > 1u) {
        qsort(tbl->items, tbl->len, sizeof(tbl->items[0]), cmp_mo_pair);
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return -1;
    }
    uint32_t n = (uint32_t)tbl->len;
    uint32_t orig_off = 28u;
    uint32_t trans_off = orig_off + n * 8u;
    uint32_t str_off = trans_off + n * 8u;

    if (write_u32(fp, MO_MAGIC_LE) != 0 || write_u32(fp, 0u) != 0 ||
        write_u32(fp, n) != 0 || write_u32(fp, orig_off) != 0 ||
        write_u32(fp, trans_off) != 0 || write_u32(fp, 0u) != 0 ||
        write_u32(fp, 0u) != 0) {
        (void)fclose(fp);
        return -1;
    }

    uint32_t cur = str_off;
    for (uint32_t i = 0u; i < n; i++) {
        uint32_t ol = (uint32_t)tbl->items[i].orig_len;
        if (write_u32(fp, ol) != 0 || write_u32(fp, cur) != 0) {
            (void)fclose(fp);
            return -1;
        }
        cur += ol + 1u;
    }
    for (uint32_t i = 0u; i < n; i++) {
        uint32_t tl = (uint32_t)tbl->items[i].trans_len;
        if (write_u32(fp, tl) != 0 || write_u32(fp, cur) != 0) {
            (void)fclose(fp);
            return -1;
        }
        cur += tl + 1u;
    }
    for (uint32_t i = 0u; i < n; i++) {
        size_t ol = tbl->items[i].orig_len + 1u;
        if (fwrite(tbl->items[i].orig, 1u, ol, fp) != ol) {
            (void)fclose(fp);
            return -1;
        }
    }
    for (uint32_t i = 0u; i < n; i++) {
        size_t tl = tbl->items[i].trans_len + 1u;
        if (fwrite(tbl->items[i].trans, 1u, tl, fp) != tl) {
            (void)fclose(fp);
            return -1;
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    const char *out_path = "messages.mo";
    int verbose = 0;
    int idx = 1;

    while (idx < argc && argv[idx][0] == '-' && argv[idx][1] != '\0') {
        if (strcmp(argv[idx], "--") == 0) {
            idx++;
            break;
        }
        if (strcmp(argv[idx], "-o") == 0 && idx + 1 < argc) {
            out_path = argv[idx + 1];
            idx += 2;
        } else if (strcmp(argv[idx], "-D") == 0 && idx + 1 < argc) {
            idx += 2;
        } else if (strcmp(argv[idx], "-c") == 0 ||
                   strcmp(argv[idx], "-f") == 0) {
            idx++;
        } else if (strcmp(argv[idx], "-v") == 0) {
            verbose = 1;
            idx++;
        } else {
            fprintf(stderr, "msgfmt: unknown option '%s'\n", argv[idx]);
            return 1;
        }
    }
    if (idx >= argc) {
        fprintf(stderr, "usage: msgfmt [-c] [-v] [-o output.mo] file.po...\n");
        return 1;
    }

    struct mo_table tbl = {0};
    int rc = 0;
    for (; idx < argc; idx++) {
        FILE *fp = stdin;
        if (strcmp(argv[idx], "-") != 0) {
            fp = fopen(argv[idx], "r");
            if (!fp) {
                perror(argv[idx]);
                rc = 1;
                break;
            }
        }
        if (parse_po_file(fp, &tbl) != 0) {
            fprintf(stderr, "msgfmt: syntax error in '%s'\n", argv[idx]);
            rc = 1;
        }
        if (fp != stdin) {
            (void)fclose(fp);
        }
        if (rc != 0) {
            break;
        }
    }
    if (rc == 0 && write_mo(out_path, &tbl) != 0) {
        rc = 1;
    }
    if (rc == 0 && verbose) {
        fprintf(stderr, "%zu translated messages.\n", tbl.len);
    }
    for (size_t i = 0u; i < tbl.len; i++) {
        free(tbl.items[i].orig);
        free(tbl.items[i].trans);
    }
    free(tbl.items);
    return rc;
}
