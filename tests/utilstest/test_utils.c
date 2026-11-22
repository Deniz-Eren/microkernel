/**
 * @file     test_utils.c
 * @brief    QEMU test for POSIX.1-2024 Base headers and standard C utilities
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <fnmatch.h>
#include <iso646.h>
#include <limits.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_LIBC_0008 */

static volatile sig_atomic_t g_alrm_hits;
static int g_atexit_pipe_w = -1;

static void alrm_handler(int sig) {
    (void)sig;
    g_alrm_hits++;
}

static void atexit_a(void) {
    if (g_atexit_pipe_w >= 0) {
        (void)write(g_atexit_pipe_w, "A", 1u);
    }
}

static void atexit_nested(void) {
    if (g_atexit_pipe_w >= 0) {
        (void)write(g_atexit_pipe_w, "N", 1u);
    }
}

static void atexit_b(void) {
    if (g_atexit_pipe_w >= 0) {
        (void)write(g_atexit_pipe_w, "B", 1u);
    }
    (void)atexit(atexit_nested);
}

static void atexit_c(void) {
    if (g_atexit_pipe_w >= 0) {
        (void)write(g_atexit_pipe_w, "C", 1u);
    }
}

static int cmp_int(const void *a, const void *b) {
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return (va > vb) - (va < vb);
}

static int test_uname_endian_sched_times(void) {
    struct utsname u;
    errno = 0;
    if (uname(NULL) != -1 || errno != EFAULT) {
        printf("[FAIL] uname(NULL) expected EFAULT got %d\n", errno);
        return -1;
    }
    if (uname(&u) != 0 || strcmp(u.sysname, "Microkernel") != 0 ||
        strcmp(u.machine, "x86_64") != 0 || u.release[0] == '\0') {
        printf("[FAIL] uname fields invalid\n");
        return -1;
    }

    if (BYTE_ORDER != LITTLE_ENDIAN || htobe16(0x1234u) != 0x3412u ||
        be16toh(0x3412u) != 0x1234u || htole16(0x1234u) != 0x1234u ||
        le16toh(0x1234u) != 0x1234u || htobe32(0x12345678u) != 0x78563412u ||
        be32toh(0x78563412u) != 0x12345678u ||
        htole32(0x12345678u) != 0x12345678u ||
        le32toh(0x12345678u) != 0x12345678u ||
        htobe64(0x0123456789ABCDEFULL) != 0xEFCDAB8967452301ULL ||
        be64toh(0xEFCDAB8967452301ULL) != 0x0123456789ABCDEFULL ||
        htole64(0x0123456789ABCDEFULL) != 0x0123456789ABCDEFULL ||
        le64toh(0x0123456789ABCDEFULL) != 0x0123456789ABCDEFULL) {
        printf("[FAIL] endian.h conversions failed\n");
        return -1;
    }

    if (sched_yield() != 0) {
        printf("[FAIL] sched_yield failed\n");
        return -1;
    }

    struct tms t1 = {0};
    struct tms t2 = {0};
    errno = 0;
    if (times(NULL) != (clock_t)-1 || errno != EFAULT) {
        printf("[FAIL] times(NULL) expected EFAULT got %d\n", errno);
        return -1;
    }
    clock_t c1 = times(&t1);
    struct timespec slp = {.tv_sec = 0, .tv_nsec = 15000000L};
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &slp, NULL);
    clock_t c2 = times(&t2);
    if (c1 == (clock_t)-1 || c2 == (clock_t)-1 || c2 <= c1) {
        printf("[FAIL] times() did not advance (%ld -> %ld)\n", (long)c1,
               (long)c2);
        return -1;
    }

    printf("[OK] uname, endian.h, sched_yield, and times verified\n");
    return 0;
}

static void jump_helper(jmp_buf env, int code) {
    longjmp(env, code);
}

static void sigjump_helper(sigjmp_buf env, int code) {
    siglongjmp(env, code);
}

