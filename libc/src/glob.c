/**
 * @file     glob.c
 * @brief    POSIX.1-2024 pathname pattern matching (glob and globfree)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* need:: COMP_LIBC_0014 */

struct glob_vec {
    char **items;
    size_t len;
    size_t cap;
};

static int vec_push(struct glob_vec *v, char *s) {
    if (v->len == v->cap) {
        size_t ncap = v->cap ? v->cap * 2u : 16u;
        char **ni = (char **)reallocarray(v->items, ncap, sizeof(char *));
        if (!ni) {
            return GLOB_NOSPACE;
        }
        v->items = ni;
        v->cap = ncap;
    }
    v->items[v->len++] = s;
    return 0;
}

static void vec_free(struct glob_vec *v) {
    for (size_t i = 0u; i < v->len; i++) {
        free(v->items[i]);
    }
    free(v->items);
    v->items = NULL;
    v->len = 0u;
    v->cap = 0u;
}

static int has_glob_meta(const char *seg, size_t len, int noescape) {
    for (size_t i = 0u; i < len; i++) {
        if (!noescape && seg[i] == '\\' && i + 1u < len) {
            i++;
            continue;
        }
        if (seg[i] == '*' || seg[i] == '?' || seg[i] == '[') {
            return 1;
        }
    }
    return 0;
}

static void unescape_seg(const char *seg, size_t len, int noescape, char *out,
                         size_t out_cap) {
    size_t j = 0u;
    for (size_t i = 0u; i < len && j + 1u < out_cap; i++) {
        if (!noescape && seg[i] == '\\' && i + 1u < len) {
            i++;
        }
        out[j++] = seg[i];
    }
    out[j] = '\0';
}

static int join_path(const char *base, const char *name, char *out,
                     size_t cap) {
    size_t blen = strlen(base);
    size_t nlen = strlen(name);
    if (blen == 0u) {
        if (nlen + 1u > cap) {
            return -1;
        }
        memcpy(out, name, nlen + 1u);
        return 0;
    }
    int need_slash = (base[blen - 1u] != '/');
    if (blen + (need_slash ? 1u : 0u) + nlen + 1u > cap) {
        return -1;
    }
    memcpy(out, base, blen);
    size_t pos = blen;
    if (need_slash) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, nlen + 1u);
    return 0;
}

static int add_match(struct glob_vec *out, const char *path, int dir_only,
                     int flags) {
    struct stat st = {0};
    int is_dir = 0;
    if (stat(path, &st) == 0) {
        is_dir = S_ISDIR(st.st_mode);
    } else if (dir_only) {
        return 0;
    } else if (lstat(path, &st) != 0) {
        return 0;
    }
    if (dir_only && !is_dir) {
        return 0;
    }
    size_t len = strlen(path);
    int add_slash = (dir_only || ((flags & GLOB_MARK) && is_dir)) &&
                    (len == 0u || path[len - 1u] != '/');
    char *copy = (char *)malloc(len + (add_slash ? 2u : 1u));
    if (!copy) {
        return GLOB_NOSPACE;
    }
    memcpy(copy, path, len);
    if (add_slash) {
        copy[len++] = '/';
    }
    copy[len] = '\0';
    int rc = vec_push(out, copy);
    if (rc != 0) {
        free(copy);
    }
    return rc;
}

