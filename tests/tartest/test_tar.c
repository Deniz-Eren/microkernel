/**
 * @file     test_tar.c
 * @brief    Ring 3 QEMU verification of sysroot libarchive bsdtar
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0040 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define BASE_DIR "/tmp/tartest"
#define SRC_DIR BASE_DIR "/src"
#define ALPHA_PATH SRC_DIR "/alpha.bin"
#define BETA_PATH SRC_DIR "/sub/beta.bin"
#define GAMMA_PATH SRC_DIR "/gamma.txt"
#define PLAIN_TAR BASE_DIR "/plain.tar"
#define GZ_TAR BASE_DIR "/comp.tar.gz"
#define OUT1_DIR BASE_DIR "/out1"
#define OUT2_DIR BASE_DIR "/out2"

static char g_alpha[4096];
static char g_beta[512];
static const char k_gamma[] = "tar Ring 3 extraction payload line\n";
static char g_buf[1024];

static int run(const char *cmd) {
    int st = system(cmd);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static int capture(const char *cmd, char *out, size_t outsz) {
    FILE *f = popen(cmd, "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(out, 1u, outsz - 1u, f);
    out[n] = '\0';
    int st = pclose(f);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static int write_file(const char *path, const char *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    size_t off = 0u;
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w <= 0) {
            (void)close(fd);
            return -1;
        }
        off += (size_t)w;
    }
    return close(fd);
}

static int check_magic(const char *path, long offset, const char *magic,
                       size_t mlen) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[FAIL] tartest cannot open %s\n", path);
        return -1;
    }
    if (fseek(f, offset, SEEK_SET) != 0) {
        (void)fclose(f);
        printf("[FAIL] tartest cannot seek %s\n", path);
        return -1;
    }
    size_t got = fread(g_buf, 1u, mlen, f);
    (void)fclose(f);
    if (got != mlen || memcmp(g_buf, magic, mlen) != 0) {
        printf("[FAIL] tartest bad magic at %ld in %s\n", offset, path);
        return -1;
    }
    return 0;
}

static int check_file(const char *path, const char *data, size_t len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("[FAIL] tartest cannot open extracted %s\n", path);
        return -1;
    }
    size_t off = 0u;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > sizeof(g_buf)) {
            chunk = sizeof(g_buf);
        }
        ssize_t r = read(fd, g_buf, chunk);
        if (r <= 0 || memcmp(g_buf, data + off, (size_t)r) != 0) {
            (void)close(fd);
            printf("[FAIL] tartest content mismatch in %s at %zu\n", path, off);
            return -1;
        }
        off += (size_t)r;
    }
    if (read(fd, g_buf, 1u) != 0) {
        (void)close(fd);
        printf("[FAIL] tartest trailing data in %s\n", path);
        return -1;
    }
    (void)close(fd);
    return 0;
}

int main(void) {
    char out[4096];

    if (access("/usr/bin/tar", X_OK) != 0) {
        printf("[FAIL] tartest missing /usr/bin/tar\n");
        return 1;
    }

    for (size_t i = 0u; i < sizeof(g_alpha); i++) {
        g_alpha[i] = (char)((i * 31u + (i / 97u)) & 0xFFu);
    }
    for (size_t i = 0u; i < sizeof(g_beta); i++) {
        g_beta[i] = (char)('a' + (i % 26u));
    }

    (void)system("/bin/rm -rf " BASE_DIR);
    if (mkdir(BASE_DIR, 0755) != 0 || mkdir(SRC_DIR, 0755) != 0 ||
        mkdir(SRC_DIR "/sub", 0755) != 0) {
        printf("[FAIL] tartest mkdir failed\n");
        return 1;
    }
    if (write_file(ALPHA_PATH, g_alpha, sizeof(g_alpha)) != 0 ||
        write_file(BETA_PATH, g_beta, sizeof(g_beta)) != 0 ||
        write_file(GAMMA_PATH, k_gamma, sizeof(k_gamma) - 1u) != 0) {
        printf("[FAIL] tartest writing source files failed\n");
        return 1;
    }

    /* Create a plain .tar and verify the ustar magic field. */
    if (run("/usr/bin/tar -cf " PLAIN_TAR " -C " SRC_DIR " .") != 0 ||
        check_magic(PLAIN_TAR, 257, "ustar", 5) != 0) {
        printf("[FAIL] tartest plain.tar creation failed\n");
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    /* List the archive and verify members appear. */
    if (capture("/usr/bin/tar -tf " PLAIN_TAR, out, sizeof(out)) != 0 ||
        strstr(out, "alpha.bin") == NULL ||
        strstr(out, "sub/beta.bin") == NULL) {
        printf("[FAIL] tartest tar -tf listing failed: '%s'\n", out);
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    /* Extract the plain .tar and compare all member contents. */
    if (mkdir(OUT1_DIR, 0755) != 0 ||
        run("/usr/bin/tar -xf " PLAIN_TAR " -C " OUT1_DIR) != 0 ||
        check_file(OUT1_DIR "/alpha.bin", g_alpha, sizeof(g_alpha)) != 0 ||
        check_file(OUT1_DIR "/sub/beta.bin", g_beta, sizeof(g_beta)) != 0 ||
        check_file(OUT1_DIR "/gamma.txt", k_gamma, sizeof(k_gamma) - 1u) != 0) {
        printf("[FAIL] tartest plain.tar extraction failed\n");
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    /* Create a gzip-compressed .tar.gz and verify the gzip magic. */
    if (run("/usr/bin/tar -czf " GZ_TAR " -C " SRC_DIR " .") != 0 ||
        check_magic(GZ_TAR, 0, "\x1f\x8b", 2) != 0) {
        printf("[FAIL] tartest comp.tar.gz creation failed\n");
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    /* Extract the .tar.gz through the gzip filter and compare contents. */
    if (mkdir(OUT2_DIR, 0755) != 0 ||
        run("/usr/bin/tar -xzf " GZ_TAR " -C " OUT2_DIR) != 0 ||
        check_file(OUT2_DIR "/alpha.bin", g_alpha, sizeof(g_alpha)) != 0 ||
        check_file(OUT2_DIR "/sub/beta.bin", g_beta, sizeof(g_beta)) != 0 ||
        check_file(OUT2_DIR "/gamma.txt", k_gamma, sizeof(k_gamma) - 1u) != 0) {
        printf("[FAIL] tartest comp.tar.gz extraction failed\n");
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    /* Examine PT_INTERP and DT_NEEDED resolution with the Ring 3 ldd trace
     * mode (llvm-readelf checks run in the CTest packaging test). */
    if (capture("/usr/bin/ldd /usr/bin/tar", out, sizeof(out)) != 0 ||
        strstr(out, "libz.so.1 =>") == NULL ||
        strstr(out, "libc.so =>") == NULL) {
        printf("[FAIL] tartest ldd /usr/bin/tar failed: '%s'\n", out);
        (void)system("/bin/rm -rf " BASE_DIR);
        return 1;
    }

    (void)system("/bin/rm -rf " BASE_DIR);

    printf("[OK] tar .tar and .tar.gz creation, list, extraction, and ldd "
           "linkage verified\n");
    return 0;
}
