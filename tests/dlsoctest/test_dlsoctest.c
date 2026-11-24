/**
 * @file     test_dlsoctest.c
 * @brief    QEMU test for kernel PT_INTERP loading, SysV auxv, and /boot/ld.so
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: VER_PROC_0011 VER_LIBC_0032 */

/* Exit codes of dlsoctest_dyn (see dlsoctest_dyn.c). */
#define DYN_EXIT_AUXV 10
#define DYN_EXIT_VALUE 11
#define DYN_EXIT_ADD 12
#define DYN_EXIT_DATA 13
#define DYN_EXIT_INIT 14
#define DYN_EXIT_WX 15

static int copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) {
        return -1;
    }
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out < 0) {
        (void)close(in);
        return -1;
    }
    char buf[512];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        ssize_t done = 0;
        while (done < r) {
            ssize_t w = write(out, buf + done, (size_t)(r - done));
            if (w <= 0) {
                (void)close(in);
                (void)close(out);
                return -1;
            }
            done += w;
        }
    }
    (void)close(in);
    (void)close(out);
    return r < 0 ? -1 : 0;
}

static int run_dyn(char *const envp[], int expect_exit) {
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] dlsoctest fork failed errno=%d\n", errno);
        return -1;
    }
    if (pid == 0) {
        char *const args[] = {"dlsoctest_dyn", 0};
        (void)execve("dlsoctest_dyn", args, envp);
        _exit(200); /* execve must not return */
    }
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        printf("[FAIL] dlsoctest dyn child st=0x%x\n", st);
        return -1;
    }
    int code = WEXITSTATUS(st);
    if (code != expect_exit) {
        printf("[FAIL] dlsoctest dyn exit=%d expected=%d\n", code, expect_exit);
        return -1;
    }
    return 0;
}

int main(void) {
    /* The decoy /boot/libmtest.so exports value 1; ld.so must only pick it
     * when LD_LIBRARY_PATH does not shadow it. */
    struct stat st;
    if (stat("/boot/libmtest.so", &st) != 0) {
        printf("[FAIL] dlsoctest /boot/libmtest.so missing\n");
        return 1;
    }
    if (stat("/boot/libmtest_alt.so", &st) != 0) {
        printf("[FAIL] dlsoctest /boot/libmtest_alt.so missing\n");
        return 1;
    }
    if (mkdir("/tmp/libs", 0755) != 0 && errno != EEXIST) {
        printf("[FAIL] dlsoctest mkdir /tmp/libs errno=%d\n", errno);
        return 1;
    }
    if (copy_file("/boot/libmtest_alt.so", "/tmp/libs/libmtest.so") != 0) {
        printf("[FAIL] dlsoctest copy to /tmp/libs failed errno=%d\n", errno);
        return 1;
    }
    /* Without LD_LIBRARY_PATH the search falls back to /boot (value 1), so
     * the dynamic executable must reject it with DYN_EXIT_VALUE. */
    char *const no_env[] = {0};
    if (run_dyn(no_env, DYN_EXIT_VALUE) != 0) {
        return 1;
    }

    /* With LD_LIBRARY_PATH the /tmp/libs variant (value 2) wins; the dynamic
     * linker then applies relocations, runs DT_INIT_ARRAY, and checks W^X. */
    char *const lib_env[] = {"LD_LIBRARY_PATH=/tmp/libs", 0};
    if (run_dyn(lib_env, 0) != 0) {
        return 1;
    }

    printf("[OK] dlsoctest PT_INTERP ld.so, auxv, relocations, DT_INIT_ARRAY, "
           "and LD_LIBRARY_PATH search verified\n");
    return 0;
}
