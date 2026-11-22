/**
 * @file     test_utils.c
 * @brief    QEMU test for POSIX.1-2024 Base headers and standard C utilities
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <fnmatch.h>
#include <grp.h>
#include <iso646.h>
#include <limits.h>
#include <pwd.h>
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
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_LIBC_0008 VER_PROC_0008 VER_PROC_0009 VER_IPC_0010 */

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

static int test_sysconf_tty_and_proclist(void) {
    /* 1. sysconf() Base values, indeterminate preservation, and EINVAL. */
    if (sysconf(_SC_PAGESIZE) != 4096L || sysconf(_SC_CLK_TCK) != 1000L ||
        sysconf(_SC_OPEN_MAX) != OPEN_MAX || sysconf(_SC_VERSION) != 202405L ||
        sysconf(_SC_2_VERSION) != 202405L || sysconf(_SC_THREADS) != 202405L ||
        sysconf(_SC_SHARED_MEMORY_OBJECTS) != 202405L) {
        printf("[FAIL] sysconf Base values mismatch\n");
        return -1;
    }
    errno = EAGAIN;
    if (sysconf(_SC_SPAWN) != -1L || errno != EAGAIN) {
        printf("[FAIL] sysconf(_SC_SPAWN) altered errno (%d)\n", errno);
        return -1;
    }
    errno = 0;
    if (sysconf(-999) != -1L || errno != EINVAL) {
        printf("[FAIL] sysconf(-999) expected EINVAL got %d\n", errno);
        return -1;
    }

    /* 2. confstr() _CS_PATH, truncation, and EINVAL. */
    char cbuf[32];
    size_t clen = confstr(_CS_PATH, cbuf, sizeof(cbuf));
    if (clen != 6u || strcmp(cbuf, "/boot") != 0) {
        printf("[FAIL] confstr(_CS_PATH) got len=%zu '%s'\n", clen, cbuf);
        return -1;
    }
    char small[4];
    if (confstr(_CS_PATH, small, sizeof(small)) != 6u ||
        strcmp(small, "/bo") != 0) {
        printf("[FAIL] confstr truncation failed\n");
        return -1;
    }
    errno = 0;
    if (confstr(-1, cbuf, sizeof(cbuf)) != 0u || errno != EINVAL) {
        printf("[FAIL] confstr(-1) expected EINVAL got %d\n", errno);
        return -1;
    }

    /* 3. pathconf() and fpathconf(). */
    if (pathconf("/", _PC_PATH_MAX) != PATH_MAX ||
        pathconf("/boot", _PC_NAME_MAX) != NAME_MAX) {
        printf("[FAIL] pathconf values mismatch\n");
        return -1;
    }
    errno = 0;
    if (pathconf("/no_such_pc_file", _PC_PATH_MAX) != -1L || errno != ENOENT) {
        printf("[FAIL] pathconf missing expected ENOENT got %d\n", errno);
        return -1;
    }
    errno = 0;
    if (fpathconf(-1, _PC_PATH_MAX) != -1L || errno != EBADF) {
        printf("[FAIL] fpathconf(-1) expected EBADF got %d\n", errno);
        return -1;
    }
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] pipe2 for fpathconf/ttyname failed\n");
        return -1;
    }
    if (fpathconf(pfd[0], _PC_PIPE_BUF) != PIPE_BUF) {
        printf("[FAIL] fpathconf(_PC_PIPE_BUF) mismatch\n");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }

    /* 4. gethostname(), ctermid(), ttyname(), ttyname_r(). */
    char hbuf[64];
    if (gethostname(hbuf, sizeof(hbuf)) != 0 ||
        strcmp(hbuf, "microkernel") != 0) {
        printf("[FAIL] gethostname mismatch '%s'\n", hbuf);
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    char ctbuf[L_ctermid];
    if (strcmp(ctermid(NULL), "/dev/tty") != 0 ||
        strcmp(ctermid(ctbuf), "/dev/tty") != 0) {
        printf("[FAIL] ctermid mismatch\n");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    char tbuf[32];
    if (ttyname_r(pfd[0], tbuf, sizeof(tbuf)) != ENOTTY ||
        ttyname_r(STDIN_FILENO, tbuf, 4u) != ERANGE ||
        ttyname_r(STDIN_FILENO, tbuf, sizeof(tbuf)) != 0 ||
        strcmp(tbuf, "/dev/tty") != 0) {
        printf("[FAIL] ttyname_r checks failed\n");
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    close(pfd[0]);
    close(pfd[1]);
    char *tn = ttyname(STDIN_FILENO);
    if (!tn || strcmp(tn, "/dev/tty") != 0) {
        printf("[FAIL] ttyname(STDIN_FILENO) mismatch\n");
        return -1;
    }

    /* 5. tcgetsid(), tcgetwinsize(), tcsetwinsize() round-trip. */
    if (tcgetsid(STDIN_FILENO) <= 0) {
        printf("[FAIL] tcgetsid failed\n");
        return -1;
    }
    struct winsize orig_ws = {0, 0};
    struct winsize set_ws = {.ws_row = 40u, .ws_col = 120u};
    struct winsize got_ws = {0, 0};
    if (tcgetwinsize(STDIN_FILENO, &orig_ws) != 0 || orig_ws.ws_row == 0u ||
        orig_ws.ws_col == 0u || tcsetwinsize(STDIN_FILENO, &set_ws) != 0 ||
        tcgetwinsize(STDIN_FILENO, &got_ws) != 0 || got_ws.ws_row != 40u ||
        got_ws.ws_col != 120u) {
        printf("[FAIL] tcgetwinsize/tcsetwinsize round-trip failed\n");
        return -1;
    }
    (void)tcsetwinsize(STDIN_FILENO, &orig_ws);

    /* 6. ProcList() kernel snapshot check. */
    struct __k_proc_info plist[PROC_LIST_MAX];
    memset(plist, 0, sizeof(plist));
    long nproc = ProcList(plist, PROC_LIST_MAX);
    int seen_sysproc = 0;
    int seen_self = 0;
    pid_t me = getpid();
    for (long i = 0; i < nproc; i++) {
        if (plist[i].pid == 1u && strcmp(plist[i].comm, "sysproc") == 0) {
            seen_sysproc = 1;
        }
        if ((pid_t)plist[i].pid == me &&
            strcmp(plist[i].comm, "utilstest") == 0) {
            seen_self = 1;
        }
    }
    if (nproc < 3 || !seen_sysproc || !seen_self) {
        printf("[FAIL] ProcList n=%ld sysproc=%d self=%d\n", nproc,
               seen_sysproc, seen_self);
        return -1;
    }

    printf("[OK] sysconf, confstr, pathconf, gethostname, ttyname, "
           "tcgetwinsize, and ProcList verified\n");
    return 0;
}

static int test_cred_pwd_grp_rlimit(void) {
    /* 1. Initial root credentials, groups, and getlogin/getlogin_r. */
    gid_t grps[8] = {99u};
    int ng = getgroups(8, grps);
    if (getuid() != 0u || geteuid() != 0u || getgid() != 0u ||
        getegid() != 0u || ng != 1 || grps[0] != 0u ||
        getgroups(0, NULL) != 1) {
        printf("[FAIL] initial credentials mismatch\n");
        return -1;
    }
    char lbuf[32];
    if (strcmp(getlogin(), "root") != 0 || getlogin_r(lbuf, 2u) != ERANGE ||
        getlogin_r(lbuf, sizeof(lbuf)) != 0 || strcmp(lbuf, "root") != 0) {
        printf("[FAIL] getlogin/getlogin_r failed\n");
        return -1;
    }

    /* 2. <pwd.h> and <grp.h> lookups and reentrant _r variants. */
    struct passwd *pw = getpwuid(0u);
    struct passwd *pwn = getpwnam("nobody");
    if (!pw || strcmp(pw->pw_name, "root") != 0 || pw->pw_uid != 0u || !pwn ||
        pwn->pw_uid != 65534u || getpwuid(12345u) != NULL) {
        printf("[FAIL] getpwuid/getpwnam failed\n");
        return -1;
    }
    struct passwd pwr;
    struct passwd *pw_res = NULL;
    char pbuf[128];
    if (getpwuid_r(0u, &pwr, pbuf, 4u, &pw_res) != ERANGE ||
        getpwuid_r(0u, &pwr, pbuf, sizeof(pbuf), &pw_res) != 0 ||
        pw_res != &pwr || strcmp(pwr.pw_name, "root") != 0 ||
        getpwnam_r("no_such_user", &pwr, pbuf, sizeof(pbuf), &pw_res) != 0 ||
        pw_res != NULL) {
        printf("[FAIL] getpwuid_r/getpwnam_r failed\n");
        return -1;
    }

    struct group *gr = getgrgid(0u);
    struct group *grw = getgrnam("wheel");
    if (!gr || strcmp(gr->gr_name, "root") != 0 || !grw || grw->gr_gid != 10u ||
        getgrgid(12345u) != NULL) {
        printf("[FAIL] getgrgid/getgrnam failed\n");
        return -1;
    }
    struct group grr;
    struct group *gr_res = NULL;
    char gbuf[128];
    if (getgrgid_r(0u, &grr, gbuf, 4u, &gr_res) != ERANGE ||
        getgrgid_r(0u, &grr, gbuf, sizeof(gbuf), &gr_res) != 0 ||
        gr_res != &grr || !grr.gr_mem || !grr.gr_mem[0] ||
        strcmp(grr.gr_mem[0], "root") != 0) {
        printf("[FAIL] getgrgid_r failed\n");
        return -1;
    }

    /* 3. Fork child to test RLIMIT_NOFILE, credential drop, EPERM, and nice. */
    pid_t child = fork();
    if (child < 0) {
        printf("[FAIL] cred fork failed\n");
        return -1;
    }
    if (child == 0) {
        struct rlimit rl = {0, 0};
        if (getrlimit(RLIMIT_NOFILE, &rl) != 0 || rl.rlim_cur != OPEN_MAX) {
            _exit(10);
        }
        struct rlimit bad = {20UL, 10UL};
        errno = 0;
        if (setrlimit(RLIMIT_NOFILE, &bad) != -1 || errno != EINVAL) {
            _exit(11);
        }
        struct rlimit tight = {5UL, 5UL};
        if (setrlimit(RLIMIT_NOFILE, &tight) != 0) {
            _exit(12);
        }
        int p1[2] = {-1, -1};
        int p2[2] = {-1, -1};
        if (pipe2(p1, O_CLOEXEC) != 0) {
            _exit(13);
        }
        errno = 0;
        if (pipe2(p2, O_CLOEXEC) != -1 || errno != EMFILE) {
            _exit(14);
        }
        close(p1[0]);
        close(p1[1]);

        /* seteuid drop + restore via saved UID, then permanent setuid drop. */
        if (seteuid(65534u) != 0 || geteuid() != 65534u || getuid() != 0u) {
            _exit(15);
        }
        if (seteuid(0u) != 0 || geteuid() != 0u) {
            _exit(16);
        }
        if (setgid(65534u) != 0 || getgid() != 65534u || getegid() != 65534u) {
            _exit(17);
        }
        if (setuid(65534u) != 0 || getuid() != 65534u || geteuid() != 65534u) {
            _exit(18);
        }
        errno = 0;
        if (setuid(0u) != -1 || errno != EPERM) {
            _exit(19);
        }
        struct rlimit raise = {5UL, 10UL};
        errno = 0;
        if (setrlimit(RLIMIT_NOFILE, &raise) != -1 || errno != EPERM) {
            _exit(20);
        }
        if (ProcSetNice(0, 5, 1) != 25L || ProcGetNice(0) != 25L) {
            _exit(21);
        }
        if (ProcSetNice(0, -1, 1) != -EPERM) {
            _exit(22);
        }
        _exit(0);
    }

    int st = 0;
    if (waitpid(child, &st, 0) != child || !WIFEXITED(st) ||
        WEXITSTATUS(st) != 0) {
        printf("[FAIL] cred/rlimit child exited status=%d\n",
               WIFEXITED(st) ? WEXITSTATUS(st) : -1);
        return -1;
    }

    printf("[OK] credentials, pwd/grp, getlogin, rlimit, and nice verified\n");
    return 0;
}

static int test_vfs_links_fifo_statvfs_temp(void) {
    /* 1. chmod/fchmod/chown/lchown/faccessat. */
    int fd = open("/tmp/v33_f", O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644u);
    if (fd < 0) {
        printf("[FAIL] v33 create file failed\n");
        return -1;
    }
    if (pwrite(fd, "0123456789", 10u, 0) != 10 ||
        pwrite(fd, "ABC", 3u, 4) != 3) {
        printf("[FAIL] pwrite failed\n");
        close(fd);
        return -1;
    }
    char pbuf[16] = {0};
    if (pread(fd, pbuf, 10u, 0) != 10 || strcmp(pbuf, "0123ABC789") != 0 ||
        lseek(fd, 0, SEEK_CUR) != 0) {
        printf("[FAIL] pread or offset preservation failed ('%s')\n", pbuf);
        close(fd);
        return -1;
    }
    if (fchmod(fd, 0750u) != 0 || fchown(fd, 1000u, 2000u) != 0) {
        printf("[FAIL] fchmod/fchown failed\n");
        close(fd);
        return -1;
    }
    struct stat st = {0};
    if (fstat(fd, &st) != 0 || (st.st_mode & 0777u) != 0750u ||
        st.st_uid != 1000u || st.st_gid != 2000u) {
        printf("[FAIL] fstat mode/uid/gid mismatch\n");
        close(fd);
        return -1;
    }
    if (posix_close(fd, POSIX_CLOSE_RESTART) != 0) {
        printf("[FAIL] posix_close failed\n");
        return -1;
    }
    if (truncate("/tmp/v33_f", 5) != 0 || stat("/tmp/v33_f", &st) != 0 ||
        st.st_size != 5 ||
        faccessat(AT_FDCWD, "/tmp/v33_f", R_OK | X_OK, 0) != 0) {
        printf("[FAIL] truncate or faccessat failed\n");
        return -1;
    }

    /* 2. Hard links and symbolic links (including ELOOP). */
    if (link("/tmp/v33_f", "/tmp/v33_hl") != 0 ||
        stat("/tmp/v33_hl", &st) != 0 || st.st_nlink != 2u) {
        printf("[FAIL] hard link failed (nlink=%u)\n", (unsigned)st.st_nlink);
        return -1;
    }
    if (symlink("/tmp/v33_f", "/tmp/v33_sl") != 0) {
        printf("[FAIL] symlink failed\n");
        return -1;
    }
    char lbuf[64] = {0};
    ssize_t ln = readlink("/tmp/v33_sl", lbuf, sizeof(lbuf) - 1u);
    struct stat lst = {0};
    if (ln != 10 || strcmp(lbuf, "/tmp/v33_f") != 0 ||
        lstat("/tmp/v33_sl", &lst) != 0 || !S_ISLNK(lst.st_mode) ||
        stat("/tmp/v33_sl", &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size != 5) {
        printf("[FAIL] readlink/lstat/stat symlink follow failed\n");
        return -1;
    }
    (void)symlink("/tmp/v33_loop2", "/tmp/v33_loop1");
    (void)symlink("/tmp/v33_loop1", "/tmp/v33_loop2");
    errno = 0;
    if (stat("/tmp/v33_loop1", &st) != -1 || errno != ELOOP) {
        printf("[FAIL] symlink ELOOP check failed errno=%d\n", errno);
        return -1;
    }
    (void)unlink("/tmp/v33_loop1");
    (void)unlink("/tmp/v33_loop2");
    (void)unlink("/tmp/v33_sl");
    (void)unlink("/tmp/v33_f");
    /* Hard link must survive unlink of original path. */
    if (stat("/tmp/v33_hl", &st) != 0 || st.st_nlink != 1u || st.st_size != 5) {
        printf("[FAIL] hard link did not survive unlink\n");
        return -1;
    }
    if (remove("/tmp/v33_hl") != 0) {
        printf("[FAIL] remove() file failed\n");
        return -1;
    }

    /* 3. FIFO mkfifo + cross-process blocking open/write/read. */
    if (mkfifo("/tmp/v33_fifo", 0660u) != 0 ||
        stat("/tmp/v33_fifo", &st) != 0 || !S_ISFIFO(st.st_mode)) {
        printf("[FAIL] mkfifo failed\n");
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        (void)unlink("/tmp/v33_fifo");
        return -1;
    }
    if (pid == 0) {
        int wf = open("/tmp/v33_fifo", O_WRONLY | O_CLOEXEC);
        if (wf < 0) {
            _exit(1);
        }
        (void)write(wf, "fifo_ok", 7u);
        (void)close(wf);
        _exit(0);
    }
    int rf = open("/tmp/v33_fifo", O_RDONLY | O_CLOEXEC);
    char fbuf[16] = {0};
    ssize_t fn = (rf >= 0) ? read(rf, fbuf, sizeof(fbuf) - 1u) : -1;
    if (rf >= 0) {
        (void)close(rf);
    }
    int wst = 0;
    (void)waitpid(pid, &wst, 0);
    (void)unlink("/tmp/v33_fifo");
    if (fn != 7 || strcmp(fbuf, "fifo_ok") != 0) {
        printf("[FAIL] FIFO read failed fn=%ld '%s'\n", (long)fn, fbuf);
        return -1;
    }

    /* 4. posix_getdents, scandir/alphasort, readdir_r, statvfs/fstatvfs. */
    int dfd = open("/boot", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        return -1;
    }
    uint8_t dbuf[1024];
    int saw_sh = 0;
    for (;;) {
        ssize_t got = posix_getdents(dfd, dbuf, sizeof(dbuf), 0);
        if (got <= 0) {
            break;
        }
        size_t off = 0u;
        while (off < (size_t)got) {
            struct posix_dent *pd = (struct posix_dent *)(dbuf + off);
            if (strcmp(pd->d_name, "sh") == 0 && pd->d_type == DT_REG) {
                saw_sh = 1;
            }
            off += pd->d_reclen;
        }
    }
    struct statvfs sv = {0};
    struct statvfs fsv = {0};
    int sf_ok = (statvfs("/tmp", &sv) == 0 && fstatvfs(dfd, &fsv) == 0 &&
                 sv.f_bsize == 4096UL && (fsv.f_flag & ST_RDONLY) != 0UL);
    (void)close(dfd);
    struct dirent **slist = NULL;
    int sc = scandir("/boot", &slist, NULL, alphasort);
    int sorted = (sc >= 3);
    for (int i = 1; i < sc; i++) {
        if (strcmp(slist[i - 1]->d_name, slist[i]->d_name) > 0) {
            sorted = 0;
        }
    }
    for (int i = 0; i < sc; i++) {
        free(slist[i]);
    }
    free(slist);
    DIR *dirp = opendir("/boot");
    struct dirent rent;
    struct dirent *rres = NULL;
    int rr_ok = (dirp && readdir_r(dirp, &rent, &rres) == 0 && rres == &rent);
    if (dirp) {
        (void)closedir(dirp);
    }
    if (!saw_sh || !sf_ok || !sorted || !rr_ok) {
        printf("[FAIL] getdents=%d statvfs=%d scandir=%d readdir_r=%d\n",
               saw_sh, sf_ok, sorted, rr_ok);
        return -1;
    }

    /* 5. mkstemp, mkostemp, mkdtemp, tmpfile, tmpnam. */
    char t1[] = "/tmp/ut_mkXXXXXX";
    int tfd = mkstemp(t1);
    char t2[] = "/tmp/ut_mdXXXXXX";
    char *td = mkdtemp(t2);
    FILE *tfp = tmpfile();
    char tn[L_tmpnam];
    char *tnp = tmpnam(tn);
    if (tfd < 0 || !td || !tfp || !tnp) {
        printf("[FAIL] temp file helpers failed\n");
        if (tfd >= 0) {
            close(tfd);
        }
        if (tfp) {
            fclose(tfp);
        }
        return -1;
    }
    (void)close(tfd);
    (void)unlink(t1);
    (void)remove(t2);
    (void)fputs("tmp", tfp);
    rewind(tfp);
    char rtmp[8] = {0};
    (void)fgets(rtmp, sizeof(rtmp), tfp);
    (void)fclose(tfp);
    if (strcmp(rtmp, "tmp") != 0) {
        printf("[FAIL] tmpfile round-trip failed\n");
        return -1;
    }

    printf("[OK] VFS chmod/chown, links, FIFO, pread/pwrite, getdents, "
           "statvfs, and temp files verified\n");
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
    if (test_sysconf_tty_and_proclist() != 0) {
        return 1;
    }
    if (test_cred_pwd_grp_rlimit() != 0) {
        return 1;
    }
    if (test_vfs_links_fifo_statvfs_temp() != 0) {
        return 1;
    }
    return 0;
}
