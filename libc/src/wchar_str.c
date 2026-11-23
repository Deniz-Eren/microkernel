/**
 * @file     wchar_str.c
 * @brief    POSIX.1-2024 wide memory and wide string manipulation routines
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

/* need:: COMP_LIBC_0012 */

wchar_t *wmemchr(const wchar_t *ws, wchar_t wc, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (ws[i] == wc) {
            return (wchar_t *)(ws + i);
        }
    }
    return NULL;
}

int wmemcmp(const wchar_t *ws1, const wchar_t *ws2, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (ws1[i] != ws2[i]) {
            return ws1[i] < ws2[i] ? -1 : 1;
        }
    }
    return 0;
}

wchar_t *wmemcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        ws1[i] = ws2[i];
    }
    return ws1;
}

wchar_t *wmemmove(wchar_t *ws1, const wchar_t *ws2, size_t n) {
    if (ws1 == ws2 || n == 0u) {
        return ws1;
    }
    if (ws1 < ws2) {
        for (size_t i = 0u; i < n; i++) {
            ws1[i] = ws2[i];
        }
    } else {
        for (size_t i = n; i > 0u; i--) {
            ws1[i - 1u] = ws2[i - 1u];
        }
    }
    return ws1;
}

wchar_t *wmemset(wchar_t *ws, wchar_t wc, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        ws[i] = wc;
    }
    return ws;
}

size_t wcslen(const wchar_t *ws) {
    size_t n = 0u;
    while (ws[n] != L'\0') {
        n++;
    }
    return n;
}

size_t wcsnlen(const wchar_t *ws, size_t maxlen) {
    size_t n = 0u;
    while (n < maxlen && ws[n] != L'\0') {
        n++;
    }
    return n;
}

wchar_t *wcpcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2) {
    while ((*ws1 = *ws2) != L'\0') {
        ws1++;
        ws2++;
    }
    return ws1;
}

wchar_t *wcscpy(wchar_t *restrict ws1, const wchar_t *restrict ws2) {
    (void)wcpcpy(ws1, ws2);
    return ws1;
}

wchar_t *wcpncpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    size_t i = 0u;
    for (; i < n && ws2[i] != L'\0'; i++) {
        ws1[i] = ws2[i];
    }
    wchar_t *end = ws1 + i;
    for (; i < n; i++) {
        ws1[i] = L'\0';
    }
    return end;
}

wchar_t *wcsncpy(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    (void)wcpncpy(ws1, ws2, n);
    return ws1;
}

size_t wcslcpy(wchar_t *restrict ws1, const wchar_t *restrict ws2,
               size_t dstsize) {
    size_t slen = wcslen(ws2);
    if (dstsize > 0u) {
        size_t copy = (slen < dstsize - 1u) ? slen : (dstsize - 1u);
        if (copy > 0u) {
            (void)wmemcpy(ws1, ws2, copy);
        }
        ws1[copy] = L'\0';
    }
    return slen;
}

wchar_t *wcscat(wchar_t *restrict ws1, const wchar_t *restrict ws2) {
    (void)wcpcpy(ws1 + wcslen(ws1), ws2);
    return ws1;
}

wchar_t *wcsncat(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    wchar_t *d = ws1 + wcslen(ws1);
    size_t i = 0u;
    for (; i < n && ws2[i] != L'\0'; i++) {
        d[i] = ws2[i];
    }
    d[i] = L'\0';
    return ws1;
}

size_t wcslcat(wchar_t *restrict ws1, const wchar_t *restrict ws2,
               size_t dstsize) {
    size_t dlen = wcsnlen(ws1, dstsize);
    size_t slen = wcslen(ws2);
    if (dlen == dstsize) {
        return dstsize + slen;
    }
    size_t avail = dstsize - dlen - 1u;
    size_t copy = (slen < avail) ? slen : avail;
    if (copy > 0u) {
        (void)wmemcpy(ws1 + dlen, ws2, copy);
    }
    ws1[dlen + copy] = L'\0';
    return dlen + slen;
}

int wcscmp(const wchar_t *ws1, const wchar_t *ws2) {
    while (*ws1 != L'\0' && *ws1 == *ws2) {
        ws1++;
        ws2++;
    }
    if (*ws1 == *ws2) {
        return 0;
    }
    return *ws1 < *ws2 ? -1 : 1;
}

int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n) {
    for (size_t i = 0u; i < n; i++) {
        if (ws1[i] != ws2[i] || ws1[i] == L'\0') {
            if (ws1[i] == ws2[i]) {
                return 0;
            }
            return ws1[i] < ws2[i] ? -1 : 1;
        }
    }
    return 0;
}