static int test_setjmp_and_sigsetjmp(void) {
    jmp_buf env;
    volatile int step = 0;
    volatile int kept = 0x1357;
    int r = setjmp(env);
    if (r == 0) {
        step = 1;
        jump_helper(env, 0); /* val=0 must become 1 */
    }
    if (step == 1) {
        if (r != 1 || kept != 0x1357) {
            printf("[FAIL] longjmp(0) expected 1 got %d\n", r);
            return -1;
        }
        step = 2;
        jump_helper(env, 42);
    }
    if (step != 2 || r != 42 || kept != 0x1357) {
        printf("[FAIL] longjmp(42) expected 42 got %d\n", r);
        return -1;
    }

    /* sigsetjmp(savemask=1) must restore signal mask across siglongjmp. */
    sigset_t blk, cur;
    sigemptyset(&blk);
    sigaddset(&blk, SIGUSR1);
    (void)sigprocmask(SIG_UNBLOCK, &blk, NULL);

    sigjmp_buf senv;
    int sr = sigsetjmp(senv, 1);
    if (sr == 0) {
        (void)sigprocmask(SIG_BLOCK, &blk, NULL);
        sigjump_helper(senv, 7);
    }
    sigemptyset(&cur);
    (void)sigprocmask(SIG_SETMASK, NULL, &cur);
    if (sr != 7 || sigismember(&cur, SIGUSR1) != 0) {
        printf("[FAIL] siglongjmp(savemask=1) did not restore mask\n");
        return -1;
    }

    /* sigsetjmp(savemask=0) leaves the modified signal mask intact. */
    sigjmp_buf senv0;
    int sr0 = sigsetjmp(senv0, 0);
    if (sr0 == 0) {
        (void)sigprocmask(SIG_BLOCK, &blk, NULL);
        sigjump_helper(senv0, 9);
    }
    sigemptyset(&cur);
    (void)sigprocmask(SIG_SETMASK, NULL, &cur);
    (void)sigprocmask(SIG_UNBLOCK, &blk, NULL);
    if (sr0 != 9 || sigismember(&cur, SIGUSR1) != 1) {
        printf("[FAIL] siglongjmp(savemask=0) unexpectedly changed mask\n");
        return -1;
    }

    printf("[OK] setjmp/longjmp and sigsetjmp/siglongjmp signal masks "
           "verified\n");
    return 0;
}

static int test_fnmatch_suite(void) {
    if (fnmatch("*.c", "main.c", 0) != 0 ||
        fnmatch("*.c", "main.h", 0) != FNM_NOMATCH ||
        fnmatch("a?c", "abc", 0) != 0 ||
        fnmatch("a?c", "ac", 0) != FNM_NOMATCH ||
        fnmatch("a*b", "a/b", FNM_PATHNAME) != FNM_NOMATCH ||
        fnmatch("a*b", "a/b", 0) != 0 ||
        fnmatch("*.txt", ".secret.txt", FNM_PERIOD) != FNM_NOMATCH ||
        fnmatch(".*.txt", ".secret.txt", FNM_PERIOD) != 0 ||
        fnmatch("a/*.txt", "a/.hid.txt", FNM_PATHNAME | FNM_PERIOD) !=
            FNM_NOMATCH ||
        fnmatch("[a-z][!0-9][[:digit:]]", "k_7", 0) != 0 ||
        fnmatch("[a-z][!0-9][[:digit:]]", "k57", 0) != FNM_NOMATCH ||
        fnmatch("FOO*.C", "foobar.c", FNM_IGNORECASE) != 0 ||
        fnmatch("FOO*.C", "foobar.c", 0) != FNM_NOMATCH ||
        fnmatch("a\\*b", "a*b", 0) != 0 ||
        fnmatch("a\\*b", "a\\xb", FNM_NOESCAPE) != 0 ||
        fnmatch("[abc", "[abc", 0) != 0) {
        printf("[FAIL] fnmatch cases failed\n");
        return -1;
    }

    printf("[OK] fnmatch wildcards, classes, FNM_PATHNAME/PERIOD/IGNORECASE "
           "verified\n");
    return 0;
}

