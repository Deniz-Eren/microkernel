/**
 * @file     test_tz.c
 * @brief    Ring 3 QEMU verification of sysroot IANA tzcode and tzdata
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0039 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ZONE_DIR "/tmp/zoneinfo"

static int capture(const char *cmd, char *buf, size_t bufsz) {
    FILE *f = popen(cmd, "r");
    if (!f) {
        return -1;
    }
    size_t n = fread(buf, 1u, bufsz - 1u, f);
    buf[n] = '\0';
    int st = pclose(f);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static int check_tzif_magic(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[FAIL] tztest cannot open %s\n", path);
        return -1;
    }
    char magic[4] = {0, 0, 0, 0};
    size_t got = fread(magic, 1u, 4u, f);
    (void)fclose(f);
    if (got != 4u || memcmp(magic, "TZif", 4u) != 0) {
        printf("[FAIL] tztest bad TZif magic in %s\n", path);
        return -1;
    }
    return 0;
}

int main(void) {
    char buf[4096];

    (void)system("/bin/rm -rf " ZONE_DIR);

    /* Verify standard symlinks and tzselect tables installed by vfsserv. */
    if (access("/usr/sbin/zic", R_OK) != 0 ||
        access("/usr/bin/zdump", R_OK) != 0 ||
        access("/usr/bin/tzselect", R_OK) != 0 ||
        access("/usr/share/zoneinfo/iso3166.tab", R_OK) != 0 ||
        access("/usr/share/zoneinfo/zone1970.tab", R_OK) != 0) {
        printf("[FAIL] tztest missing tzcode symlinks or zoneinfo tables\n");
        return 1;
    }

    /* Compile real upstream IANA tzdata2025b etcetera and australasia files. */
    if (system("/usr/sbin/zic -d " ZONE_DIR " /boot/etcetera") != 0) {
        printf("[FAIL] tztest zic etcetera failed\n");
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }
    if (system("/usr/sbin/zic -d " ZONE_DIR " /boot/australasia") != 0) {
        printf("[FAIL] tztest zic australasia failed\n");
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }

    if (check_tzif_magic(ZONE_DIR "/Etc/UTC") != 0 ||
        check_tzif_magic(ZONE_DIR "/Australia/Sydney") != 0) {
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }

    /* Verify zdump output on real compiled Etc/UTC TZif. */
    if (capture("/usr/bin/zdump -v -c 2024,2026 " ZONE_DIR "/Etc/UTC", buf,
                sizeof(buf)) != 0 ||
        strstr(buf, "UTC") == NULL || strstr(buf, "isdst=0") == NULL ||
        strstr(buf, "gmtoff=0") == NULL) {
        printf("[FAIL] tztest zdump Etc/UTC got '%s'\n", buf);
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }

    /* Verify zdump DST transitions on real compiled Australia/Sydney TZif. */
    if (capture("/usr/bin/zdump -v -c 2024,2026 " ZONE_DIR "/Australia/Sydney",
                buf, sizeof(buf)) != 0 ||
        strstr(buf, "AEDT") == NULL || strstr(buf, "isdst=1") == NULL ||
        strstr(buf, "gmtoff=39600") == NULL || strstr(buf, "AEST") == NULL ||
        strstr(buf, "isdst=0") == NULL || strstr(buf, "gmtoff=36000") == NULL) {
        printf("[FAIL] tztest zdump Australia/Sydney got '%s'\n", buf);
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }

    /* Verify generated tzselect script runs under Ring 3 /bin/sh. */
    if (capture("/bin/sh /usr/bin/tzselect --version", buf, sizeof(buf)) != 0 ||
        strstr(buf, "tzselect (tzcode) 2025b") == NULL) {
        printf("[FAIL] tztest tzselect --version got '%s'\n", buf);
        (void)system("/bin/rm -rf " ZONE_DIR);
        return 1;
    }

    (void)system("/bin/rm -rf " ZONE_DIR);

    printf("[OK] tzcode zic compiled IANA tzdata TZif and zdump/tzselect "
           "verified\n");
    return 0;
}