static int glob_walk(const char *prefix, const char *pat, int flags,
                     int (*errfunc)(const char *, int), struct glob_vec *out) {
    /* Consume leading slashes if prefix is empty (absolute pattern). */
    if (prefix[0] == '\0' && pat[0] == '/') {
        size_t s = 0u;
        while (pat[s] == '/') {
            s++;
        }
        if (pat[s] == '\0') {
            return add_match(out, "/", 0, flags);
        }
        return glob_walk("/", pat + s, flags, errfunc, out);
    }

    const char *slash = strchr(pat, '/');
    size_t seglen = slash ? (size_t)(slash - pat) : strlen(pat);
    const char *rest = slash ? slash : "";
    int trailing_slash = 0;
    if (slash) {
        while (*rest == '/') {
            rest++;
        }
        if (*rest == '\0') {
            trailing_slash = 1;
        }
    }

    int noescape = (flags & GLOB_NOESCAPE) != 0;
    if (seglen >= PATH_MAX) {
        return 0;
    }
    char seg[PATH_MAX];
    memcpy(seg, pat, seglen);
    seg[seglen] = '\0';

    if (!has_glob_meta(seg, seglen, noescape)) {
        char lit[PATH_MAX];
        unescape_seg(seg, seglen, noescape, lit, sizeof(lit));
        char next[PATH_MAX];
        if (join_path(prefix, lit, next, sizeof(next)) != 0) {
            return 0;
        }
        if (*rest == '\0') {
            return add_match(out, next, trailing_slash, flags);
        }
        struct stat st = {0};
        if (stat(next, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return 0;
        }
        return glob_walk(next, rest, flags, errfunc, out);
    }

    const char *open_path = (prefix[0] != '\0') ? prefix : ".";
    DIR *dp = opendir(open_path);
    if (!dp) {
        int saved = errno;
        if ((errfunc && errfunc(open_path, saved) != 0) || (flags & GLOB_ERR)) {
            return GLOB_ABORTED;
        }
        return 0;
    }

    int fn_flags = FNM_PERIOD | (noescape ? FNM_NOESCAPE : 0);
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            if (seg[0] != '.') {
                continue;
            }
        }
        if (fnmatch(seg, de->d_name, fn_flags) != 0) {
            continue;
        }
        char next[PATH_MAX];
        if (join_path(prefix, de->d_name, next, sizeof(next)) != 0) {
            continue;
        }
        int rc = 0;
        if (*rest == '\0') {
            rc = add_match(out, next, trailing_slash, flags);
        } else {
            struct stat st = {0};
            if (stat(next, &st) == 0 && S_ISDIR(st.st_mode)) {
                rc = glob_walk(next, rest, flags, errfunc, out);
            }
        }
        if (rc != 0) {
            (void)closedir(dp);
            return rc;
        }
    }
    (void)closedir(dp);
    return 0;
}

static int cmp_str_ptr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcoll(*sa, *sb);
}

int glob(const char *restrict pattern, int flags,
         int (*errfunc)(const char *epath, int eerrno),
         glob_t *restrict pglob) {
    if (!pattern || !pglob) {
        return GLOB_NOMATCH;
    }
    size_t offs = (flags & GLOB_DOOFFS) ? pglob->gl_offs : 0u;
    if (!(flags & GLOB_APPEND)) {
        pglob->gl_pathc = 0u;
        pglob->gl_pathv = NULL;
    }

    struct glob_vec found = {0};
    int rc = 0;
    if (pattern[0] != '\0') {
        rc = glob_walk("", pattern, flags, errfunc, &found);
    }
    if (rc != 0) {
        vec_free(&found);
        return rc;
    }

    if (found.len == 0u) {
        if (flags & GLOB_NOCHECK) {
            char *dup = strdup(pattern);
            if (!dup) {
                vec_free(&found);
                return GLOB_NOSPACE;
            }
            if (vec_push(&found, dup) != 0) {
                free(dup);
                vec_free(&found);
                return GLOB_NOSPACE;
            }
        } else {
            vec_free(&found);
            return GLOB_NOMATCH;
        }
    } else if (!(flags & GLOB_NOSORT) && found.len > 1u) {
        qsort(found.items, found.len, sizeof(char *), cmp_str_ptr);
    }

    size_t old_count = (flags & GLOB_APPEND) ? pglob->gl_pathc : 0u;
    size_t total = offs + old_count + found.len + 1u;
    char **nv = (char **)reallocarray(
        (flags & GLOB_APPEND) ? pglob->gl_pathv : NULL, total, sizeof(char *));
    if (!nv) {
        vec_free(&found);
        return GLOB_NOSPACE;
    }
    if (!(flags & GLOB_APPEND)) {
        for (size_t i = 0u; i < offs; i++) {
            nv[i] = NULL;
        }
    }
    for (size_t i = 0u; i < found.len; i++) {
        nv[offs + old_count + i] = found.items[i];
    }
    nv[offs + old_count + found.len] = NULL;
    free(found.items);

    pglob->gl_pathv = nv;
    pglob->gl_pathc = old_count + found.len;
    return 0;
}

void globfree(glob_t *pglob) {
    if (!pglob || !pglob->gl_pathv) {
        return;
    }
    size_t idx = 0u;
    while (idx < pglob->gl_offs && pglob->gl_pathv[idx] == NULL) {
        idx++;
    }
    for (size_t i = 0u; i < pglob->gl_pathc; i++) {
        free(pglob->gl_pathv[idx + i]);
    }
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0u;
}
