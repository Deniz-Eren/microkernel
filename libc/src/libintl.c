/**
 * @file     libintl.c
 * @brief    POSIX.1-2024 gettext/ngettext message catalog implementation
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0015 */

#define MO_MAGIC_LE 0x950412DEu
#define MO_MAGIC_BE 0xDE120495u

struct domain_binding {
    char *domain;
    char *dirname;
    char *codeset;
    struct domain_binding *next;
};

struct mo_catalog {
    char *path;
    uint8_t *data;
    size_t size;
    int swapped;
    uint32_t nstrings;
    uint32_t orig_off;
    uint32_t trans_off;
    unsigned long nplurals;
    char *plural_expr;
    char *src_charset;
    struct mo_catalog *next;
};

struct conv_entry {
    const struct mo_catalog *cat;
    const char *src;
    char *codeset;
    char *converted;
    struct conv_entry *next;
};

static pthread_mutex_t g_intl_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_current_domain[128] = "messages";
static struct domain_binding *g_bindings = NULL;
static struct mo_catalog *g_catalogs = NULL;
static struct conv_entry *g_convs = NULL;

static uint32_t mo_u32(const struct mo_catalog *mo, uint32_t off) {
    const uint8_t *p = mo->data + off;
    if (mo->swapped) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static struct domain_binding *find_binding(const char *domain, int create) {
    for (struct domain_binding *b = g_bindings; b; b = b->next) {
        if (strcmp(b->domain, domain) == 0) {
            return b;
        }
    }
    if (!create) {
        return NULL;
    }
    struct domain_binding *b = (struct domain_binding *)calloc(1u, sizeof(*b));
    if (!b) {
        return NULL;
    }
    b->domain = strdup(domain);
    b->dirname = strdup("/usr/share/locale");
    if (!b->domain || !b->dirname) {
        free(b->domain);
        free(b->dirname);
        free(b);
        return NULL;
    }
    b->next = g_bindings;
    g_bindings = b;
    return b;
}

static const char *cat_dir_name(int category) {
    switch (category) {
    case LC_COLLATE:
        return "LC_COLLATE";
    case LC_CTYPE:
        return "LC_CTYPE";
    case LC_MONETARY:
        return "LC_MONETARY";
    case LC_NUMERIC:
        return "LC_NUMERIC";
    case LC_TIME:
        return "LC_TIME";
    case LC_MESSAGES:
    default:
        return "LC_MESSAGES";
    }
}

/* Recursive-descent evaluator for standard gettext Plural-Forms expressions. */
struct pe_ctx {
    const char *p;
    unsigned long n;
};

static unsigned long pe_ternary(struct pe_ctx *c);

static void pe_ws(struct pe_ctx *c) {
    while (*c->p != '\0' && isspace((unsigned char)*c->p)) {
        c->p++;
    }
}

static unsigned long pe_atom(struct pe_ctx *c) {
    pe_ws(c);
    if (*c->p == '(') {
        c->p++;
        unsigned long v = pe_ternary(c);
        pe_ws(c);
        if (*c->p == ')') {
            c->p++;
        }
        return v;
    }
    if (*c->p == '!') {
        c->p++;
        return !pe_atom(c);
    }
    if (*c->p == 'n') {
        c->p++;
        return c->n;
    }
    unsigned long v = 0ul;
    while (isdigit((unsigned char)*c->p)) {
        v = v * 10ul + (unsigned long)(*c->p - '0');
        c->p++;
    }
    return v;
}

static unsigned long pe_mul(struct pe_ctx *c) {
    unsigned long v = pe_atom(c);
    for (;;) {
        pe_ws(c);
        char op = *c->p;
        if (op != '*' && op != '/' && op != '%') {
            break;
        }
        c->p++;
        unsigned long r = pe_atom(c);
        if (op == '*') {
            v *= r;
        } else if (r != 0ul) {
            v = (op == '/') ? (v / r) : (v % r);
        }
    }
    return v;
}

static unsigned long pe_add(struct pe_ctx *c) {
    unsigned long v = pe_mul(c);
    for (;;) {
        pe_ws(c);
        char op = *c->p;
        if (op != '+' && op != '-') {
            break;
        }
        c->p++;
        unsigned long r = pe_mul(c);
        v = (op == '+') ? (v + r) : (v - r);
    }
    return v;
}

static unsigned long pe_rel(struct pe_ctx *c) {
    unsigned long v = pe_add(c);
    for (;;) {
        pe_ws(c);
        if (c->p[0] == '<' && c->p[1] == '=') {
            c->p += 2;
            v = (v <= pe_add(c));
        } else if (c->p[0] == '>' && c->p[1] == '=') {
            c->p += 2;
            v = (v >= pe_add(c));
        } else if (c->p[0] == '<') {
            c->p++;
            v = (v < pe_add(c));
        } else if (c->p[0] == '>') {
            c->p++;
            v = (v > pe_add(c));
        } else {
            break;
        }
    }
    return v;
}

static unsigned long pe_eq(struct pe_ctx *c) {
    unsigned long v = pe_rel(c);
    for (;;) {
        pe_ws(c);
        if (c->p[0] == '=' && c->p[1] == '=') {
            c->p += 2;
            v = (v == pe_rel(c));
        } else if (c->p[0] == '!' && c->p[1] == '=') {
            c->p += 2;
            v = (v != pe_rel(c));
        } else {
            break;
        }
    }
    return v;
}

static unsigned long pe_and(struct pe_ctx *c) {
    unsigned long v = pe_eq(c);
    for (;;) {
        pe_ws(c);
        if (c->p[0] == '&' && c->p[1] == '&') {
            c->p += 2;
            unsigned long r = pe_eq(c);
            v = (v && r) ? 1ul : 0ul;
        } else {
            break;
        }
    }
    return v;
}

static unsigned long pe_or(struct pe_ctx *c) {
    unsigned long v = pe_and(c);
    for (;;) {
        pe_ws(c);
        if (c->p[0] == '|' && c->p[1] == '|') {
            c->p += 2;
            unsigned long r = pe_and(c);
            v = (v || r) ? 1ul : 0ul;
        } else {
            break;
        }
    }
    return v;
}

static unsigned long pe_ternary(struct pe_ctx *c) {
    unsigned long cond = pe_or(c);
    pe_ws(c);
    if (*c->p == '?') {
        c->p++;
        unsigned long t = pe_ternary(c);
        pe_ws(c);
        if (*c->p == ':') {
            c->p++;
        }
        unsigned long f = pe_ternary(c);
        return cond ? t : f;
    }
    return cond;
}

static unsigned long eval_plural(const struct mo_catalog *mo, unsigned long n) {
    if (!mo->plural_expr || mo->plural_expr[0] == '\0') {
        return (n == 1ul) ? 0ul : 1ul;
    }
    struct pe_ctx ctx = {.p = mo->plural_expr, .n = n};
    unsigned long idx = pe_ternary(&ctx);
    if (mo->nplurals > 0ul && idx >= mo->nplurals) {
        idx = 0ul;
    }
    return idx;
}

static const char *mo_lookup_raw(const struct mo_catalog *mo, const char *key,
                                 size_t key_len, size_t *out_len) {
    uint32_t lo = 0u;
    uint32_t hi = mo->nstrings;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        uint32_t olen = mo_u32(mo, mo->orig_off + mid * 8u);
        uint32_t ooff = mo_u32(mo, mo->orig_off + mid * 8u + 4u);
        if ((size_t)ooff + (size_t)olen >= mo->size) {
            return NULL;
        }
        const char *os = (const char *)(mo->data + ooff);
        size_t minlen = (key_len < (size_t)olen) ? key_len : (size_t)olen;
        int cmp = memcmp(os, key, minlen);
        if (cmp == 0) {
            if ((size_t)olen < key_len) {
                cmp = -1;
            } else if ((size_t)olen > key_len) {
                cmp = 1;
            }
        }
        if (cmp < 0) {
            lo = mid + 1u;
        } else if (cmp > 0) {
            hi = mid;
        } else {
            uint32_t tlen = mo_u32(mo, mo->trans_off + mid * 8u);
            uint32_t toff = mo_u32(mo, mo->trans_off + mid * 8u + 4u);
            if ((size_t)toff + (size_t)tlen >= mo->size) {
                return NULL;
            }
            if (out_len) {
                *out_len = (size_t)tlen;
            }
            return (const char *)(mo->data + toff);
        }
    }
    return NULL;
}

