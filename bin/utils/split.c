/**
 * @file     split.c
 * @brief    Ring 3 split utility targeting POSIX.1-2024 XCU (-l, -b, -a)
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
#include <unistd.h>

/* need:: COMP_LIBC_0023 */

static int make_suffix(unsigned long idx, int width, char *out) {
    for (int i = width - 1; i >= 0; i--) {
        out[i] = (char)('a' + (idx % 26ul));
        idx /= 26ul;
    }
    out[width] = '\0';
    return (idx == 0ul) ? 0 : -1;
}

static FILE *open_chunk(const char *prefix, unsigned long idx, int sw) {
    char suf[32];
    if (sw <= 0 || sw >= (int)sizeof(suf) || make_suffix(idx, sw, suf) != 0) {
        (void)fprintf(stderr, "split: output file suffix exhausted\n");
        return NULL;
    }
    char path[512];
    (void)snprintf(path, sizeof(path), "%s%s", prefix, suf);
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
    }
    return fp;
}

static int parse_bytes(const char *arg, unsigned long *out) {
    char *end = NULL;
    unsigned long v = strtoul(arg, &end, 10);
    if (end == arg || v == 0ul) {
        return -1;
    }
    if (*end == 'k' || *end == 'K') {
        v *= 1024ul;
        end++;
    } else if (*end == 'm' || *end == 'M') {
        v *= 1048576ul;
        end++;
    }
    if (*end != '\0') {
        return -1;
    }
    *out = v;
    return 0;
}

int main(int argc, char **argv) {
    int suffix_len = 2;
    int byte_mode = 0;
    unsigned long limit = 1000ul;
    int opt;
    while ((opt = getopt(argc, argv, "a:b:l:")) != -1) {
        switch (opt) {
        case 'a':
            suffix_len = (int)strtol(optarg, NULL, 10);
            if (suffix_len <= 0 || suffix_len > 16) {
                (void)fprintf(stderr, "split: invalid suffix length\n");
                return 1;
            }
            break;
        case 'b':
            byte_mode = 1;
            if (parse_bytes(optarg, &limit) != 0) {
                (void)fprintf(stderr, "split: invalid byte count\n");
                return 1;
            }
            break;
        case 'l':
            byte_mode = 0;
            limit = strtoul(optarg, NULL, 10);
            if (limit == 0ul) {
                (void)fprintf(stderr, "split: invalid line count\n");
                return 1;
            }
            break;
        default:
            return 1;
        }
    }

    const char *in_path = (optind < argc) ? argv[optind] : "-";
    const char *prefix = (optind + 1 < argc) ? argv[optind + 1] : "x";
    FILE *in = (strcmp(in_path, "-") == 0) ? stdin : fopen(in_path, "rb");
    if (!in) {
        perror(in_path);
        return 1;
    }

    FILE *out = NULL;
    unsigned long chunk_idx = 0ul;
    unsigned long cur_cnt = 0ul;
    int rc = 0;

    if (byte_mode) {
        uint8_t buf[1024];
        for (;;) {
            size_t got = fread(buf, 1u, sizeof(buf), in);
            if (got == 0u) {
                break;
            }
            size_t off = 0u;
            while (off < got) {
                if (!out || cur_cnt >= limit) {
                    if (out && fclose(out) != 0) {
                        rc = 1;
                    }
                    out = open_chunk(prefix, chunk_idx++, suffix_len);
                    if (!out) {
                        rc = 1;
                        goto done;
                    }
                    cur_cnt = 0ul;
                }
                size_t space = (size_t)(limit - cur_cnt);
                size_t take = (got - off < space) ? (got - off) : space;
                if (fwrite(buf + off, 1u, take, out) != take) {
                    rc = 1;
                    goto done;
                }
                off += take;
                cur_cnt += take;
            }
        }
    } else {
        int ch;
        while ((ch = fgetc(in)) != EOF) {
            if (!out || cur_cnt >= limit) {
                if (out && fclose(out) != 0) {
                    rc = 1;
                }
                out = open_chunk(prefix, chunk_idx++, suffix_len);
                if (!out) {
                    rc = 1;
                    goto done;
                }
                cur_cnt = 0ul;
            }
            if (fputc(ch, out) == EOF) {
                rc = 1;
                goto done;
            }
            if (ch == '\n') {
                cur_cnt++;
            }
        }
    }

done:
    if (out && fclose(out) != 0) {
        rc = 1;
    }
    if (in != stdin) {
        (void)fclose(in);
    }
    return rc;
}
