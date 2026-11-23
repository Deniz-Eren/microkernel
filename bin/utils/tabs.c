/**
 * @file     tabs.c
 * @brief    Ring 3 POSIX.1-2024 XCU tabs terminal tab-stop utility
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* need:: COMP_LIBC_0026 */

#define MAX_STOPS 64

static int parse_list(const char *s, int *stops, size_t *count) {
    *count = 0;
    const char *p = s;
    int prev = 0;
    while (*p != '\0') {
        int rel = 0;
        if (*p == '+') {
            rel = 1;
            p++;
        }
        if (!isdigit((unsigned char)*p)) {
            return -1;
        }
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (v < 0 || (v == 0 && !rel)) {
            return -1;
        }
        int col = rel ? (prev + (int)v) : (int)v;
        if (col <= prev || *count >= MAX_STOPS) {
            return -1;
        }
        stops[(*count)++] = col;
        prev = col;
        p = end;
        if (*p == ',' || *p == ' ') {
            p++;
        } else if (*p != '\0') {
            return -1;
        }
    }
    return (*count > 0) ? 0 : -1;
}

static void fill_uniform(int step, int max_col, int *stops, size_t *count) {
    *count = 0;
    if (step <= 0) {
        return;
    }
    for (int col = 1 + step; col <= max_col && *count < MAX_STOPS;
         col += step) {
        stops[(*count)++] = col;
    }
}

static int apply_preset(const char *flag, int *stops, size_t *count) {
    static const int k_a[] = {1, 10, 16, 36, 72};
    static const int k_a2[] = {1, 10, 16, 40, 72};
    static const int k_c[] = {1, 8, 12, 16, 20, 55};
    static const int k_c2[] = {1, 6, 10, 14, 49};
    static const int k_c3[] = {1,  6,  10, 14, 18, 22, 26, 30, 34,
                               38, 42, 46, 50, 54, 58, 62, 67};
    static const int k_f[] = {1, 7, 11, 15, 19, 23};
    static const int k_p[] = {1,  5,  9,  13, 17, 21, 25, 29,
                              33, 37, 41, 45, 49, 53, 57, 61};
    static const int k_s[] = {1, 10, 55};
    static const int k_u[] = {1, 12, 20, 44};

    const int *src = NULL;
    size_t n = 0;
    if (strcmp(flag, "-a") == 0) {
        src = k_a;
        n = sizeof(k_a) / sizeof(k_a[0]);
    } else if (strcmp(flag, "-a2") == 0) {
        src = k_a2;
        n = sizeof(k_a2) / sizeof(k_a2[0]);
    } else if (strcmp(flag, "-c") == 0) {
        src = k_c;
        n = sizeof(k_c) / sizeof(k_c[0]);
    } else if (strcmp(flag, "-c2") == 0) {
        src = k_c2;
        n = sizeof(k_c2) / sizeof(k_c2[0]);
    } else if (strcmp(flag, "-c3") == 0) {
        src = k_c3;
        n = sizeof(k_c3) / sizeof(k_c3[0]);
    } else if (strcmp(flag, "-f") == 0) {
        src = k_f;
        n = sizeof(k_f) / sizeof(k_f[0]);
    } else if (strcmp(flag, "-p") == 0) {
        src = k_p;
        n = sizeof(k_p) / sizeof(k_p[0]);
    } else if (strcmp(flag, "-s") == 0) {
        src = k_s;
        n = sizeof(k_s) / sizeof(k_s[0]);
    } else if (strcmp(flag, "-u") == 0) {
        src = k_u;
        n = sizeof(k_u) / sizeof(k_u[0]);
    } else {
        return -1;
    }
    (void)memcpy(stops, src, n * sizeof(int));
    *count = n;
    return 0;
}

int main(int argc, char **argv) {
    int stops[MAX_STOPS];
    size_t count = 0;
    int margin = 0;
    int spec_set = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-T") == 0) {
            if (i + 1 >= argc) {
                (void)fprintf(stderr, "tabs: -T requires a terminal type\n");
                return 1;
            }
            i++;
            continue;
        }
        if (strncmp(a, "-T", 2) == 0 && a[2] != '\0') {
            continue;
        }
        if (strncmp(a, "+m", 2) == 0) {
            margin = (a[2] != '\0') ? atoi(a + 2) : 10;
            if (margin < 0) {
                (void)fprintf(stderr, "tabs: invalid margin: %s\n", a);
                return 1;
            }
            continue;
        }
        if (a[0] == '-' && isdigit((unsigned char)a[1])) {
            char *end = NULL;
            long step = strtol(a + 1, &end, 10);
            if (!end || *end != '\0' || step < 0) {
                (void)fprintf(stderr, "tabs: invalid tab increment: %s\n", a);
                return 1;
            }
            fill_uniform((int)step, 80, stops, &count);
            spec_set = 1;
            continue;
        }
        if (a[0] == '-') {
            if (apply_preset(a, stops, &count) != 0) {
                (void)fprintf(stderr, "tabs: invalid option: %s\n", a);
                return 1;
            }
            spec_set = 1;
            continue;
        }
        if (parse_list(a, stops, &count) != 0) {
            (void)fprintf(stderr, "tabs: invalid tab list: %s\n", a);
            return 1;
        }
        spec_set = 1;
    }

    if (!spec_set) {
        fill_uniform(8, 80, stops, &count);
    }

    /* Clear all terminal tab stops, then set requested stops via HTS */
    (void)fputs("\r\033[3g", stdout);
    int cur = 1;
    for (size_t i = 0; i < count; i++) {
        int target = stops[i] + margin;
        if (target < cur) {
            continue;
        }
        int delta = target - cur;
        if (delta > 0) {
            (void)printf("\033[%dC", delta);
        }
        (void)fputs("\033H", stdout);
        cur = target;
    }
    (void)putchar('\r');
    return 0;
}
