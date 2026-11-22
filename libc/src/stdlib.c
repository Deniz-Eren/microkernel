/**
 * @file     stdlib.c
 * @brief    POSIX.1-2024 process exit, environment mutation, and strto* APIs
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/bootinfo.h>
#include <unistd.h>

/* need:: COMP_LIBC_0001 COMP_LIBC_0004 COMP_LIBC_0006 */

#define AT_SECURE_KEY 23UL
#define ATEXIT_CAPACITY 64u

static pthread_mutex_t g_env_lock = PTHREAD_MUTEX_INITIALIZER;
static char **g_owned_env = 0;
static size_t g_owned_cap = 0;

static pthread_mutex_t g_atexit_lock = PTHREAD_MUTEX_INITIALIZER;
static void (*g_atexit_fns[ATEXIT_CAPACITY])(void);
static size_t g_atexit_count = 0;

int atexit(void (*func)(void)) {
    if (!func) {
        errno = EINVAL;
        return -1;
    }
    (void)pthread_mutex_lock(&g_atexit_lock);
    if (g_atexit_count >= ATEXIT_CAPACITY) {
        (void)pthread_mutex_unlock(&g_atexit_lock);
        errno = ENOMEM;
        return -1;
    }
    g_atexit_fns[g_atexit_count++] = func;
    (void)pthread_mutex_unlock(&g_atexit_lock);
    return 0;
}

static void run_atexit_handlers(void) {
    for (;;) {
        void (*fn)(void) = 0;
        (void)pthread_mutex_lock(&g_atexit_lock);
        if (g_atexit_count > 0u) {
            fn = g_atexit_fns[--g_atexit_count];
        }
        (void)pthread_mutex_unlock(&g_atexit_lock);
        if (!fn) {
            break;
        }
        fn();
    }
}

void exit(int status) {
    extern void __libc_fini_array(void);
    run_atexit_handlers();
    __libc_fini_array();
    (void)fflush(0);
    _exit(status);
}

void _Exit(int status) {
    _exit(status);
}

void abort(void) {
    _exit(134);
}

static char *getenv_unlocked(const char *name) {
    if (!name || !*name || strchr(name, '=') || !environ) {
        return 0;
    }
    size_t nlen = strlen(name);
    for (char **e = environ; *e; e++) {
        if (strncmp(*e, name, nlen) == 0 && (*e)[nlen] == '=') {
            return *e + nlen + 1;
        }
    }
    return 0;
}

char *getenv(const char *name) {
    (void)pthread_mutex_lock(&g_env_lock);
    char *v = getenv_unlocked(name);
    (void)pthread_mutex_unlock(&g_env_lock);
    return v;
}

char *secure_getenv(const char *name) {
    const unsigned long *auxv = (const unsigned long *)__auxv;
    if (auxv) {
        for (size_t i = 0; auxv[i] != 0UL; i += 2) {
            if (auxv[i] == AT_SECURE_KEY && auxv[i + 1] != 0UL) {
                return 0;
            }
        }
    }
    return getenv(name);
}

static int env_ensure_owned_locked(size_t extra_slots) {
    size_t count = 0;
    if (environ) {
        while (environ[count]) {
            count++;
        }
    }
    size_t need = count + extra_slots + 1u;
    if (g_owned_env && environ == g_owned_env && need <= g_owned_cap) {
        return 0;
    }

    size_t new_cap = g_owned_cap ? g_owned_cap : 16u;
    while (new_cap < need) {
        new_cap *= 2u;
    }
    char **arr = (char **)calloc(new_cap, sizeof(char *));
    if (!arr) {
        errno = ENOMEM;
        return -1;
    }

    if (g_owned_env && environ == g_owned_env) {
        for (size_t i = 0; i < count; i++) {
            arr[i] = g_owned_env[i];
        }
        free(g_owned_env);
    } else {
        for (size_t i = 0; i < count; i++) {
            arr[i] = strdup(environ[i]);
            if (!arr[i]) {
                for (size_t j = 0; j < i; j++) {
                    free(arr[j]);
                }
                free(arr);
                errno = ENOMEM;
                return -1;
            }
        }
    }

    arr[count] = 0;
    g_owned_env = arr;
    g_owned_cap = new_cap;
    environ = g_owned_env;
    return 0;
}

