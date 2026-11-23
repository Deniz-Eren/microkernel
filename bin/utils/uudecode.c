/**
 * @file     uudecode.c
 * @brief    Ring 3 POSIX.1-2024 XCU uudecode utility (historical and Base64)
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
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0026 */

static int b64_val(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return 26 + (c - 'a');
    }
    if (c >= '0' && c <= '9') {
        return 52 + (c - '0');
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

static unsigned int uu_val(int c) {
    return (unsigned int)((c - 32) & 0x3f);
}

static int decode_uu(FILE *in, FILE *out) {
    char line[512];
    int saw_end = 0;
    while (fgets(line, sizeof(line), in) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strcmp(line, "end") == 0) {
            saw_end = 1;
            break;
        }
        if (len == 0) {
            continue;
        }
        unsigned int n = uu_val((unsigned char)line[0]);
        if (n == 0u) {
            continue;
        }
        size_t pos = 1;
        unsigned int written = 0;
        while (written < n && pos < len) {
            unsigned int c0 = uu_val((unsigned char)line[pos]);
            unsigned int c1 =
                (pos + 1 < len) ? uu_val((unsigned char)line[pos + 1]) : 0u;
            unsigned int c2 =
                (pos + 2 < len) ? uu_val((unsigned char)line[pos + 2]) : 0u;
            unsigned int c3 =
                (pos + 3 < len) ? uu_val((unsigned char)line[pos + 3]) : 0u;
            pos += 4;
            if (written < n) {
                (void)fputc((int)((c0 << 2) | (c1 >> 4)), out);
                written++;
            }
            if (written < n) {
                (void)fputc((int)(((c1 & 0x0fu) << 4) | (c2 >> 2)), out);
                written++;
            }
            if (written < n) {
                (void)fputc((int)(((c2 & 0x03u) << 6) | c3), out);
                written++;
            }
        }
    }
    if (!saw_end) {
        (void)fprintf(stderr, "uudecode: missing end line\n");
        return 1;
    }
    return ferror(out) ? 1 : 0;
}

static int decode_base64(FILE *in, FILE *out) {
    char line[512];
    int saw_end = 0;
    int quad[4];
    int qlen = 0;
    while (fgets(line, sizeof(line), in) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strcmp(line, "====") == 0) {
            saw_end = 1;
            break;
        }
        for (size_t i = 0; i < len; i++) {
            unsigned char ch = (unsigned char)line[i];
            if (isspace(ch)) {
                continue;
            }
            if (ch == '=') {
                quad[qlen++] = -2;
            } else {
                int v = b64_val(ch);
                if (v < 0) {
                    continue;
                }
                quad[qlen++] = v;
            }
            if (qlen == 4) {
                if (quad[0] >= 0 && quad[1] >= 0) {
                    (void)fputc((quad[0] << 2) | (quad[1] >> 4), out);
                    if (quad[2] >= 0) {
                        (void)fputc(((quad[1] & 0x0f) << 4) | (quad[2] >> 2),
                                    out);
                        if (quad[3] >= 0) {
                            (void)fputc(((quad[2] & 0x03) << 6) | quad[3], out);
                        }
                    }
                }
                qlen = 0;
            }
        }
    }
    if (!saw_end) {
        (void)fprintf(stderr, "uudecode: missing ==== terminator\n");
        return 1;
    }
    return ferror(out) ? 1 : 0;
}

int main(int argc, char **argv) {
    const char *out_override = NULL;
    int opt = 0;
    while ((opt = getopt(argc, argv, "o:")) != -1) {
        if (opt == 'o') {
            out_override = optarg;
        } else {
            (void)fprintf(stderr, "usage: uudecode [-o outfile] [file]\n");
            return 1;
        }
    }

    FILE *in = stdin;
    if (optind < argc && strcmp(argv[optind], "-") != 0) {
        in = fopen(argv[optind], "rb");
        if (!in) {
            perror(argv[optind]);
            return 1;
        }
    }

    char line[512];
    int is_b64 = 0;
    int found_hdr = 0;
    unsigned long mode = 0644ul;
    char hdr_name[512];
    hdr_name[0] = '\0';

    while (fgets(line, sizeof(line), in) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        const char *rest = NULL;
        if (strncmp(line, "begin-base64 ", 13) == 0) {
            is_b64 = 1;
            rest = line + 13;
        } else if (strncmp(line, "begin ", 6) == 0) {
            is_b64 = 0;
            rest = line + 6;
        } else {
            continue;
        }
        char *end = NULL;
        mode = strtoul(rest, &end, 8);
        if (!end || *end != ' ') {
            continue;
        }
        while (*end == ' ') {
            end++;
        }
        (void)snprintf(hdr_name, sizeof(hdr_name), "%s", end);
        found_hdr = 1;
        break;
    }

    if (!found_hdr) {
        (void)fprintf(stderr, "uudecode: no begin line found\n");
        if (in != stdin) {
            (void)fclose(in);
        }
        return 1;
    }

    const char *target = out_override ? out_override : hdr_name;
    FILE *out = stdout;
    int close_out = 0;
    if (strcmp(target, "/dev/stdout") != 0 && strcmp(target, "-") != 0) {
        out = fopen(target, "wb");
        if (!out) {
            perror(target);
            if (in != stdin) {
                (void)fclose(in);
            }
            return 1;
        }
        close_out = 1;
    }

    int rc = is_b64 ? decode_base64(in, out) : decode_uu(in, out);
    if (close_out) {
        (void)fclose(out);
        (void)chmod(target, (mode_t)(mode & 0777ul));
    }
    if (in != stdin) {
        (void)fclose(in);
    }
    return rc;
}
