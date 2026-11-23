/**
 * @file     file.c
 * @brief    Ring 3 file type classification utility targeting POSIX.1-2024 XCU
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* need:: COMP_LIBC_0023 */

static uint16_t read_u16(const unsigned char *p, int be) {
    if (be) {
        return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
    }
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void describe_elf(const unsigned char *buf, size_t n, const char *path) {
    const char *cls = "unknown-class";
    if (n > 4) {
        if (buf[4] == 1) {
            cls = "32-bit";
        } else if (buf[4] == 2) {
            cls = "64-bit";
        }
    }
    int be = (n > 5 && buf[5] == 2) ? 1 : 0;
    const char *end = (n > 5 && buf[5] == 1) ? "LSB" : (be ? "MSB" : "unknown");
    const char *kind = "object";
    const char *arch = "unknown";
    if (n >= 20) {
        uint16_t et = read_u16(buf + 16, be);
        uint16_t em = read_u16(buf + 18, be);
        if (et == 1) {
            kind = "relocatable";
        } else if (et == 2) {
            kind = "executable";
        } else if (et == 3) {
            kind = "shared object";
        } else if (et == 4) {
            kind = "core file";
        }
        if (em == 62) {
            arch = "x86-64";
        } else if (em == 183) {
            arch = "aarch64";
        } else if (em == 3) {
            arch = "Intel 80386";
        }
    }
    (void)printf("%s: ELF %s %s %s, %s\n", path, cls, end, kind, arch);
}

static int classify_content(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        (void)printf("%s: cannot open (%s)\n", path, strerror(errno));
        return 1;
    }
    unsigned char buf[512];
    ssize_t nr = read(fd, buf, sizeof(buf));
    (void)close(fd);
    if (nr < 0) {
        (void)printf("%s: cannot read (%s)\n", path, strerror(errno));
        return 1;
    }
    size_t n = (size_t)nr;
    if (n == 0) {
        (void)printf("%s: empty\n", path);
        return 0;
    }
    if (n >= 4 && buf[0] == 0x7fu && buf[1] == 'E' && buf[2] == 'L' &&
        buf[3] == 'F') {
        describe_elf(buf, n, path);
        return 0;
    }
    if (n >= 263 && memcmp(buf + 257, "ustar", 5) == 0) {
        (void)printf("%s: POSIX tar archive (ustar)\n", path);
        return 0;
    }
    if (n >= 6 &&
        (memcmp(buf, "070707", 6) == 0 || memcmp(buf, "070701", 6) == 0 ||
         memcmp(buf, "070702", 6) == 0)) {
        (void)printf("%s: ASCII cpio archive\n", path);
        return 0;
    }
    if (n >= 8 && memcmp(buf, "!<arch>\n", 8) == 0) {
        (void)printf("%s: current ar archive\n", path);
        return 0;
    }
    if (n >= 2 && buf[0] == '#' && buf[1] == '!') {
        char line[128];
        size_t j = 0;
        for (size_t i = 2; i < n && buf[i] != '\n' && j + 1 < sizeof(line);
             i++) {
            line[j++] = (char)buf[i];
        }
        line[j] = '\0';
        if (strstr(line, "sh") != NULL) {
            (void)printf("%s: POSIX shell script text\n", path);
        } else {
            (void)printf("%s: script text\n", path);
        }
        return 0;
    }

    int high = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = buf[i];
        if (c == 0u ||
            (c < 0x20u && c != '\t' && c != '\n' && c != '\r' && c != '\f' &&
             c != '\v' && c != '\b' && c != 0x1bu) ||
            c == 0x7fu) {
            (void)printf("%s: data\n", path);
            return 0;
        }
        if (c >= 0x80u) {
            high = 1;
        }
    }

    char text[513];
    (void)memcpy(text, buf, n);
    text[n] = '\0';
    if (strncmp(text, "/*", 2) == 0 || strncmp(text, "//", 2) == 0 ||
        strstr(text, "#include ") != NULL ||
        strstr(text, "int main(") != NULL) {
        (void)printf("%s: C program text\n", path);
    } else if (high) {
        (void)printf("%s: UTF-8 text\n", path);
    } else {
        (void)printf("%s: ASCII text\n", path);
    }
    return 0;
}

static int inspect_path(const char *path, int follow, int no_content) {
    struct stat st;
    int st_rc = follow ? stat(path, &st) : lstat(path, &st);
    int broken_link = 0;
    if (st_rc != 0 && follow && lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
        st_rc = 0;
        broken_link = 1;
    }
    if (st_rc != 0) {
        (void)printf("%s: cannot open (%s)\n", path, strerror(errno));
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        (void)printf("%s: directory\n", path);
        return 0;
    }
    if (S_ISLNK(st.st_mode)) {
        char target[512];
        ssize_t rl = readlink(path, target, sizeof(target) - 1);
        if (rl >= 0) {
            target[rl] = '\0';
            (void)printf("%s: %ssymbolic link to %s\n", path,
                         broken_link ? "broken " : "", target);
        } else {
            (void)printf("%s: symbolic link\n", path);
        }
        return 0;
    }
    if (S_ISFIFO(st.st_mode)) {
        (void)printf("%s: fifo (named pipe)\n", path);
        return 0;
    }
    if (S_ISCHR(st.st_mode)) {
        (void)printf("%s: character special\n", path);
        return 0;
    }
    if (S_ISBLK(st.st_mode)) {
        (void)printf("%s: block special\n", path);
        return 0;
    }
    if (S_ISSOCK(st.st_mode)) {
        (void)printf("%s: socket\n", path);
        return 0;
    }
    if (S_ISREG(st.st_mode)) {
        if (st.st_size == 0) {
            (void)printf("%s: empty\n", path);
            return 0;
        }
        if (no_content) {
            (void)printf("%s: regular file\n", path);
            return 0;
        }
        return classify_content(path);
    }
    (void)printf("%s: special file\n", path);
    return 0;
}

int main(int argc, char **argv) {
    int follow = 1;
    int no_content = 0;
    int opt = 0;

    while ((opt = getopt(argc, argv, "dhLim:M:")) != -1) {
        switch (opt) {
        case 'd':
        case 'm':
        case 'M':
            break;
        case 'h':
            follow = 0;
            break;
        case 'L':
            follow = 1;
            break;
        case 'i':
            no_content = 1;
            break;
        default:
            (void)fprintf(stderr, "usage: file [-dhLi] file...\n");
            return 1;
        }
    }

    if (optind >= argc) {
        (void)fprintf(stderr, "usage: file [-dhLi] file...\n");
        return 1;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (inspect_path(argv[i], follow, no_content) != 0) {
            rc = 1;
        }
    }
    return rc;
}
