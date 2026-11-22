/**
 * @file     test_ext2.c
 * @brief    Ring 3 /mnt ext2 filesystem and cross-reboot persistence test
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_IPC_0009 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* 16 KiB (16 blocks) exercises both direct and single-indirect ext2 blocks. */
#define BIG_FILE_SIZE (16u * 1024u)

static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t done = 0u;
    while (done < len) {
        ssize_t n = write(fd, p + done, len - done);
        if (n <= 0) {
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    size_t done = 0u;
    while (done < len) {
        ssize_t n = read(fd, p + done, len - done);
        if (n <= 0) {
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

static void fill_pattern(uint8_t *buf, size_t len, uint32_t gen) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)((i * 131u + gen * 29u + 17u) & 0xFFu);
    }
}

int main(void) {
    struct stat st = {0};
    if (stat("/mnt", &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("[FAIL] ext2test: /mnt mountpoint missing\n");
        return 1;
    }

    static uint8_t big_w[BIG_FILE_SIZE];
    static uint8_t big_r[BIG_FILE_SIZE];

    /* 1. Check whether persistent files from a previous boot exist on /mnt. */
    uint32_t prev_gen = 0u;
    int cfd = open("/mnt/persist/counter.txt", O_RDONLY);
    if (cfd >= 0) {
        char line[64] = {0};
        ssize_t rn = read(cfd, line, sizeof(line) - 1u);
        (void)close(cfd);
        if (rn > 4 && memcmp(line, "gen=", 4u) == 0) {
            prev_gen = (uint32_t)strtoul(line + 4, NULL, 10);
        }
        int bfd = open("/mnt/persist/big.bin", O_RDONLY);
        if (bfd < 0 || read_all(bfd, big_r, BIG_FILE_SIZE) != 0) {
            if (bfd >= 0) {
                (void)close(bfd);
            }
            printf("[FAIL] ext2test: persistent big.bin read failed\n");
            return 1;
        }
        (void)close(bfd);
        fill_pattern(big_w, BIG_FILE_SIZE, prev_gen);
        if (memcmp(big_w, big_r, BIG_FILE_SIZE) != 0) {
            printf("[FAIL] ext2test: persistent big.bin content mismatch\n");
            return 1;
        }
        printf("[OK] ext2test verified cross-reboot /mnt file and dir"
               " persistence (gen=%u)\n",
               prev_gen);
    }

    /* 2. Create /mnt/persist (if absent) and write updated persistent files. */
    if (mkdir("/mnt/persist", 0755u) != 0 && errno != EEXIST) {
        printf("[FAIL] ext2test: mkdir /mnt/persist failed (%d)\n", errno);
        return 1;
    }
    uint32_t next_gen = prev_gen + 1u;

    int wfd =
        open("/mnt/persist/counter.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644u);
    if (wfd < 0) {
        printf("[FAIL] ext2test: open counter.txt failed (%d)\n", errno);
        return 1;
    }
    char out[32] = {0};
    int olen = snprintf(out, sizeof(out), "gen=%u\n", next_gen);
    if (olen <= 0 || write_all(wfd, out, (size_t)olen) != 0) {
        (void)close(wfd);
        printf("[FAIL] ext2test: write counter.txt failed\n");
        return 1;
    }
    (void)close(wfd);

    fill_pattern(big_w, BIG_FILE_SIZE, next_gen);
    int bfd = open("/mnt/persist/big.bin", O_CREAT | O_RDWR | O_TRUNC, 0644u);
    if (bfd < 0 || write_all(bfd, big_w, BIG_FILE_SIZE) != 0) {
        if (bfd >= 0) {
            (void)close(bfd);
        }
        printf("[FAIL] ext2test: write 16 KiB big.bin failed\n");
        return 1;
    }
    if (lseek(bfd, 0, SEEK_SET) != 0 ||
        read_all(bfd, big_r, BIG_FILE_SIZE) != 0 ||
        memcmp(big_w, big_r, BIG_FILE_SIZE) != 0) {
        (void)close(bfd);
        printf("[FAIL] ext2test: 16 KiB big.bin readback mismatch\n");
        return 1;
    }
    (void)close(bfd);

    /* 3. Verify ftruncate, rename, EXDEV across filesystems, unlink, rmdir. */
    int sfd =
        open("/mnt/persist/scratch.txt", O_CREAT | O_RDWR | O_TRUNC, 0644u);
    if (sfd < 0 || write_all(sfd, "0123456789abcdef", 16u) != 0 ||
        ftruncate(sfd, 6) != 0 || fstat(sfd, &st) != 0 || st.st_size != 6) {
        if (sfd >= 0) {
            (void)close(sfd);
        }
        printf("[FAIL] ext2test: ftruncate on /mnt failed\n");
        return 1;
    }
    (void)close(sfd);

    if (rename("/mnt/persist/scratch.txt", "/mnt/persist/renamed.txt") != 0) {
        printf("[FAIL] ext2test: rename within /mnt failed\n");
        return 1;
    }
    if (stat("/mnt/persist/scratch.txt", &st) == 0 ||
        stat("/mnt/persist/renamed.txt", &st) != 0 || st.st_size != 6) {
        printf("[FAIL] ext2test: post-rename stat mismatch\n");
        return 1;
    }

    /* Cross-device rename (/mnt -> /tmp) must fail with EXDEV. */
    errno = 0;
    if (rename("/mnt/persist/renamed.txt", "/tmp/xdev.txt") == 0 ||
        errno != EXDEV) {
        printf("[FAIL] ext2test: cross-fs rename did not return EXDEV\n");
        return 1;
    }

    /* Unlinked-while-open persistence on ext2. */
    int ufd = open("/mnt/persist/renamed.txt", O_RDONLY);
    if (ufd < 0 || unlink("/mnt/persist/renamed.txt") != 0) {
        if (ufd >= 0) {
            (void)close(ufd);
        }
        printf("[FAIL] ext2test: unlink renamed.txt failed\n");
        return 1;
    }
    char kept[8] = {0};
    if (read(ufd, kept, 6u) != 6 || memcmp(kept, "012345", 6u) != 0) {
        (void)close(ufd);
        printf("[FAIL] ext2test: unlinked open file read failed\n");
        return 1;
    }
    (void)close(ufd);

    /* Temporary subdirectory create + readdir + rmdir. */
    if (mkdir("/mnt/persist/sub", 0755u) != 0 ||
        rmdir("/mnt/persist/sub") != 0) {
        printf("[FAIL] ext2test: mkdir/rmdir on /mnt failed\n");
        return 1;
    }

    DIR *d = opendir("/mnt/persist");
    int saw_counter = 0;
    int saw_big = 0;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, "counter.txt") == 0) {
                saw_counter = 1;
            } else if (strcmp(de->d_name, "big.bin") == 0) {
                saw_big = 1;
            }
        }
        (void)closedir(d);
    }
    if (!saw_counter || !saw_big) {
        printf("[FAIL] ext2test: readdir /mnt/persist missing entries\n");
        return 1;
    }

    printf("[OK] ext2test /mnt create, multi-block write/read, truncate,"
           " rename, and unlink verified (gen=%u)\n",
           next_gen);
    return 0;
}
