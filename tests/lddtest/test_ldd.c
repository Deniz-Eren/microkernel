/**
 * @file     test_ldd.c
 * @brief    QEMU test for /boot/ld.so ldd trace mode and /usr/sbin/ldconfig
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0036 VER_LIBC_0037 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_child(const char *path, char *const args[], char *const envp[]) {
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] lddtest fork failed errno=%d\n", errno);
        return -1;
    }
    if (pid == 0) {
        (void)execve(path, args, envp);
        _exit(200);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        printf("[FAIL] lddtest child st=0x%x\n", st);
        return -1;
    }
    return WEXITSTATUS(st);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        return -1;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0u) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int check_link(const char *path, const char *expected) {
    char target[128];
    ssize_t n = readlink(path, target, sizeof(target) - 1u);
    if (n < 0) {
        printf("[FAIL] lddtest readlink %s errno=%d\n", path, errno);
        return -1;
    }
    target[n] = '\0';
    if (strcmp(target, expected) != 0) {
        printf("[FAIL] lddtest %s -> '%s' (expected '%s')\n", path, target,
               expected);
        return -1;
    }
    return 0;
}

int main(void) {
    char *const no_env[] = {0};
    char *const zlib_args[] = {"ldd", "/boot/zlibtest", 0};
    char *const openssl_args[] = {"ldd", "/boot/openssltest", 0};
    char *const sshd_args[] = {"ldd", "/boot/sshd", 0};
    char *const trace_args[] = {"zlibtest", 0};
    char *const trace_env[] = {"LD_TRACE_LOADED_OBJECTS=1", 0};
    char *const ldconfig_args[] = {"ldconfig", 0};
    char *const ldconfig_tmp_args[] = {"ldconfig", "-n", "/tmp/ldcfgtest", 0};

    if (check_link("/usr/bin/ldd", "/boot/ld.so") != 0 ||
        check_link("/usr/sbin/ldconfig", "/boot/ldconfig") != 0) {
        return 1;
    }

    if (run_child("/boot/ldd", zlib_args, no_env) != 0) {
        printf("[FAIL] lddtest ldd /boot/zlibtest failed\n");
        return 1;
    }
    if (run_child("/boot/ldd", openssl_args, no_env) != 0) {
        printf("[FAIL] lddtest ldd /boot/openssltest failed\n");
        return 1;
    }
    if (run_child("/boot/ldd", sshd_args, no_env) != 0) {
        printf("[FAIL] lddtest ldd /boot/sshd failed\n");
        return 1;
    }
    if (run_child("/boot/zlibtest", trace_args, trace_env) != 0) {
        printf("[FAIL] lddtest LD_TRACE_LOADED_OBJECTS=1 failed\n");
        return 1;
    }
    if (run_child("/boot/ldconfig", ldconfig_args, no_env) != 0) {
        printf("[FAIL] lddtest ldconfig default scan failed\n");
        return 1;
    }

    /* Verify ldconfig parses DT_SONAME and creates the missing symlink in a
     * writable directory, then clean up so stresstest sees zero leaks. */
    if (mkdir("/tmp/ldcfgtest", 0755) != 0 && errno != EEXIST) {
        printf("[FAIL] lddtest mkdir /tmp/ldcfgtest errno=%d\n", errno);
        return 1;
    }
    if (copy_file("/boot/libz.so.1", "/tmp/ldcfgtest/libz.so.1.3.1") != 0) {
        printf("[FAIL] lddtest copy libz.so.1 failed\n");
        return 1;
    }
    if (run_child("/boot/ldconfig", ldconfig_tmp_args, no_env) != 0) {
        printf("[FAIL] lddtest ldconfig -n /tmp/ldcfgtest failed\n");
        return 1;
    }
    int link_ok = check_link("/tmp/ldcfgtest/libz.so.1", "libz.so.1.3.1");
    (void)unlink("/tmp/ldcfgtest/libz.so.1");
    (void)unlink("/tmp/ldcfgtest/libz.so.1.3.1");
    (void)rmdir("/tmp/ldcfgtest");
    if (link_ok != 0) {
        return 1;
    }

    printf("[OK] lddtest ldd over /etc/ld.so.conf, LD_TRACE_LOADED_OBJECTS, "
           "and ldconfig symlinks verified\n");
    return 0;
}