int setenv(const char *envname, const char *envval, int overwrite) {
    if (!envname || !*envname || strchr(envname, '=') || !envval) {
        errno = EINVAL;
        return -1;
    }
    size_t nlen = strlen(envname);
    size_t vlen = strlen(envval);

    (void)pthread_mutex_lock(&g_env_lock);
    if (env_ensure_owned_locked(1u) != 0) {
        (void)pthread_mutex_unlock(&g_env_lock);
        return -1;
    }

    size_t idx = 0;
    while (g_owned_env[idx]) {
        if (strncmp(g_owned_env[idx], envname, nlen) == 0 &&
            g_owned_env[idx][nlen] == '=') {
            if (!overwrite) {
                (void)pthread_mutex_unlock(&g_env_lock);
                return 0;
            }
            break;
        }
        idx++;
    }

    char *entry = (char *)malloc(nlen + 1u + vlen + 1u);
    if (!entry) {
        (void)pthread_mutex_unlock(&g_env_lock);
        errno = ENOMEM;
        return -1;
    }
    memcpy(entry, envname, nlen);
    entry[nlen] = '=';
    memcpy(entry + nlen + 1u, envval, vlen + 1u);

    if (g_owned_env[idx]) {
        free(g_owned_env[idx]);
        g_owned_env[idx] = entry;
    } else {
        g_owned_env[idx] = entry;
        g_owned_env[idx + 1u] = 0;
    }
    (void)pthread_mutex_unlock(&g_env_lock);
    return 0;
}

int unsetenv(const char *name) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    size_t nlen = strlen(name);

    (void)pthread_mutex_lock(&g_env_lock);
    if (!environ) {
        (void)pthread_mutex_unlock(&g_env_lock);
        return 0;
    }
    if (env_ensure_owned_locked(0u) != 0) {
        (void)pthread_mutex_unlock(&g_env_lock);
        return -1;
    }

    size_t i = 0;
    while (g_owned_env[i]) {
        if (strncmp(g_owned_env[i], name, nlen) == 0 &&
            g_owned_env[i][nlen] == '=') {
            free(g_owned_env[i]);
            size_t j = i;
            while (g_owned_env[j + 1u]) {
                g_owned_env[j] = g_owned_env[j + 1u];
                j++;
            }
            g_owned_env[j] = 0;
            continue;
        }
        i++;
    }
    (void)pthread_mutex_unlock(&g_env_lock);
    return 0;
}

static unsigned long long parse_u64(const char *str, char **endptr, int base,
                                    int *neg_out, int *ovf_out) {
    const char *orig = str;
    const char *s = str;
    *neg_out = 0;
    *ovf_out = 0;

    if (base < 0 || base == 1 || base > 36) {
        errno = EINVAL;
        if (endptr) {
            *endptr = (char *)orig;
        }
        return 0ULL;
    }

    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '-') {
        *neg_out = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if ((base == 0 || base == 16) && s[0] == '0' &&
        (s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2])) {
        base = 16;
        s += 2;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    unsigned long long acc = 0ULL;
    unsigned long long ubase = (unsigned long long)base;
    unsigned long long cutoff = ULLONG_MAX / ubase;
    unsigned long long cutlim = ULLONG_MAX % ubase;
    int any = 0;

    while (*s) {
        int d = -1;
        if (*s >= '0' && *s <= '9') {
            d = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            d = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            d = *s - 'A' + 10;
        } else {
            break;
        }
        if (d >= base) {
            break;
        }
        any = 1;
        if (acc > cutoff || (acc == cutoff && (unsigned long long)d > cutlim)) {
            *ovf_out = 1;
        } else {
            acc = acc * ubase + (unsigned long long)d;
        }
        s++;
    }

    if (endptr) {
        *endptr = (char *)(any ? s : orig);
    }
    return acc;
}

