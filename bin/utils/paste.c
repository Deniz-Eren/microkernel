/**
 * @file     paste.c
 * @brief    Ring 3 paste utility targeting POSIX.1-2024 XCU (-d, -s)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_LIBC_0020 */

#define MAX_DELIMS 256

static int parse_delims(const char *spec, int out[MAX_DELIMS]) {
    int count = 0;
    const char *p = spec;
    while (*p != '\0' && count < MAX_DELIMS) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            if (*p == 'n') {
                out[count++] = '\n';
            } else if (*p == 't') {
                out[count++] = '\t';
            } else if (*p == '0') {
                out[count++] = -1; /* empty string delimiter */
            } else if (*p == '\\') {
                out[count++] = '\\';
            } else {
                out[count++] = (unsigned char)*p;
            }
            p++;
        } else {
            out[count++] = (unsigned char)*p++;
        }
    }
    return count;
}

static int paste_serial(FILE **fps, int nfiles, const int *delims,
                        int ndelims) {
    char *line = NULL;
    size_t cap = 0u;
    for (int i = 0; i < nfiles; i++) {
        int first = 1;
        int di = 0;
        ssize_t n;
        while ((n = getline(&line, &cap, fps[i])) >= 0) {
            size_t len = (size_t)n;
            if (len > 0u && line[len - 1u] == '\n') {
                len--;
            }
            if (!first) {
                int d = delims[di % ndelims];
                di++;
                if (d >= 0) {
                    (void)putchar(d);
                }
            }
            if (len > 0u) {
                (void)fwrite(line, 1u, len, stdout);
            }
            first = 0;
        }
        if (!first) {
            (void)putchar('\n');
        }
    }
    free(line);
    return 0;
}

static int paste_parallel(FILE **fps, int nfiles, const int *delims,
                          int ndelims) {
    char **lines = (char **)calloc((size_t)nfiles, sizeof(char *));
    size_t *caps = (size_t *)calloc((size_t)nfiles, sizeof(size_t));
    ssize_t *lens = (ssize_t *)calloc((size_t)nfiles, sizeof(ssize_t));
    int *eof = (int *)calloc((size_t)nfiles, sizeof(int));
    if (!lines || !caps || !lens || !eof) {
        free(lines);
        free(caps);
        free(lens);
        free(eof);
        return 1;
    }

    for (;;) {
        int any = 0;
        for (int i = 0; i < nfiles; i++) {
            if (eof[i]) {
                lens[i] = -1;
                continue;
            }
            lens[i] = getline(&lines[i], &caps[i], fps[i]);
            if (lens[i] < 0) {
                eof[i] = 1;
            } else {
                if (lens[i] > 0 && lines[i][lens[i] - 1] == '\n') {
                    lens[i]--;
                }
                any = 1;
            }
        }
        if (!any) {
            break;
        }
        for (int i = 0; i < nfiles; i++) {
            if (i > 0) {
                int d = delims[(i - 1) % ndelims];
                if (d >= 0) {
                    (void)putchar(d);
                }
            }
            if (lens[i] > 0) {
                (void)fwrite(lines[i], 1u, (size_t)lens[i], stdout);
            }
        }
        (void)putchar('\n');
    }

    for (int i = 0; i < nfiles; i++) {
        free(lines[i]);
    }
    free(lines);
    free(caps);
    free(lens);
    free(eof);
    return 0;
}

int main(int argc, char **argv) {
    int delims[MAX_DELIMS];
    delims[0] = '\t';
    int ndelims = 1;
    int serial = 0;
    int opt;
    while ((opt = getopt(argc, argv, "d:s")) != -1) {
        if (opt == 'd') {
            ndelims = parse_delims(optarg, delims);
            if (ndelims <= 0) {
                (void)fprintf(stderr, "paste: empty delimiter list\n");
                return 1;
            }
        } else if (opt == 's') {
            serial = 1;
        } else {
            return 1;
        }
    }

    int nfiles = argc - optind;
    if (nfiles <= 0) {
        FILE *fp = stdin;
        return serial ? paste_serial(&fp, 1, delims, ndelims)
                      : paste_parallel(&fp, 1, delims, ndelims);
    }

    FILE **fps = (FILE **)calloc((size_t)nfiles, sizeof(FILE *));
    if (!fps) {
        return 1;
    }
    int rc = 0;
    for (int i = 0; i < nfiles; i++) {
        if (strcmp(argv[optind + i], "-") == 0) {
            fps[i] = stdin;
        } else {
            fps[i] = fopen(argv[optind + i], "r");
            if (!fps[i]) {
                perror(argv[optind + i]);
                rc = 1;
                break;
            }
        }
    }
    if (rc == 0) {
        rc = serial ? paste_serial(fps, nfiles, delims, ndelims)
                    : paste_parallel(fps, nfiles, delims, ndelims);
    }
    for (int i = 0; i < nfiles; i++) {
        if (fps[i] && fps[i] != stdin) {
            (void)fclose(fps[i]);
        }
    }
    free(fps);
    return rc;
}