static void parse_mo_header(struct mo_catalog *mo) {
    size_t hlen = 0u;
    const char *hdr = mo_lookup_raw(mo, "", 0u, &hlen);
    if (!hdr || hlen == 0u) {
        return;
    }
    const char *cs = strstr(hdr, "charset=");
    if (cs) {
        cs += 8;
        const char *end = cs;
        while (*end != '\0' && *end != '\n' && *end != '\r' && *end != ';' &&
               !isspace((unsigned char)*end)) {
            end++;
        }
        if (end > cs) {
            mo->src_charset = strndup(cs, (size_t)(end - cs));
        }
    }
    const char *np = strstr(hdr, "nplurals=");
    if (np) {
        mo->nplurals = strtoul(np + 9, NULL, 10);
    }
    const char *pl = strstr(hdr, "plural=");
    if (pl) {
        pl += 7;
        const char *end = pl;
        while (*end != '\0' && *end != ';' && *end != '\n' && *end != '\r') {
            end++;
        }
        if (end > pl) {
            mo->plural_expr = strndup(pl, (size_t)(end - pl));
        }
    }
}

static struct mo_catalog *load_mo(const char *path) {
    for (struct mo_catalog *c = g_catalogs; c; c = c->next) {
        if (strcmp(c->path, path) == 0) {
            return c;
        }
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 28 ||
        st.st_size > (off_t)(4 * 1024 * 1024)) {
        (void)close(fd);
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        (void)close(fd);
        return NULL;
    }
    size_t got = 0u;
    while (got < sz) {
        ssize_t r = read(fd, buf + got, sz - got);
        if (r <= 0) {
            free(buf);
            (void)close(fd);
            return NULL;
        }
        got += (size_t)r;
    }
    (void)close(fd);