static int test_atexit_qsort_bsearch_strto_getopt(void) {
    /* 1. atexit LIFO + nested registration, and _Exit skipping handlers. */
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] atexit pipe2 failed\n");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        close(pfd[0]);
        g_atexit_pipe_w = pfd[1];
        (void)atexit(atexit_a);
        (void)atexit(atexit_b);
        (void)atexit(atexit_c);
        exit(0);
    }
    close(pfd[1]);
    char seq[8] = {0};
    ssize_t n = 0;
    while (n < (ssize_t)(sizeof(seq) - 1u)) {
        ssize_t got =
            read(pfd[0], seq + n, (size_t)(sizeof(seq) - 1u) - (size_t)n);
        if (got <= 0) {
            break;
        }
        n += got;
    }
    close(pfd[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0 ||
        n != 4 || strcmp(seq, "CBNA") != 0) {
        printf("[FAIL] atexit expected 'CBNA' got '%s' (n=%ld)\n", seq,
               (long)n);
        return -1;
    }

    int pfd2[2] = {-1, -1};
    if (pipe2(pfd2, O_CLOEXEC) != 0) {
        return -1;
    }
    pid_t pid2 = fork();
    if (pid2 < 0) {
        close(pfd2[0]);
        close(pfd2[1]);
        return -1;
    }
    if (pid2 == 0) {
        close(pfd2[0]);
        g_atexit_pipe_w = pfd2[1];
        (void)atexit(atexit_a);
        _Exit(0);
    }
    close(pfd2[1]);
    char seq2[8] = {0};
    ssize_t n2 = read(pfd2[0], seq2, sizeof(seq2) - 1u);
    close(pfd2[0]);
    if (waitpid(pid2, &st, 0) != pid2 || n2 != 0) {
        printf("[FAIL] _Exit ran atexit handler (n2=%ld)\n", (long)n2);
        return -1;
    }

    /* 2. qsort and bsearch. */
    int arr[10] = {9, -3, 4, 4, 0, 12, -100, 7, 1, 4};
    qsort(arr, 10u, sizeof(arr[0]), cmp_int);
    for (size_t i = 1u; i < 10u; i++) {
        if (arr[i - 1u] > arr[i]) {
            printf("[FAIL] qsort output not sorted\n");
            return -1;
        }
    }
    int key_hit = 7;
    int key_miss = 5;
    int *found = (int *)bsearch(&key_hit, arr, 10u, sizeof(arr[0]), cmp_int);
    int *miss = (int *)bsearch(&key_miss, arr, 10u, sizeof(arr[0]), cmp_int);
    if (!found || *found != 7 || miss != NULL) {
        printf("[FAIL] bsearch lookup failed\n");
        return -1;
    }

    /* 3. strtol and strtoul edge cases. */
    char *end = NULL;
    errno = 0;
    long v1 = strtol("  -0x1aZ", &end, 0);
    if (v1 != -26L || !end || *end != 'Z' || errno != 0) {
        printf("[FAIL] strtol hex failed\n");
        return -1;
    }
    errno = 0;
    unsigned long v2 = strtoul("0777!", &end, 0);
    if (v2 != 0777UL || !end || *end != '!' || errno != 0) {
        printf("[FAIL] strtoul octal failed\n");
        return -1;
    }
    errno = 0;
    long v3 = strtol("0x", &end, 0);
    if (v3 != 0L || !end || *end != 'x') {
        printf("[FAIL] strtol '0x' edge failed\n");
        return -1;
    }
    errno = 0;
    long ovf = strtol("999999999999999999999999", &end, 10);
    if (ovf != LONG_MAX || errno != ERANGE) {
        printf("[FAIL] strtol overflow expected ERANGE got %d\n", errno);
        return -1;
    }
    errno = 0;
    (void)strtol("10", &end, 1);
    if (errno != EINVAL) {
        printf("[FAIL] strtol base=1 expected EINVAL got %d\n", errno);
        return -1;
    }

    /* 4. getopt bundled options, arguments, --, and error modes. */
    char *av[] = {"prog", "-ab", "-fval", "-o", "out.txt", "--", "-x", NULL};
    int ac = 7;
    opterr = 0;
    optind = 1;
    int seen_a = 0;
    int seen_b = 0;
    const char *fval = NULL;
    const char *oval = NULL;
    int c;
    while ((c = getopt(ac, av, ":abf:o:")) != -1) {
        if (c == 'a') {
            seen_a++;
        } else if (c == 'b') {
            seen_b++;
        } else if (c == 'f') {
            fval = optarg;
        } else if (c == 'o') {
            oval = optarg;
        } else {
            printf("[FAIL] getopt unexpected option %d\n", c);
            return -1;
        }
    }
    if (seen_a != 1 || seen_b != 1 || !fval || strcmp(fval, "val") != 0 ||
        !oval || strcmp(oval, "out.txt") != 0 || optind != 6 ||
        strcmp(av[optind], "-x") != 0) {
        printf("[FAIL] getopt parsed state mismatch (optind=%d)\n", optind);
        return -1;
    }

    char *av_err[] = {"prog", "-z", "-f", NULL};
    optind = 1;
    int c1 = getopt(3, av_err, ":f:");
    int o1 = optopt;
    int c2 = getopt(3, av_err, ":f:");
    int o2 = optopt;
    if (c1 != '?' || o1 != 'z' || c2 != ':' || o2 != 'f') {
        printf("[FAIL] getopt error handling mismatch (%d/%c, %d/%c)\n", c1, o1,
               c2, o2);
        return -1;
    }

    printf("[OK] atexit LIFO, qsort/bsearch, strtol/strtoul, and getopt "
           "verified\n");
    return 0;
}

