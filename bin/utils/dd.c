/**
 * @file     dd.c
 * @brief    POSIX.1-2024 XCU dd utility (convert and copy a file)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static int parse_size(const char *s, size_t *out) {
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno != 0 || end == s) {
        return -1;
    }
    if (*end == 'k' || *end == 'K') {
        v *= 1024UL;
        end++;
    } else if (*end == 'b' || *end == 'B') {
        v *= 512UL;
        end++;
    }
    if (*end != '\0') {
        return -1;
    }
    *out = (size_t)v;
    return 0;
}

int main(int argc, char **argv) {
    const char *if_path = NULL;
    const char *of_path = NULL;
    size_t ibs = 512u;
    size_t obs = 512u;
    size_t count = 0u;
    int has_count = 0;
    size_t skip = 0u;
    size_t seek = 0u;
    int conv_notrunc = 0;
    int conv_sync = 0;
    int conv_lcase = 0;
    int conv_ucase = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strncmp(arg, "if=", 3u) == 0) {
            if_path = arg + 3;
        } else if (strncmp(arg, "of=", 3u) == 0) {
            of_path = arg + 3;
        } else if (strncmp(arg, "bs=", 3u) == 0) {
            size_t bs = 0u;
            if (parse_size(arg + 3, &bs) != 0 || bs == 0u) {
                (void)fprintf(stderr, "dd: invalid bs\n");
                return 1;
            }
            ibs = bs;
            obs = bs;
        } else if (strncmp(arg, "ibs=", 4u) == 0) {
            if (parse_size(arg + 4, &ibs) != 0 || ibs == 0u) {
                (void)fprintf(stderr, "dd: invalid ibs\n");
                return 1;
            }
        } else if (strncmp(arg, "obs=", 4u) == 0) {
            if (parse_size(arg + 4, &obs) != 0 || obs == 0u) {
                (void)fprintf(stderr, "dd: invalid obs\n");
                return 1;
            }
        } else if (strncmp(arg, "count=", 6u) == 0) {
            if (parse_size(arg + 6, &count) != 0) {
                (void)fprintf(stderr, "dd: invalid count\n");
                return 1;
            }
            has_count = 1;
        } else if (strncmp(arg, "skip=", 5u) == 0) {
            if (parse_size(arg + 5, &skip) != 0) {
                (void)fprintf(stderr, "dd: invalid skip\n");
                return 1;
            }
        } else if (strncmp(arg, "seek=", 5u) == 0) {
            if (parse_size(arg + 5, &seek) != 0) {
                (void)fprintf(stderr, "dd: invalid seek\n");
                return 1;
            }
        } else if (strncmp(arg, "conv=", 5u) == 0) {
            const char *p = arg + 5;
            while (*p) {
                const char *comma = strchr(p, ',');
                size_t len = comma ? (size_t)(comma - p) : strlen(p);
                if (len == 7u && memcmp(p, "notrunc", 7u) == 0) {
                    conv_notrunc = 1;
                } else if (len == 4u && memcmp(p, "sync", 4u) == 0) {
                    conv_sync = 1;
                } else if (len == 5u && memcmp(p, "lcase", 5u) == 0) {
                    conv_lcase = 1;
                } else if (len == 5u && memcmp(p, "ucase", 5u) == 0) {
                    conv_ucase = 1;
                } else {
                    (void)fprintf(stderr, "dd: unsupported conv\n");
                    return 1;
                }
                if (!comma) {
                    break;
                }
                p = comma + 1;
            }
        } else {
            (void)fprintf(stderr, "dd: invalid operand: %s\n", arg);
            return 1;
        }
    }

    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;
    if (if_path) {
        in_fd = open(if_path, O_RDONLY | O_CLOEXEC);
        if (in_fd < 0) {
            perror(if_path);
            return 1;
        }
    }
    if (of_path) {
        int oflags = O_WRONLY | O_CREAT | O_CLOEXEC;
        if (!conv_notrunc) {
            oflags |= O_TRUNC;
        }
        out_fd = open(of_path, oflags, 0666u);
        if (out_fd < 0) {
            perror(of_path);
            if (if_path) {
                (void)close(in_fd);
            }
            return 1;
        }
    }

    size_t buf_cap = ibs > obs ? ibs : obs;
    unsigned char *buf = (unsigned char *)malloc(buf_cap);
    if (!buf) {
        (void)fprintf(stderr, "dd: out of memory\n");
        if (if_path) {
            (void)close(in_fd);
        }
        if (of_path) {
            (void)close(out_fd);
        }
        return 1;
    }

    for (size_t s = 0u; s < skip; s++) {
        ssize_t r = read(in_fd, buf, ibs);
        if (r <= 0) {
            break;
        }
    }
    if (seek > 0u) {
        (void)lseek(out_fd, (off_t)(seek * obs), SEEK_SET);
    }

    unsigned long in_full = 0UL;
    unsigned long in_part = 0UL;
    unsigned long out_full = 0UL;
    unsigned long out_part = 0UL;

    for (size_t rec = 0u; !has_count || rec < count; rec++) {
        ssize_t n = read(in_fd, buf, ibs);
        if (n < 0) {
            perror("dd: read");
            free(buf);
            if (if_path) {
                (void)close(in_fd);
            }
            if (of_path) {
                (void)close(out_fd);
            }
            return 1;
        }
        if (n == 0) {
            break;
        }
        size_t got = (size_t)n;
        if (got == ibs) {
            in_full++;
        } else {
            in_part++;
            if (conv_sync) {
                memset(buf + got, 0, ibs - got);
                got = ibs;
            }
        }
        if (conv_lcase) {
            for (size_t k = 0; k < got; k++) {
                buf[k] = (unsigned char)tolower(buf[k]);
            }
        } else if (conv_ucase) {
            for (size_t k = 0; k < got; k++) {
                buf[k] = (unsigned char)toupper(buf[k]);
            }
        }

        size_t off = 0u;
        while (off < got) {
            size_t chunk = (got - off > obs) ? obs : (got - off);
            ssize_t w = write(out_fd, buf + off, chunk);
            if (w <= 0) {
                perror("dd: write");
                free(buf);
                if (if_path) {
                    (void)close(in_fd);
                }
                if (of_path) {
                    (void)close(out_fd);
                }
                return 1;
            }
            if ((size_t)w == obs) {
                out_full++;
            } else {
                out_part++;
            }
            off += (size_t)w;
        }
    }

    free(buf);
    if (if_path) {
        (void)close(in_fd);
    }
    if (of_path) {
        (void)close(out_fd);
    }
    (void)fprintf(stderr, "%lu+%lu records in\n%lu+%lu records out\n", in_full,
                  in_part, out_full, out_part);
    return 0;
}