    uint32_t magic_le = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                        ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    int swapped = 0;
    if (magic_le == MO_MAGIC_LE) {
        swapped = 0;
    } else if (magic_le == MO_MAGIC_BE) {
        swapped = 1;
    } else {
        free(buf);
        return NULL;
    }
    struct mo_catalog *mo = (struct mo_catalog *)calloc(1u, sizeof(*mo));
    if (!mo) {
        free(buf);
        return NULL;
    }
    mo->path = strdup(path);
    if (!mo->path) {
        free(mo);
        free(buf);
        return NULL;
    }
    mo->data = buf;
    mo->size = sz;
    mo->swapped = swapped;
    mo->nstrings = mo_u32(mo, 8u);
    mo->orig_off = mo_u32(mo, 12u);
    mo->trans_off = mo_u32(mo, 16u);
    if (mo->nstrings > 65536u ||
        (size_t)mo->orig_off + (size_t)mo->nstrings * 8u > sz ||
        (size_t)mo->trans_off + (size_t)mo->nstrings * 8u > sz) {
        free(mo->path);
        free(buf);
        free(mo);
        return NULL;
    }
    parse_mo_header(mo);
    mo->next = g_catalogs;
    g_catalogs = mo;
    return mo;
}

static const char *convert_if_needed(const struct mo_catalog *mo,
                                     const char *str, const char *want_cs) {
    if (!want_cs || want_cs[0] == '\0') {
        return str;
    }
    const char *src_cs = mo->src_charset ? mo->src_charset : "UTF-8";
    if (strcasecmp(src_cs, want_cs) == 0) {
        return str;
    }
    for (struct conv_entry *e = g_convs; e; e = e->next) {
        if (e->cat == mo && e->src == str && strcmp(e->codeset, want_cs) == 0) {
            return e->converted;
        }
    }
    iconv_t cd = iconv_open(want_cs, src_cs);
    if (cd == (iconv_t)-1) {
        return str;
    }
    size_t inleft = strlen(str);
    size_t cap = inleft * 4u + 16u;
    char *out = (char *)malloc(cap);
    if (!out) {
        (void)iconv_close(cd);
        return str;
    }
    char *ip = (char *)str;
    char *op = out;
    size_t outleft = cap - 1u;
    if (iconv(cd, &ip, &inleft, &op, &outleft) == (size_t)-1) {
        free(out);
        (void)iconv_close(cd);
        return str;
    }
    *op = '\0';
    (void)iconv_close(cd);
    struct conv_entry *ce = (struct conv_entry *)calloc(1u, sizeof(*ce));
    if (!ce) {
        free(out);
        return str;
    }
    ce->cat = mo;
    ce->src = str;
    ce->codeset = strdup(want_cs);
    ce->converted = out;
    if (!ce->codeset) {
        free(out);
        free(ce);
        return str;
    }
    ce->next = g_convs;
    g_convs = ce;
    return ce->converted;
}

