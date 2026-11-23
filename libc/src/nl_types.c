/**
 * @file     nl_types.c
 * @brief    POSIX.1-2024 message catalog APIs (catopen, catgets, catclose)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0015 */

#define MKCAT_MAGIC "MKCAT01"
#define MKCAT_HDR_SIZE 12u

struct mkcat_entry {
    uint32_t set_id;
    uint32_t msg_id;
    uint32_t str_off;
    uint32_t str_len;
};

struct __microkernel_cat {
    uint8_t *buf;
    size_t size;
    uint32_t count;
    const struct mkcat_entry *entries;
    const char *strings;
    size_t strings_len;
};

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static struct __microkernel_cat *load_cat_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)MKCAT_HDR_SIZE ||
        st.st_size > (off_t)(4 * 1024 * 1024)) {
        (void)close(fd);
        errno = EINVAL;
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        (void)close(fd);
        errno = ENOMEM;
        return NULL;
    }
    size_t got = 0u;
    while (got < sz) {
        ssize_t r = read(fd, buf + got, sz - got);
        if (r <= 0) {
            free(buf);
            (void)close(fd);
            errno = EIO;
            return NULL;
        }
        got += (size_t)r;
    }
    (void)close(fd);

    if (memcmp(buf, MKCAT_MAGIC, 8u) != 0) {
        free(buf);
        errno = EINVAL;
        return NULL;
    }
    uint32_t count = read_u32_le(buf + 8u);
    size_t table_bytes = (size_t)count * sizeof(struct mkcat_entry);
    if (count > 65536u || MKCAT_HDR_SIZE + table_bytes > sz) {
        free(buf);
        errno = EINVAL;
        return NULL;
    }
    struct __microkernel_cat *cat =
        (struct __microkernel_cat *)calloc(1u, sizeof(*cat));
    if (!cat) {
        free(buf);
        errno = ENOMEM;
        return NULL;
    }
    cat->buf = buf;
    cat->size = sz;
    cat->count = count;
    cat->entries = (const struct mkcat_entry *)(buf + MKCAT_HDR_SIZE);
    cat->strings = (const char *)(buf + MKCAT_HDR_SIZE + table_bytes);
    cat->strings_len = sz - (MKCAT_HDR_SIZE + table_bytes);
    return cat;
}

static void expand_nlspath_entry(const char *tmpl, size_t tlen,
                                 const char *name, const char *lang, char *out,
                                 size_t out_sz) {
    char lang_l[32] = {0};
    char lang_t[32] = {0};
    char lang_c[32] = {0};
    if (lang && lang[0] != '\0') {
        const char *us = strchr(lang, '_');
        const char *dot = strchr(lang, '.');
        size_t ll = us ? (size_t)(us - lang)
                       : (dot ? (size_t)(dot - lang) : strlen(lang));
        if (ll >= sizeof(lang_l)) {
            ll = sizeof(lang_l) - 1u;
        }
        memcpy(lang_l, lang, ll);
        if (us) {
            const char *ts = us + 1;
            size_t tl = dot ? (size_t)(dot - ts) : strlen(ts);
            if (tl >= sizeof(lang_t)) {
                tl = sizeof(lang_t) - 1u;
            }
            memcpy(lang_t, ts, tl);
        }
        if (dot) {
            (void)strlcpy(lang_c, dot + 1, sizeof(lang_c));
        }
    }

    size_t pos = 0u;
    for (size_t i = 0u; i < tlen && pos + 1u < out_sz; i++) {
        if (tmpl[i] == '%' && i + 1u < tlen) {
            const char *rep = "";
            char c = tmpl[++i];
            if (c == 'N') {
                rep = name;
            } else if (c == 'L') {
                rep = lang ? lang : "";
            } else if (c == 'l') {
                rep = lang_l;
            } else if (c == 't') {
                rep = lang_t;
            } else if (c == 'c') {
                rep = lang_c;
            } else if (c == '%') {
                rep = "%";
            }
            size_t rlen = strlen(rep);
            if (pos + rlen >= out_sz) {
                rlen = out_sz - 1u - pos;
            }
            memcpy(out + pos, rep, rlen);
            pos += rlen;
        } else {
            out[pos++] = tmpl[i];
        }
    }
    out[pos] = '\0';
}

nl_catd catopen(const char *name, int oflag) {
    if (!name || name[0] == '\0') {
        errno = ENOENT;
        return (nl_catd)-1;
    }
    if (strchr(name, '/') != NULL) {
        struct __microkernel_cat *cat = load_cat_file(name);
        return cat ? (nl_catd)cat : (nl_catd)-1;
    }
    const char *lang = NULL;
    if (oflag == NL_CAT_LOCALE) {
        lang = setlocale(LC_MESSAGES, NULL);
    } else {
        lang = getenv("LANG");
    }
    if (!lang || lang[0] == '\0') {
        lang = "C";
    }
    const char *nlspath = getenv("NLSPATH");
    if (nlspath && nlspath[0] != '\0') {
        const char *p = nlspath;
        while (*p != '\0') {
            const char *colon = strchr(p, ':');
            size_t seg = colon ? (size_t)(colon - p) : strlen(p);
            char cand[PATH_MAX];
            if (seg == 0u) {
                (void)strlcpy(cand, name, sizeof(cand));
            } else {
                expand_nlspath_entry(p, seg, name, lang, cand, sizeof(cand));
            }
            struct __microkernel_cat *cat = load_cat_file(cand);
            if (cat) {
                return (nl_catd)cat;
            }
            if (!colon) {
                break;
            }
            p = colon + 1;
        }
    }
    struct __microkernel_cat *cat = load_cat_file(name);
    if (cat) {
        return (nl_catd)cat;
    }
    errno = ENOENT;
    return (nl_catd)-1;
}

char *catgets(nl_catd catd, int set_id, int msg_id, const char *s) {
    if (catd == (nl_catd)-1 || catd == NULL) {
        errno = EBADF;
        return (char *)s;
    }
    if (set_id <= 0 || msg_id <= 0) {
        errno = ENOMSG;
        return (char *)s;
    }
    const struct __microkernel_cat *cat =
        (const struct __microkernel_cat *)catd;
    uint32_t want_set = (uint32_t)set_id;
    uint32_t want_msg = (uint32_t)msg_id;
    uint32_t lo = 0u;
    uint32_t hi = cat->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        const struct mkcat_entry *e = &cat->entries[mid];
        if (e->set_id < want_set ||
            (e->set_id == want_set && e->msg_id < want_msg)) {
            lo = mid + 1u;
        } else if (e->set_id > want_set || e->msg_id > want_msg) {
            hi = mid;
        } else {
            if ((size_t)e->str_off + (size_t)e->str_len < cat->strings_len) {
                return (char *)(cat->strings + e->str_off);
            }
            errno = EINVAL;
            return (char *)s;
        }
    }
    errno = ENOMSG;
    return (char *)s;
}

int catclose(nl_catd catd) {
    if (catd == (nl_catd)-1 || catd == NULL) {
        errno = EBADF;
        return -1;
    }
    struct __microkernel_cat *cat = (struct __microkernel_cat *)catd;
    free(cat->buf);
    free(cat);
    return 0;
}
