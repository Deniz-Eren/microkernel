/**
 * @file     fnmatch.c
 * @brief    POSIX.1-2024 fnmatch() filename wildcard pattern matcher
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <fnmatch.h>
#include <stddef.h>
#include <string.h>

/* need:: COMP_LIBC_0006 */

static unsigned char fold_char(unsigned char c, int flags) {
    if (flags & FNM_IGNORECASE) {
        return (unsigned char)tolower((int)c);
    }
    return c;
}

static int match_char_class(const char *name, size_t len, unsigned char c) {
    if (len == 5 && memcmp(name, "alnum", 5) == 0) {
        return isalnum((int)c);
    }
    if (len == 5 && memcmp(name, "alpha", 5) == 0) {
        return isalpha((int)c);
    }
    if (len == 5 && memcmp(name, "blank", 5) == 0) {
        return isblank((int)c);
    }
    if (len == 5 && memcmp(name, "cntrl", 5) == 0) {
        return iscntrl((int)c);
    }
    if (len == 5 && memcmp(name, "digit", 5) == 0) {
        return isdigit((int)c);
    }
    if (len == 5 && memcmp(name, "graph", 5) == 0) {
        return isgraph((int)c);
    }
    if (len == 5 && memcmp(name, "lower", 5) == 0) {
        return islower((int)c);
    }
    if (len == 5 && memcmp(name, "print", 5) == 0) {
        return isprint((int)c);
    }
    if (len == 5 && memcmp(name, "punct", 5) == 0) {
        return ispunct((int)c);
    }
    if (len == 5 && memcmp(name, "space", 5) == 0) {
        return isspace((int)c);
    }
    if (len == 5 && memcmp(name, "upper", 5) == 0) {
        return isupper((int)c);
    }
    if (len == 6 && memcmp(name, "xdigit", 6) == 0) {
        return isxdigit((int)c);
    }
    return 0;
}

/* Parse a bracket expression starting right after '['. Returns:
 *   1 if c matched, 0 if not matched, -1 if unclosed (treat '[' literally). */
static int match_bracket(const char *p, unsigned char c, int flags,
                         const char **after_out) {
    int negate = 0;
    if (*p == '!' || *p == '^') {
        negate = 1;
        p++;
    }
    if (*p == '\0') {
        return -1;
    }

    int matched = 0;
    unsigned char fc = fold_char(c, flags);
    int first = 1;

    while (*p != '\0') {
        if (*p == ']' && !first) {
            *after_out = p + 1;
            return negate ? !matched : matched;
        }
        first = 0;

        if (*p == '[' && p[1] == ':') {
            const char *cls_start = p + 2;
            const char *cls_end = cls_start;
            while (*cls_end != '\0' &&
                   !(cls_end[0] == ':' && cls_end[1] == ']')) {
                cls_end++;
            }
            if (cls_end[0] == ':' && cls_end[1] == ']') {
                size_t clen = (size_t)(cls_end - cls_start);
                if (match_char_class(cls_start, clen, c) ||
                    ((flags & FNM_IGNORECASE) &&
                     (match_char_class(cls_start, clen,
                                       (unsigned char)tolower((int)c)) ||
                      match_char_class(cls_start, clen,
                                       (unsigned char)toupper((int)c))))) {
                    matched = 1;
                }
                p = cls_end + 2;
                continue;
            }
        }

        unsigned char lo = (unsigned char)*p;
        if (!(flags & FNM_NOESCAPE) && *p == '\\' && p[1] != '\0') {
            p++;
            lo = (unsigned char)*p;
        }
        p++;

        if (*p == '-' && p[1] != '\0' && p[1] != ']') {
            p++;
            unsigned char hi = (unsigned char)*p;
            if (!(flags & FNM_NOESCAPE) && *p == '\\' && p[1] != '\0') {
                p++;
                hi = (unsigned char)*p;
            }
            p++;
            unsigned char flo = fold_char(lo, flags);
            unsigned char fhi = fold_char(hi, flags);
            if (flo <= fc && fc <= fhi) {
                matched = 1;
            }
        } else {
            if (fold_char(lo, flags) == fc) {
                matched = 1;
            }
        }
    }
    return -1;
}

static int fnmatch_rec(const char *p, const char *s, const char *s_orig,
                       int flags) {
    while (*p != '\0') {
        char pc = *p;
        if (pc == '*') {
            while (*p == '*') {
                p++;
            }
            if ((flags & FNM_PERIOD) && *s == '.' &&
                (s == s_orig || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                return FNM_NOMATCH;
            }
            if (*p == '\0') {
                if (flags & FNM_PATHNAME) {
                    return strchr(s, '/') ? FNM_NOMATCH : 0;
                }
                return 0;
            }
            while (*s != '\0') {
                if (fnmatch_rec(p, s, s_orig, flags) == 0) {
                    return 0;
                }
                if ((flags & FNM_PATHNAME) && *s == '/') {
                    break;
                }
                s++;
            }
            return fnmatch_rec(p, s, s_orig, flags);
        }

        if (*s == '\0') {
            return FNM_NOMATCH;
        }

        if (pc == '?') {
            if ((flags & FNM_PATHNAME) && *s == '/') {
                return FNM_NOMATCH;
            }
            if ((flags & FNM_PERIOD) && *s == '.' &&
                (s == s_orig || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                return FNM_NOMATCH;
            }
            p++;
            s++;
            continue;
        }

        if (pc == '[') {
            if ((flags & FNM_PATHNAME) && *s == '/') {
                return FNM_NOMATCH;
            }
            if ((flags & FNM_PERIOD) && *s == '.' &&
                (s == s_orig || ((flags & FNM_PATHNAME) && s[-1] == '/'))) {
                return FNM_NOMATCH;
            }
            const char *after = 0;
            int bm = match_bracket(p + 1, (unsigned char)*s, flags, &after);
            if (bm == 1) {
                p = after;
                s++;
                continue;
            }
            if (bm == 0) {
                return FNM_NOMATCH;
            }
            /* Unclosed '[': fall through and compare literally. */
        }

        if (pc == '\\' && !(flags & FNM_NOESCAPE)) {
            p++;
            if (*p == '\0') {
                return FNM_NOMATCH;
            }
            pc = *p;
        }

        if (fold_char((unsigned char)pc, flags) !=
            fold_char((unsigned char)*s, flags)) {
            return FNM_NOMATCH;
        }
        p++;
        s++;
    }
    return (*s == '\0') ? 0 : FNM_NOMATCH;
}

int fnmatch(const char *pattern, const char *string, int flags) {
    if (!pattern || !string) {
        return FNM_NOMATCH;
    }
    return fnmatch_rec(pattern, string, string, flags);
}
