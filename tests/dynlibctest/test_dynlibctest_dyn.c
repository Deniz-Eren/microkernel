/**
 * @file     test_dynlibctest_dyn.c
 * @brief    Dynamic executable linked against shared libc.so (PT_INTERP)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0033 */

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Distinctive exit codes printed by the dynlibctest driver on failure. */
#define DYN_EXIT_STDIO 10
#define DYN_EXIT_MALLOC 11
#define DYN_EXIT_DLOPEN 12
#define DYN_EXIT_DLSYM 13
#define DYN_EXIT_THREAD 14
#define DYN_EXIT_SIGNAL 15
#define DYN_EXIT_PROCGEN 16
#define DYN_EXIT_DLSYM_NULL 17
#define DYN_EXIT_DLSYM_VAL 18
#define DYN_EXIT_CTOR 19

#define AT_BASE 7UL

static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_worker_incr;
static volatile sig_atomic_t g_sig_flag;

/* Runs through the executable's DT_INIT_ARRAY: ld.so defers it and
 * __libc_start_main iterates __init_array_start after TLS setup. */
static int g_ctor_flag;

__attribute__((constructor)) static void dyn_ctor(void) {
    g_ctor_flag = 1;
}

/* The worker mutates errno and verifies it stays thread-local: the main
 * thread must keep observing its own errno slot after the join. */
static void *worker(void *arg) {
    (void)arg;
    errno = 99;
    for (long i = 0; i < 100; i++) {
        (void)pthread_mutex_lock(&g_mx);
        g_worker_incr++;
        (void)pthread_mutex_unlock(&g_mx);
    }
    if (errno != 99) {
        return (void *)1;
    }
    return 0;
}

static void handler(int sig) {
    g_sig_flag = sig;
}

int main(int argc, char **argv) {
    /* Re-exec target for the fork/execve check: exit with a distinctive
     * code so the parent can verify the full exec chain. */
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        return 7;
    }

    /* The DT_INIT_ARRAY constructor ran inside __libc_start_main. */
    if (!g_ctor_flag) {
        return DYN_EXIT_CTOR;
    }

    /* stdio: buffered console output and formatted strings. */
    printf("[OK] dynlibc stdio printf on shared libc.so\n");
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d+%d=%d", 20, 22, 20 + 22);
    if (n != 8 || strcmp(buf, "20+22=42") != 0) {
        return DYN_EXIT_STDIO;
    }

    /* malloc: mmap-backed heap through the shared library. */
    char *p = malloc(1024);
    if (!p) {
        return DYN_EXIT_MALLOC;
    }
    memset(p, 0x5a, 1024);
    char *rp = realloc(p, 2048);
    if (!rp) {
        free(p);
        return DYN_EXIT_MALLOC;
    }
    p = rp;
    if (p[0] != 0x5a || p[1023] != 0x5a) {
        free(p);
        return DYN_EXIT_MALLOC;
    }
    free(p);
    long *q = calloc(16, sizeof(long));
    if (!q) {
        return DYN_EXIT_MALLOC;
    }
    if (q[0] != 0 || q[15] != 0) {
        free(q);
        return DYN_EXIT_MALLOC;
    }
    free(q);
    printf("[OK] dynlibc malloc, realloc, calloc, and free\n");

    /* dlopen: runtime load through /boot/ld.so delegation, symbol
     * resolution, data symbols, constructors, and dlclose. */
    errno = 0;
    void *h = dlopen("libdldyn.so", RTLD_NOW);
    if (!h) {
        printf("[FAIL] dynlibc dlopen: %s\n", dlerror());
        return DYN_EXIT_DLOPEN;
    }
    int (*val)(void) = (int (*)(void))dlsym(h, "dltest_value");
    int (*add)(int, int) = (int (*)(int, int))dlsym(h, "dltest_add");
    int *data = (int *)dlsym(h, "dltest_data");
    int (*cnt)(void) = (int (*)(void))dlsym(h, "dltest_init_count");
    if (!val || !add || !data || !cnt) {
        printf("[FAIL] dynlibc dlsym: %s\n", dlerror());
        return DYN_EXIT_DLSYM_NULL;
    }
    if (val() != 42 || add(20, 22) != 42 || *data != 41 || cnt() < 1) {
        return DYN_EXIT_DLSYM_VAL;
    }
    if (dlclose(h) != 0 || dlerror() != NULL) {
        return DYN_EXIT_DLOPEN;
    }
    if (dlerror() != NULL) {
        return DYN_EXIT_DLOPEN;
    }
    printf("[OK] dynlibc dlopen, dlsym, dlclose, and dlerror\n");

    /* pthreads + TLS errno: the shared library TCB carries per-thread
     * errno slots addressed through FS.base. */
    errno = 42;
    pthread_t th;
    void *ret = (void *)1;
    if (pthread_create(&th, 0, worker, 0) != 0) {
        return DYN_EXIT_THREAD;
    }
    if (pthread_join(th, &ret) != 0 || ret != 0) {
        return DYN_EXIT_THREAD;
    }
    if (g_worker_incr != 100 || errno != 42) {
        return DYN_EXIT_THREAD;
    }
    printf("[OK] dynlibc pthread join, mutex, and TLS errno isolation\n");

    /* signals: handler dispatch through the crt0 sigreturn trampoline with
     * FS.base TLS preserved across the kernel signal frame. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if (sigaction(SIGUSR1, &sa, 0) != 0) {
        return DYN_EXIT_SIGNAL;
    }
    errno = 7;
    if (raise(SIGUSR1) != 0 || g_sig_flag != SIGUSR1 || errno != 7) {
        return DYN_EXIT_SIGNAL;
    }
    printf("[OK] dynlibc SIGUSR1 handler with errno preserved\n");

    /* fork/execve: the child re-executes this dynamic image (the kernel
     * reloads PT_INTERP ld.so and libc.so) and exits with code 7. */
    pid_t pid = fork();
    if (pid < 0) {
        return DYN_EXIT_PROCGEN;
    }
    if (pid == 0) {
        char *const args[] = {"dynlibctest_dyn", "child", 0};
        char *const envp[] = {0};
        (void)execve("dynlibctest_dyn", args, envp);
        _exit(200); /* execve must not return */
    }
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 7) {
        return DYN_EXIT_PROCGEN;
    }
    printf("[OK] dynlibc fork, execve re-exec, and waitpid\n");

    printf("[OK] dynlibctest shared libc.so stdio, malloc, pthreads, "
           "TLS errno, signals, fork/execve, and dlopen verified\n");
    return 0;
}