int wcscasecmp_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale) {
    for (;;) {
        wint_t c1 = towlower_l((wint_t)*ws1, locale);
        wint_t c2 = towlower_l((wint_t)*ws2, locale);
        if (c1 != c2 || c1 == (wint_t)0) {
            if (c1 == c2) {
                return 0;
            }
            return c1 < c2 ? -1 : 1;
        }
        ws1++;
        ws2++;
    }
}

int wcscasecmp(const wchar_t *ws1, const wchar_t *ws2) {
    return wcscasecmp_l(ws1, ws2, (locale_t)0);
}

int wcsncasecmp_l(const wchar_t *ws1, const wchar_t *ws2, size_t n,
                  locale_t locale) {
    for (size_t i = 0u; i < n; i++) {
        wint_t c1 = towlower_l((wint_t)ws1[i], locale);
        wint_t c2 = towlower_l((wint_t)ws2[i], locale);
        if (c1 != c2 || c1 == (wint_t)0) {
            if (c1 == c2) {
                return 0;
            }
            return c1 < c2 ? -1 : 1;
        }
    }
    return 0;
}

int wcsncasecmp(const wchar_t *ws1, const wchar_t *ws2, size_t n) {
    return wcsncasecmp_l(ws1, ws2, n, (locale_t)0);
}

int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale) {
    (void)locale;
    return wcscmp(ws1, ws2);
}

int wcscoll(const wchar_t *ws1, const wchar_t *ws2) {
    return wcscmp(ws1, ws2);
}

size_t wcsxfrm_l(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n,
                 locale_t locale) {
    (void)locale;
    size_t len = wcslen(ws2);
    if (n > 0u) {
        size_t copy = len < (n - 1u) ? len : (n - 1u);
        if (copy > 0u) {
            (void)wmemcpy(ws1, ws2, copy);
        }
        ws1[copy] = L'\0';
    }
    return len;
}

size_t wcsxfrm(wchar_t *restrict ws1, const wchar_t *restrict ws2, size_t n) {
    return wcsxfrm_l(ws1, ws2, n, (locale_t)0);
}

wchar_t *wcschr(const wchar_t *ws, wchar_t wc) {
    for (; *ws != L'\0'; ws++) {
        if (*ws == wc) {
            return (wchar_t *)ws;
        }
    }
    return (wc == L'\0') ? (wchar_t *)ws : NULL;
}

wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc) {
    const wchar_t *last = NULL;
    for (; *ws != L'\0'; ws++) {
        if (*ws == wc) {
            last = ws;
        }
    }
    if (wc == L'\0') {
        return (wchar_t *)ws;
    }
    return (wchar_t *)last;
}

size_t wcsspn(const wchar_t *ws1, const wchar_t *ws2) {
    size_t n = 0u;
    while (ws1[n] != L'\0' && wcschr(ws2, ws1[n]) != NULL) {
        n++;
    }
    return n;
}

size_t wcscspn(const wchar_t *ws1, const wchar_t *ws2) {
    size_t n = 0u;
    while (ws1[n] != L'\0' && wcschr(ws2, ws1[n]) == NULL) {
        n++;
    }
    return n;
}

wchar_t *wcspbrk(const wchar_t *ws1, const wchar_t *ws2) {
    for (; *ws1 != L'\0'; ws1++) {
        if (wcschr(ws2, *ws1) != NULL) {
            return (wchar_t *)ws1;
        }
    }
    return NULL;
}

wchar_t *wcsstr(const wchar_t *ws1, const wchar_t *ws2) {
    if (*ws2 == L'\0') {
        return (wchar_t *)ws1;
    }
    size_t nlen = wcslen(ws2);
    for (const wchar_t *p = ws1; *p != L'\0'; p++) {
        if (*p == *ws2 && wcsncmp(p, ws2, nlen) == 0) {
            return (wchar_t *)p;
        }
    }
    return NULL;
}

wchar_t *wcstok(wchar_t *restrict ws1, const wchar_t *restrict ws2,
                wchar_t **restrict ptr) {
    wchar_t *p = ws1 ? ws1 : *ptr;
    if (!p) {
        return NULL;
    }
    p += wcsspn(p, ws2);
    if (*p == L'\0') {
        *ptr = NULL;
        return NULL;
    }
    wchar_t *end = p + wcscspn(p, ws2);
    if (*end != L'\0') {
        *end = L'\0';
        *ptr = end + 1;
    } else {
        *ptr = NULL;
    }
    return p;
}

wchar_t *wcsdup(const wchar_t *string) {
    size_t len = wcslen(string);
    wchar_t *copy = (wchar_t *)reallocarray(NULL, len + 1u, sizeof(wchar_t));
    if (!copy) {
        errno = ENOMEM;
        return NULL;
    }
    (void)wmemcpy(copy, string, len + 1u);
    return copy;
}