static struct mo_catalog *try_open_locale(const char *dir, const char *loc,
                                          const char *cat_name,
                                          const char *dom) {
    char path[PATH_MAX];
    (void)snprintf(path, sizeof(path), "%s/%s/%s/%s.mo", dir, loc, cat_name,
                   dom);
    struct mo_catalog *mo = load_mo(path);
    if (mo) {
        return mo;
    }
    const char *dot = strchr(loc, '.');
    if (dot && dot > loc) {
        char base[64];
        size_t bl = (size_t)(dot - loc);
        if (bl >= sizeof(base)) {
            bl = sizeof(base) - 1u;
        }
        memcpy(base, loc, bl);
        base[bl] = '\0';
        (void)snprintf(path, sizeof(path), "%s/%s/%s/%s.mo", dir, base,
                       cat_name, dom);
        mo = load_mo(path);
        if (mo) {
            return mo;
        }
        const char *us = strchr(base, '_');
        if (us && us > base) {
            base[us - base] = '\0';
            (void)snprintf(path, sizeof(path), "%s/%s/%s/%s.mo", dir, base,
                           cat_name, dom);
            mo = load_mo(path);
            if (mo) {
                return mo;
            }
        }
    }
    const char *us = strchr(loc, '_');
    if (us && us > loc) {
        char base[64];
        size_t bl = (size_t)(us - loc);
        if (bl >= sizeof(base)) {
            bl = sizeof(base) - 1u;
        }
        memcpy(base, loc, bl);
        base[bl] = '\0';
        (void)snprintf(path, sizeof(path), "%s/%s/%s/%s.mo", dir, base,
                       cat_name, dom);
        return load_mo(path);
    }
    return NULL;
}

static const char *resolve_locale_name(int category, locale_t locale) {
    if (locale == (locale_t)0) {
        locale = uselocale((locale_t)0);
    }
    const char *nm = getlocalename_l(category, locale);
    if (nm && strcmp(nm, "C") != 0 && strcmp(nm, "POSIX") != 0) {
        return nm;
    }
    return "C";
}