static int test_alarm_rename_realpath(void) {
    /* 1. alarm() cancel + SIGALRM delivery. */
    struct sigaction sa = {0};
    sa.sa_handler = alrm_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        printf("[FAIL] sigaction(SIGALRM) failed\n");
        return -1;
    }
    if (alarm(10u) != 0u) {
        printf("[FAIL] initial alarm(10) expected 0\n");
        return -1;
    }
    unsigned int rem = alarm(0u);
    if (rem == 0u || rem > 10u) {
        printf("[FAIL] alarm(0) expected remaining 1..10 got %u\n", rem);
        return -1;
    }
    g_alrm_hits = 0;
    (void)alarm(1u);
    struct timespec wait_ts = {.tv_sec = 1, .tv_nsec = 500000000L};
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &wait_ts, NULL);
    if (g_alrm_hits != 1) {
        printf("[FAIL] alarm(1) expected 1 SIGALRM got %d\n", (int)g_alrm_hits);
        return -1;
    }

    /* 2. rename, renameat, and realpath. */
    if (mkdir("/tmp/ut_d1", 0755) != 0 || mkdir("/tmp/ut_d2", 0755) != 0) {
        printf("[FAIL] mkdir /tmp/ut_d* failed\n");
        return -1;
    }
    int fd =
        open("/tmp/ut_d1/a.txt", O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0 || write(fd, "hello", 5u) != 5) {
        printf("[FAIL] create /tmp/ut_d1/a.txt failed\n");
        if (fd >= 0) {
            close(fd);
        }
        return -1;
    }
    close(fd);

    int fd2 =
        open("/tmp/ut_d2/b.txt", O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0644);
    if (fd2 < 0 || write(fd2, "old", 3u) != 3) {
        if (fd2 >= 0) {
            close(fd2);
        }
        return -1;
    }
    close(fd2);

    /* Cross-directory rename overwriting existing target file. */
    if (rename("/tmp/ut_d1/a.txt", "/tmp/ut_d2/b.txt") != 0) {
        printf("[FAIL] rename cross-dir overwrite failed errno=%d\n", errno);
        return -1;
    }
    struct stat st = {0};
    if (stat("/tmp/ut_d1/a.txt", &st) == 0 ||
        stat("/tmp/ut_d2/b.txt", &st) != 0 || st.st_size != 5) {
        printf("[FAIL] rename result stat mismatch\n");
        return -1;
    }

    /* renameat with open directory FDs + EROFS / ENOTEMPTY checks. */
    int dfd2 = open("/tmp/ut_d2", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    int dfd1 = open("/tmp/ut_d1", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd1 < 0 || dfd2 < 0) {
        if (dfd1 >= 0) {
            close(dfd1);
        }
        if (dfd2 >= 0) {
            close(dfd2);
        }
        return -1;
    }
    if (renameat(dfd2, "b.txt", dfd1, "c.txt") != 0) {
        printf("[FAIL] renameat failed errno=%d\n", errno);
        close(dfd1);
        close(dfd2);
        return -1;
    }
    close(dfd1);
    close(dfd2);

    errno = 0;
    if (rename("/boot/sh", "/tmp/sh_moved") != -1 || errno != EROFS) {
        printf("[FAIL] rename(/boot/sh) expected EROFS got %d\n", errno);
        return -1;
    }
    errno = 0;
    if (rename("/tmp/ut_d2", "/tmp/ut_d1") != -1 || errno != ENOTEMPTY) {
        printf("[FAIL] rename over non-empty dir expected ENOTEMPTY got %d\n",
               errno);
        return -1;
    }

    /* realpath with caller buffer and NULL malloc buffer. */
    char resolved[PATH_MAX];
    char *rp1 = realpath("/tmp/ut_d2/../ut_d1/./c.txt", resolved);
    char *rp2 = realpath("/tmp/ut_d1/c.txt", NULL);
    if (!rp1 || rp1 != resolved || strcmp(rp1, "/tmp/ut_d1/c.txt") != 0 ||
        !rp2 || strcmp(rp2, "/tmp/ut_d1/c.txt") != 0) {
        printf("[FAIL] realpath mismatch ('%s' / '%s')\n", rp1 ? rp1 : "(null)",
               rp2 ? rp2 : "(null)");
        free(rp2);
        return -1;
    }
    free(rp2);

    errno = 0;
    if (realpath("/tmp/no_such_path", resolved) != NULL || errno != ENOENT) {
        printf("[FAIL] realpath missing expected ENOENT got %d\n", errno);
        return -1;
    }

    (void)unlink("/tmp/ut_d1/c.txt");
    (void)rmdir("/tmp/ut_d1");
    (void)rmdir("/tmp/ut_d2");

    printf("[OK] alarm SIGALRM, rename/renameat, and realpath verified\n");
    return 0;
}

int main(void) {
    if (test_uname_endian_sched_times() != 0) {
        return 1;
    }
    if (test_setjmp_and_sigsetjmp() != 0) {
        return 1;
    }
    if (test_fnmatch_suite() != 0) {
        return 1;
    }
    if (test_atexit_qsort_bsearch_strto_getopt() != 0) {
        return 1;
    }
    if (test_alarm_rename_realpath() != 0) {
        return 1;
    }
    return 0;
}
