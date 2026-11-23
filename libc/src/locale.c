/**
 * @file     locale.c
 * @brief    POSIX.1-2024 C and C.UTF-8 locale state and per-thread locale APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "locale_internal.h"
#include "tls_internal.h"

/* need:: COMP_LIBC_0012 */

static pthread_mutex_t g_locale_lock = PTHREAD_MUTEX_INITIALIZER;
static struct __microkernel_locale g_global_locale;

static const char *const k_cat_env[LOC_CAT_COUNT] = {
    "LC_COLLATE",  "LC_CTYPE",   "LC_MESSAGES",
    "LC_MONETARY", "LC_NUMERIC", "LC_TIME",
};

static char g_c_dot[] = ".";
static char g_c_empty[] = "";

static struct lconv g_c_lconv = {
    .decimal_point = g_c_dot,
    .thousands_sep = g_c_empty,
    .grouping = g_c_empty,
    .int_curr_symbol = g_c_empty,
    .currency_symbol = g_c_empty,
    .mon_decimal_point = g_c_empty,
    .mon_thousands_sep = g_c_empty,
    .mon_grouping = g_c_empty,
    .positive_sign = g_c_empty,
    .negative_sign = g_c_empty,
    .int_frac_digits = CHAR_MAX,
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX,
};

static int cat_to_index(int category) {
    if (category >= LC_COLLATE && category <= LC_TIME) {
        return category - LC_COLLATE;
    }
    return -1;
}

static const char *loc_id_name(uint8_t id) {
    return (id == LOC_ID_UTF8) ? "C.UTF-8" : "C";
}

static const char *loc_cat_name(const struct __microkernel_locale *loc,
                                int idx) {
    if (loc->name[idx][0] != '\0') {
        return loc->name[idx];
    }
    return loc_id_name(loc->cat[idx]);
}

static int parse_locale_name(const char *name, uint8_t *out_id,
                             char out_name[LOC_NAME_MAX]) {
    if (!name) {
        return -1;
    }
    if (strcmp(name, "C") == 0 || strcmp(name, "POSIX") == 0) {
        *out_id = LOC_ID_C;
        (void)strlcpy(out_name, "C", LOC_NAME_MAX);
        return 0;
    }
    if (strcmp(name, "C.UTF-8") == 0 || strcmp(name, "C.utf8") == 0 ||
        strcmp(name, "UTF-8") == 0 || strcmp(name, "utf8") == 0) {
        *out_id = LOC_ID_UTF8;
        (void)strlcpy(out_name, "C.UTF-8", LOC_NAME_MAX);
        return 0;
    }
    const char *dot = strchr(name, '.');
    if (dot && (strcmp(dot, ".UTF-8") == 0 || strcmp(dot, ".utf8") == 0)) {
        *out_id = LOC_ID_UTF8;
        (void)strlcpy(out_name, name, LOC_NAME_MAX);
        return 0;
    }
    return -1;
}

static int resolve_env_locale(int cat_idx, uint8_t *out_id,
                              char out_name[LOC_NAME_MAX]) {
    const char *v = getenv("LC_ALL");
    if (!v || !*v) {
        v = getenv(k_cat_env[cat_idx]);
    }
    if (!v || !*v) {
        v = getenv("LANG");
    }
    if (!v || !*v) {
        *out_id = LOC_ID_C;
        (void)strlcpy(out_name, "C", LOC_NAME_MAX);
        return 0;
    }
    return parse_locale_name(v, out_id, out_name);
}

static int resolve_category_id(const char *locale, int cat_idx, uint8_t *out_id,
                               char out_name[LOC_NAME_MAX]) {
    if (locale[0] == '\0') {
        return resolve_env_locale(cat_idx, out_id, out_name);
    }
    return parse_locale_name(locale, out_id, out_name);
}

int __libc_locale_is_utf8(locale_t loc) {
    if (loc == (locale_t)0) {
        __pthread_tls_t *tls = __libc_tls_self();
        if (tls && tls->locale) {
            const struct __microkernel_locale *tl =
                (const struct __microkernel_locale *)tls->locale;
            return tl->cat[LC_CTYPE - LC_COLLATE] == LOC_ID_UTF8;
        }
        return __atomic_load_n(&g_global_locale.cat[LC_CTYPE - LC_COLLATE],
                               __ATOMIC_RELAXED) == LOC_ID_UTF8;
    }
    if (loc == LC_GLOBAL_LOCALE) {
        return __atomic_load_n(&g_global_locale.cat[LC_CTYPE - LC_COLLATE],
                               __ATOMIC_RELAXED) == LOC_ID_UTF8;
    }
    return loc->cat[LC_CTYPE - LC_COLLATE] == LOC_ID_UTF8;
}

size_t __libc_mb_cur_max(void) {
    return __libc_locale_is_utf8((locale_t)0) ? 4u : 1u;
}

static char *format_global_all(void) {
    static char g_all_buf[256];
    const char *first = loc_cat_name(&g_global_locale, 0);
    int same = 1;
    for (int i = 1; i < (int)LOC_CAT_COUNT; i++) {
        if (strcmp(loc_cat_name(&g_global_locale, i), first) != 0) {
            same = 0;
            break;
        }
    }
    if (same) {
        return (char *)first;
    }
    (void)snprintf(
        g_all_buf, sizeof(g_all_buf), "%s;%s;%s;%s;%s;%s",
        loc_cat_name(&g_global_locale, 0), loc_cat_name(&g_global_locale, 1),
        loc_cat_name(&g_global_locale, 2), loc_cat_name(&g_global_locale, 3),
        loc_cat_name(&g_global_locale, 4), loc_cat_name(&g_global_locale, 5));
    return g_all_buf;
}