unsigned long long strtoull(const char *restrict str, char **restrict endptr,
                            int base) {
    int neg = 0;
    int ovf = 0;
    unsigned long long v = parse_u64(str, endptr, base, &neg, &ovf);
    if (ovf) {
        errno = ERANGE;
        return ULLONG_MAX;
    }
    return neg ? (0ULL - v) : v;
}

unsigned long strtoul(const char *restrict str, char **restrict endptr,
                      int base) {
    return (unsigned long)strtoull(str, endptr, base);
}

long long strtoll(const char *restrict str, char **restrict endptr, int base) {
    int neg = 0;
    int ovf = 0;
    unsigned long long v = parse_u64(str, endptr, base, &neg, &ovf);
    unsigned long long max_pos = (unsigned long long)LLONG_MAX;
    unsigned long long max_neg = max_pos + 1ULL;

    if (neg) {
        if (ovf || v > max_neg) {
            errno = ERANGE;
            return LLONG_MIN;
        }
        if (v == max_neg) {
            return LLONG_MIN;
        }
        return -(long long)v;
    }
    if (ovf || v > max_pos) {
        errno = ERANGE;
        return LLONG_MAX;
    }
    return (long long)v;
}

long strtol(const char *restrict str, char **restrict endptr, int base) {
    return (long)strtoll(str, endptr, base);
}

int atoi(const char *s) {
    return (int)strtol(s, 0, 10);
}

long atol(const char *s) {
    return strtol(s, 0, 10);
}

long long atoll(const char *s) {
    return strtoll(s, 0, 10);
}

int abs(int i) {
    return i < 0 ? -i : i;
}

long labs(long i) {
    return i < 0 ? -i : i;
}

long long llabs(long long i) {
    return i < 0 ? -i : i;
}

static void swap_bytes(unsigned char *a, unsigned char *b, size_t width) {
    for (size_t i = 0; i < width; i++) {
        unsigned char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void sift_down(unsigned char *arr, size_t start, size_t end,
                      size_t width, int (*compar)(const void *, const void *)) {
    size_t root = start;
    while (root * 2u + 1u < end) {
        size_t child = root * 2u + 1u;
        if (child + 1u < end &&
            compar(arr + child * width, arr + (child + 1u) * width) < 0) {
            child++;
        }
        if (compar(arr + root * width, arr + child * width) < 0) {
            swap_bytes(arr + root * width, arr + child * width, width);
            root = child;
        } else {
            return;
        }
    }
}

void qsort(void *base, size_t nel, size_t width,
           int (*compar)(const void *, const void *)) {
    if (!base || !compar || nel < 2u || width == 0u) {
        return;
    }
    unsigned char *arr = (unsigned char *)base;
    for (size_t start = nel / 2u; start > 0u; start--) {
        sift_down(arr, start - 1u, nel, width, compar);
    }
    for (size_t end = nel - 1u; end > 0u; end--) {
        swap_bytes(arr, arr + end * width, width);
        sift_down(arr, 0u, end, width, compar);
    }
}

void *bsearch(const void *key, const void *base, size_t nel, size_t width,
              int (*compar)(const void *, const void *)) {
    if (!key || !base || !compar || nel == 0u || width == 0u) {
        return 0;
    }
    const unsigned char *arr = (const unsigned char *)base;
    size_t lo = 0u;
    size_t hi = nel;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2u;
        const void *elem = arr + mid * width;
        int cmp = compar(key, elem);
        if (cmp < 0) {
            hi = mid;
        } else if (cmp > 0) {
            lo = mid + 1u;
        } else {
            return (void *)elem;
        }
    }
    return 0;
}