static struct mo_catalog *resolve_catalog(const char *domain, int category,
                                          locale_t locale,
                                          const char **out_codeset) {
    const char *loc = resolve_locale_name(category, locale);
    if (strcmp(loc, "C") == 0 || strcmp(loc, "POSIX") == 0) {
        return NULL;
    }
    struct domain_binding *b = find_binding(domain, 0);
    const char *dir = b ? b->dirname : "/usr/share/locale";
    if (out_codeset) {
        *out_codeset = b ? b->codeset : NULL;
    }
    const char *cat_name = cat_dir_name(category);
    const char *lang_list = getenv("LANGUAGE");
    if (lang_list && lang_list[0] != '\0') {
        const char *p = lang_list;
        while (*p != '\0') {
            const char *colon = strchr(p, ':');
            size_t len = colon ? (size_t)(colon - p) : strlen(p);
            if (len > 0u && len < 64u) {
                char seg[64];
                memcpy(seg, p, len);
                seg[len] = '\0';
                if (strcmp(seg, "C") == 0 || strcmp(seg, "POSIX") == 0) {
                    return NULL;
                }
                struct mo_catalog *mo =
                    try_open_locale(dir, seg, cat_name, domain);
                if (mo) {
                    return mo;
                }
            }
            if (!colon) {
                break;
            }
            p = colon + 1;
        }
    }
    return try_open_locale(dir, loc, cat_name, domain);
}

char *textdomain(const char *domainname) {
    (void)pthread_mutex_lock(&g_intl_lock);
    if (domainname) {
        const char *src = (domainname[0] == '\0') ? "messages" : domainname;
        (void)strlcpy(g_current_domain, src, sizeof(g_current_domain));
    }
    char *res = g_current_domain;
    (void)pthread_mutex_unlock(&g_intl_lock);
    return res;
}

char *bindtextdomain(const char *domainname, const char *dirname) {
    if (!domainname || domainname[0] == '\0') {
        errno = EINVAL;
        return NULL;
    }
    (void)pthread_mutex_lock(&g_intl_lock);
    struct domain_binding *b = find_binding(domainname, dirname != NULL);
    if (!b) {
        (void)pthread_mutex_unlock(&g_intl_lock);
        return dirname ? NULL : (char *)"/usr/share/locale";
    }
    if (dirname) {
        char *dup = strdup(dirname);
        if (!dup) {
            (void)pthread_mutex_unlock(&g_intl_lock);
            errno = ENOMEM;
            return NULL;
        }
        free(b->dirname);
        b->dirname = dup;
    }
    char *res = b->dirname;
    (void)pthread_mutex_unlock(&g_intl_lock);
    return res;
}

char *bind_textdomain_codeset(const char *domainname, const char *codeset) {
    if (!domainname || domainname[0] == '\0') {
        errno = EINVAL;
        return NULL;
    }
    (void)pthread_mutex_lock(&g_intl_lock);
    struct domain_binding *b = find_binding(domainname, codeset != NULL);
    if (!b) {
        (void)pthread_mutex_unlock(&g_intl_lock);
        return NULL;
    }
    if (codeset) {
        char *dup = strdup(codeset);
        if (!dup) {
            (void)pthread_mutex_unlock(&g_intl_lock);
            errno = ENOMEM;
            return NULL;
        }
        free(b->codeset);
        b->codeset = dup;
    }
    char *res = b->codeset;
    (void)pthread_mutex_unlock(&g_intl_lock);
    return res;
}

char *dcgettext_l(const char *domainname, const char *msgid, int category,
                  locale_t locale) {
    if (!msgid) {
        return NULL;
    }
    int saved = errno;
    (void)pthread_mutex_lock(&g_intl_lock);
    const char *dom =
        (domainname && domainname[0] != '\0') ? domainname : g_current_domain;
    const char *want_cs = NULL;
    struct mo_catalog *mo = resolve_catalog(dom, category, locale, &want_cs);
    const char *res = msgid;
    if (mo) {
        size_t tlen = 0u;
        const char *tr = mo_lookup_raw(mo, msgid, strlen(msgid), &tlen);
        if (tr) {
            res = convert_if_needed(mo, tr, want_cs);
        }
    }
    (void)pthread_mutex_unlock(&g_intl_lock);
    errno = saved;
    return (char *)res;
}