char *setlocale(int category, const char *locale) {
    if (category != LC_ALL && cat_to_index(category) < 0) {
        errno = EINVAL;
        return NULL;
    }
    (void)pthread_mutex_lock(&g_locale_lock);
    if (!locale) {
        char *res = (category == LC_ALL)
                        ? format_global_all()
                        : (char *)loc_cat_name(&g_global_locale,
                                               cat_to_index(category));
        (void)pthread_mutex_unlock(&g_locale_lock);
        return res;
    }
    if (category == LC_ALL) {
        uint8_t next[LOC_CAT_COUNT];
        char next_names[LOC_CAT_COUNT][LOC_NAME_MAX];
        for (int i = 0; i < (int)LOC_CAT_COUNT; i++) {
            if (resolve_category_id(locale, i, &next[i], next_names[i]) != 0) {
                (void)pthread_mutex_unlock(&g_locale_lock);
                return NULL;
            }
        }
        for (size_t i = 0u; i < LOC_CAT_COUNT; i++) {
            __atomic_store_n(&g_global_locale.cat[i], next[i],
                             __ATOMIC_RELAXED);
            memcpy(g_global_locale.name[i], next_names[i], LOC_NAME_MAX);
        }
        char *res = format_global_all();
        (void)pthread_mutex_unlock(&g_locale_lock);
        return res;
    }
    int idx = cat_to_index(category);
    uint8_t id = LOC_ID_C;
    char nm[LOC_NAME_MAX];
    if (resolve_category_id(locale, idx, &id, nm) != 0) {
        (void)pthread_mutex_unlock(&g_locale_lock);
        return NULL;
    }
    __atomic_store_n(&g_global_locale.cat[idx], id, __ATOMIC_RELAXED);
    memcpy(g_global_locale.name[idx], nm, LOC_NAME_MAX);
    char *res = (char *)loc_cat_name(&g_global_locale, idx);
    (void)pthread_mutex_unlock(&g_locale_lock);
    return res;
}

struct lconv *localeconv(void) {
    return &g_c_lconv;
}

locale_t newlocale(int category_mask, const char *locale, locale_t base) {
    if ((category_mask & ~LC_ALL_MASK) != 0 || !locale ||
        base == LC_GLOBAL_LOCALE) {
        errno = EINVAL;
        return (locale_t)0;
    }
    uint8_t resolved[LOC_CAT_COUNT] = {0};
    char res_names[LOC_CAT_COUNT][LOC_NAME_MAX];
    memset(res_names, 0, sizeof(res_names));
    for (int i = 0; i < (int)LOC_CAT_COUNT; i++) {
        if (category_mask & (1 << i)) {
            if (resolve_category_id(locale, i, &resolved[i], res_names[i]) !=
                0) {
                errno = ENOENT;
                return (locale_t)0;
            }
        }
    }
    struct __microkernel_locale *loc = base;
    if (!loc) {
        loc = (struct __microkernel_locale *)calloc(1u, sizeof(*loc));
        if (!loc) {
            errno = ENOMEM;
            return (locale_t)0;
        }
    }
    for (int i = 0; i < (int)LOC_CAT_COUNT; i++) {
        if (category_mask & (1 << i)) {
            loc->cat[i] = resolved[i];
            memcpy(loc->name[i], res_names[i], LOC_NAME_MAX);
        }
    }
    return loc;
}

locale_t duplocale(locale_t locobj) {
    if (locobj == (locale_t)0) {
        errno = EINVAL;
        return (locale_t)0;
    }
    struct __microkernel_locale *copy =
        (struct __microkernel_locale *)malloc(sizeof(*copy));
    if (!copy) {
        errno = ENOMEM;
        return (locale_t)0;
    }
    if (locobj == LC_GLOBAL_LOCALE) {
        (void)pthread_mutex_lock(&g_locale_lock);
        memcpy(copy, &g_global_locale, sizeof(*copy));
        (void)pthread_mutex_unlock(&g_locale_lock);
    } else {
        memcpy(copy, locobj, sizeof(*copy));
    }
    return copy;
}

void freelocale(locale_t locobj) {
    if (locobj && locobj != LC_GLOBAL_LOCALE) {
        free(locobj);
    }
}

locale_t uselocale(locale_t newloc) {
    __pthread_tls_t *tls = __libc_tls_self();
    if (!tls) {
        return LC_GLOBAL_LOCALE;
    }
    locale_t prev = tls->locale ? (locale_t)tls->locale : LC_GLOBAL_LOCALE;
    if (newloc == LC_GLOBAL_LOCALE) {
        tls->locale = NULL;
    } else if (newloc != (locale_t)0) {
        tls->locale = newloc;
    }
    return prev;
}

const char *getlocalename_l(int category, locale_t locobj) {
    int idx = cat_to_index(category);
    if (idx < 0 || locobj == (locale_t)0) {
        errno = EINVAL;
        return NULL;
    }
    if (locobj == LC_GLOBAL_LOCALE) {
        (void)pthread_mutex_lock(&g_locale_lock);
        const char *res = loc_cat_name(&g_global_locale, idx);
        (void)pthread_mutex_unlock(&g_locale_lock);
        return res;
    }
    return loc_cat_name(locobj, idx);
}
