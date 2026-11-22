/**
 * @file     df.c
 * @brief    POSIX.1-2024 XCU df utility (report free disk space)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

/* need:: COMP_IPC_0017 */

static const char *fs_name_for_id(unsigned long fsid) {
    if (fsid == 2UL) {
        return "bootfs";
    }
    if (fsid == 3UL) {
        return "ext2";
    }
    return "tmpfs";
}

static int report_path(const char *path, int use_kib) {
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) {
        perror(path);
        return 1;
    }
    unsigned long unit = use_kib ? 1024UL : 512UL;
    unsigned long frsize = sv.f_frsize ? sv.f_frsize : 4096UL;
    unsigned long mult = frsize / unit;
    if (mult == 0UL) {
        mult = 1UL;
    }
    unsigned long total = (unsigned long)sv.f_blocks * mult;
    unsigned long avail = (unsigned long)sv.f_bavail * mult;
    unsigned long free_b = (unsigned long)sv.f_bfree * mult;
    unsigned long used = (total > free_b) ? (total - free_b) : 0UL;
    unsigned long denom = used + avail;
    unsigned long pct =
        denom > 0UL ? ((used * 100UL + denom - 1UL) / denom) : 0UL;

    (void)printf("%-10s %10lu %10lu %10lu %3lu%% %s\n",
                 fs_name_for_id(sv.f_fsid), total, used, avail, pct, path);
    return 0;
}

int main(int argc, char **argv) {
    int use_kib = 0;
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "kP")) != -1) {
        if (c == 'k') {
            use_kib = 1;
        } else if (c != 'P') {
            (void)fprintf(stderr, "df: invalid option -%c\n", optopt);
            return 1;
        }
    }

    (void)printf("%-10s %10s %10s %10s %4s %s\n", "Filesystem",
                 use_kib ? "1024-blocks" : "512-blocks", "Used", "Available",
                 "Cap", "Mounted on");

    if (optind >= argc) {
        static const char *k_mounts[] = {"/", "/boot", "/tmp", "/mnt"};
        for (size_t i = 0; i < sizeof(k_mounts) / sizeof(k_mounts[0]); i++) {
            struct statvfs sv;
            if (statvfs(k_mounts[i], &sv) == 0) {
                (void)report_path(k_mounts[i], use_kib);
            }
        }
        return 0;
    }

    int rc = 0;
    for (int i = optind; i < argc; i++) {
        if (report_path(argv[i], use_kib) != 0) {
            rc = 1;
        }
    }
    return rc;
}