char *dcgettext(const char *domainname, const char *msgid, int category) {
    return dcgettext_l(domainname, msgid, category, uselocale((locale_t)0));
}

char *dgettext_l(const char *domainname, const char *msgid, locale_t locale) {
    return dcgettext_l(domainname, msgid, LC_MESSAGES, locale);
}

char *dgettext(const char *domainname, const char *msgid) {
    return dcgettext_l(domainname, msgid, LC_MESSAGES, uselocale((locale_t)0));
}

char *gettext_l(const char *msgid, locale_t locale) {
    return dcgettext_l(NULL, msgid, LC_MESSAGES, locale);
}

char *gettext(const char *msgid) {
    return dcgettext_l(NULL, msgid, LC_MESSAGES, uselocale((locale_t)0));
}

char *dcngettext_l(const char *domainname, const char *msgid1,
                   const char *msgid2, unsigned long int n, int category,
                   locale_t locale) {
    const char *fallback = (n == 1ul) ? msgid1 : msgid2;
    if (!msgid1) {
        return (char *)fallback;
    }
    int saved = errno;
    (void)pthread_mutex_lock(&g_intl_lock);
    const char *dom =
        (domainname && domainname[0] != '\0') ? domainname : g_current_domain;
    const char *want_cs = NULL;
    struct mo_catalog *mo = resolve_catalog(dom, category, locale, &want_cs);
    const char *res = fallback;
    if (mo) {
        size_t l1 = strlen(msgid1);
        size_t l2 = msgid2 ? strlen(msgid2) : 0u;
        size_t klen = l1 + (msgid2 ? (1u + l2) : 0u);
        char *key = (char *)malloc(klen + 1u);
        if (key) {
            memcpy(key, msgid1, l1);
            if (msgid2) {
                key[l1] = '\0';
                memcpy(key + l1 + 1u, msgid2, l2 + 1u);
            } else {
                key[l1] = '\0';
            }
            size_t tlen = 0u;
            const char *tr = mo_lookup_raw(mo, key, klen, &tlen);
            free(key);
            if (tr) {
                unsigned long idx = eval_plural(mo, n);
                const char *cur = tr;
                const char *end = tr + tlen;
                for (unsigned long i = 0ul; i < idx && cur < end; i++) {
                    const char *nul =
                        (const char *)memchr(cur, '\0', (size_t)(end - cur));
                    if (!nul || nul + 1 >= end) {
                        cur = tr;
                        break;
                    }
                    cur = nul + 1;
                }
                res = convert_if_needed(mo, cur, want_cs);
            }
        }
    }
    (void)pthread_mutex_unlock(&g_intl_lock);
    errno = saved;
    return (char *)res;
}

char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2,
                 unsigned long int n, int category) {
    return dcngettext_l(domainname, msgid1, msgid2, n, category,
                        uselocale((locale_t)0));
}

char *dngettext_l(const char *domainname, const char *msgid1,
                  const char *msgid2, unsigned long int n, locale_t locale) {
    return dcngettext_l(domainname, msgid1, msgid2, n, LC_MESSAGES, locale);
}

char *dngettext(const char *domainname, const char *msgid1, const char *msgid2,
                unsigned long int n) {
    return dcngettext_l(domainname, msgid1, msgid2, n, LC_MESSAGES,
                        uselocale((locale_t)0));
}

char *ngettext_l(const char *msgid1, const char *msgid2, unsigned long int n,
                 locale_t locale) {
    return dcngettext_l(NULL, msgid1, msgid2, n, LC_MESSAGES, locale);
}

char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n) {
    return dcngettext_l(NULL, msgid1, msgid2, n, LC_MESSAGES,
                        uselocale((locale_t)0));
}
