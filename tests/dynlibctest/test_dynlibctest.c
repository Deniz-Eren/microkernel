/**
 * @file     test_dynlibctest.c
 * @brief    QEMU test driver for the shared libc.so dynamic executable
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: VER_LIBC_0033 */

int main(void) {
    struct stat st;
    const char *imgs[] = {"/boot/libc.so", "/boot/libdldyn.so",
                          "/boot/dynlibctest_dyn", "/boot/ld.so"};
    for (size_t i = 0; i < sizeof(imgs) / sizeof(imgs[0]); i++) {
        if (stat(imgs[i], &st) != 0) {
            printf("[FAIL] dynlibctest %s missing\n", imgs[i]);
            return 1;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] dynlibctest fork failed errno=%d\n", errno);
        return 1;
    }
    if (pid == 0) {
        char *const args[] = {"dynlibctest_dyn", 0};
        char *const envp[] = {0};
        (void)execve("dynlibctest_dyn", args, envp);
        _exit(200); /* execve must not return */
    }
    int wst = 0;
    if (waitpid(pid, &wst, 0) != pid || !WIFEXITED(wst)) {
        printf("[FAIL] dynlibctest dyn child st=0x%x\n", wst);
        return 1;
    }
    int code = WEXITSTATUS(wst);
    if (code != 0) {
        printf("[FAIL] dynlibctest dyn exit=%d (10 stdio, 11 malloc, "
               "12 dlopen, 13 dlsym, 14 thread, 15 signal, 16 proc, "
               "17/18 dlsym detail, 19 ctor)\n",
               code);
        return 1;
    }
    printf("[OK] dynlibctest driver verified dynamic libc.so handoff\n");
    return 0;
}
