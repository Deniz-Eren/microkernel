/**
 * @file     gencat.c
 * @brief    Ring 3 gencat utility targeting POSIX.1-2024 XCU
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

#define MKCAT_MAGIC "MKCAT01"
#define MKCAT_HDR_SIZE 12u

struct mkcat_disk_entry {
    uint32_t set_id;
    uint32_t msg_id;
    uint32_t str_off;
    uint32_t str_len;
};

struct cat_item {
    uint32_t set_id;
    uint32_t msg_id;
    char *text;
};

struct cat_list {
    struct cat_item *items;
    size_t len;
    size_t cap;
};

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int cat_set_msg(struct cat_list *list, uint32_t set_id, uint32_t msg_id,
                       const char *text) {
    for (size_t i = 0u; i < list->len; i++) {
        if (list->items[i].set_id == set_id &&
            list->items[i].msg_id == msg_id) {
            char *dup = strdup(text);
            if (!dup) {
                return -1;
            }
            free(list->items[i].text);
            list->items[i].text = dup;
            return 0;
        }
    }
    if (list->len == list->cap) {
        size_t ncap = list->cap ? list->cap * 2u : 32u;
        struct cat_item *ni =
            (struct cat_item *)reallocarray(list->items, ncap, sizeof(*ni));
        if (!ni) {
            return -1;
        }
        list->items = ni;
        list->cap = ncap;
    }
    char *dup = strdup(text);
    if (!dup) {
        return -1;
    }
    list->items[list->len].set_id = set_id;
    list->items[list->len].msg_id = msg_id;
    list->items[list->len].text = dup;
    list->len++;
    return 0;
}

static void cat_del_msg(struct cat_list *list, uint32_t set_id,
                        uint32_t msg_id) {
    for (size_t i = 0u; i < list->len; i++) {
        if (list->items[i].set_id == set_id &&
            list->items[i].msg_id == msg_id) {
            free(list->items[i].text);
            list->items[i] = list->items[--list->len];
            return;
        }
    }
}

static void cat_del_set(struct cat_list *list, uint32_t set_id) {
    size_t i = 0u;
    while (i < list->len) {
        if (list->items[i].set_id == set_id) {
            free(list->items[i].text);
            list->items[i] = list->items[--list->len];
        } else {
            i++;
        }
    }
}

static void load_existing(const char *path, struct cat_list *list) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        (void)fclose(fp);
        return;
    }
    long sz = ftell(fp);
    if (sz < (long)MKCAT_HDR_SIZE || sz > 4 * 1024 * 1024) {
        (void)fclose(fp);
        return;
    }
    (void)fseek(fp, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        (void)fclose(fp);
        return;
    }
    if (fread(buf, 1u, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        (void)fclose(fp);
        return;
    }
    (void)fclose(fp);
    if (memcmp(buf, MKCAT_MAGIC, 8u) != 0) {
        free(buf);
        return;
    }
    uint32_t count = read_u32_le(buf + 8u);
    size_t tbytes = (size_t)count * sizeof(struct mkcat_disk_entry);
    if (MKCAT_HDR_SIZE + tbytes > (size_t)sz) {
        free(buf);
        return;
    }
    const struct mkcat_disk_entry *ents =
        (const struct mkcat_disk_entry *)(buf + MKCAT_HDR_SIZE);
    const char *strs = (const char *)(buf + MKCAT_HDR_SIZE + tbytes);
    size_t slen = (size_t)sz - (MKCAT_HDR_SIZE + tbytes);
    for (uint32_t i = 0u; i < count; i++) {
        if ((size_t)ents[i].str_off + (size_t)ents[i].str_len < slen) {
            (void)cat_set_msg(list, ents[i].set_id, ents[i].msg_id,
                              strs + ents[i].str_off);
        }
    }
    free(buf);
}

static void unescape_msg(const char *src, char quote_ch, char *dst,
                         size_t dst_sz) {
    if (quote_ch != '\0' && *src == quote_ch) {
        src++;
    }
    size_t out = 0u;
    while (*src != '\0' && out + 1u < dst_sz) {
        if (quote_ch != '\0' && *src == quote_ch) {
            break;
        }
        if (*src == '\\' && src[1] != '\0') {
            src++;
            if (*src == 'n') {
                dst[out++] = '\n';
            } else if (*src == 't') {
                dst[out++] = '\t';
            } else if (*src == 'v') {
                dst[out++] = '\v';
            } else if (*src == 'b') {
                dst[out++] = '\b';
            } else if (*src == 'r') {
                dst[out++] = '\r';
            } else if (*src == 'f') {
                dst[out++] = '\f';
            } else if (*src >= '0' && *src <= '7') {
                unsigned v = (unsigned)(*src - '0');
                for (int k = 0; k < 2 && src[1] >= '0' && src[1] <= '7'; k++) {
                    src++;
                    v = v * 8u + (unsigned)(*src - '0');
                }
                dst[out++] = (char)(v & 0xFFu);
            } else {
                dst[out++] = *src;
            }
            src++;
        } else {
            dst[out++] = *src++;
        }
    }
    dst[out] = '\0';
}

static int parse_msgfile(FILE *fp, struct cat_list *list) {
    uint32_t cur_set = 1u;
    char quote_ch = '\0';
    char line[1024];
    char combined[2048];

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
            line[--len] = '\0';
        }
        (void)strlcpy(combined, line, sizeof(combined));
        while (len > 0u && combined[len - 1u] == '\\') {
            combined[--len] = '\0';
            if (!fgets(line, (int)sizeof(line), fp)) {
                break;
            }
            size_t nlen = strlen(line);
            while (nlen > 0u &&
                   (line[nlen - 1u] == '\n' || line[nlen - 1u] == '\r')) {
                line[--nlen] = '\0';
            }
            len = strlcat(combined, line, sizeof(combined));
            if (len >= sizeof(combined)) {
                len = sizeof(combined) - 1u;
            }
        }
        char *p = combined;
        if (*p == '\0') {
            continue;
        }
        if (*p == '$') {
            if (strncmp(p, "$set", 4u) == 0 && (p[4] == ' ' || p[4] == '\t')) {
                cur_set = (uint32_t)strtoul(p + 4, NULL, 10);
            } else if (strncmp(p, "$delset", 7u) == 0 &&
                       (p[7] == ' ' || p[7] == '\t')) {
                uint32_t ds = (uint32_t)strtoul(p + 7, NULL, 10);
                cat_del_set(list, ds);
            } else if (strncmp(p, "$quote", 6u) == 0) {
                char *q = p + 6;
                while (*q == ' ' || *q == '\t') {
                    q++;
                }
                quote_ch = *q;
            }
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            char *end = NULL;
            uint32_t mid = (uint32_t)strtoul(p, &end, 10);
            if (*end == '\0') {
                cat_del_msg(list, cur_set, mid);
            } else if (*end == ' ' || *end == '\t') {
                end++;
                char msg[2048];
                unescape_msg(end, quote_ch, msg, sizeof(msg));
                if (cat_set_msg(list, cur_set, mid, msg) != 0) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

static int cmp_cat_item(const void *a, const void *b) {
    const struct cat_item *ia = (const struct cat_item *)a;
    const struct cat_item *ib = (const struct cat_item *)b;
    if (ia->set_id != ib->set_id) {
        return (ia->set_id < ib->set_id) ? -1 : 1;
    }
    if (ia->msg_id != ib->msg_id) {
        return (ia->msg_id < ib->msg_id) ? -1 : 1;
    }
    return 0;
}

static int write_catalog(const char *path, struct cat_list *list) {
    if (list->len > 1u) {
        qsort(list->items, list->len, sizeof(list->items[0]), cmp_cat_item);
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return -1;
    }
    uint8_t hdr[MKCAT_HDR_SIZE];
    memcpy(hdr, MKCAT_MAGIC, 8u);
    write_u32_le(hdr + 8u, (uint32_t)list->len);
    if (fwrite(hdr, 1u, sizeof(hdr), fp) != sizeof(hdr)) {
        (void)fclose(fp);
        return -1;
    }
    uint32_t off = 0u;
    for (size_t i = 0u; i < list->len; i++) {
        uint32_t sl = (uint32_t)strlen(list->items[i].text);
        struct mkcat_disk_entry e;
        e.set_id = list->items[i].set_id;
        e.msg_id = list->items[i].msg_id;
        e.str_off = off;
        e.str_len = sl;
        if (fwrite(&e, sizeof(e), 1u, fp) != 1u) {
            (void)fclose(fp);
            return -1;
        }
        off += sl + 1u;
    }
    for (size_t i = 0u; i < list->len; i++) {
        size_t sl = strlen(list->items[i].text) + 1u;
        if (fwrite(list->items[i].text, 1u, sl, fp) != sl) {
            (void)fclose(fp);
            return -1;
        }
    }
    return fclose(fp) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: gencat catfile msgfile...\n");
        return 1;
    }
    const char *catfile = argv[1];
    struct cat_list list = {0};
    load_existing(catfile, &list);

    int rc = 0;
    for (int i = 2; i < argc; i++) {
        FILE *fp = stdin;
        if (strcmp(argv[i], "-") != 0) {
            fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                rc = 1;
                break;
            }
        }
        if (parse_msgfile(fp, &list) != 0) {
            rc = 1;
        }
        if (fp != stdin) {
            (void)fclose(fp);
        }
        if (rc != 0) {
            break;
        }
    }
    if (rc == 0 && write_catalog(catfile, &list) != 0) {
        rc = 1;
    }
    for (size_t i = 0u; i < list.len; i++) {
        free(list.items[i].text);
    }
    free(list.items);
    return rc;
}
