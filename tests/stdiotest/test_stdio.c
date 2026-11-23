/**
 * @file     test_stdio.c
 * @brief    QEMU test for buffered stdio, env, execvp, and core C17 headers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

/* need:: VER_LIBC_0006 VER_LIBC_0010 VER_LIBC_0014 */

static_assert(CHAR_BIT == 8, "CHAR_BIT");
static_assert(PATH_MAX >= _POSIX_PATH_MAX, "PATH_MAX");
static_assert(NAME_MAX >= _POSIX_NAME_MAX, "NAME_MAX");
static_assert(PIPE_BUF >= _POSIX_PIPE_BUF, "PIPE_BUF");
static_assert(ARG_MAX >= _POSIX_ARG_MAX, "ARG_MAX");
static_assert(SSIZE_MAX == LONG_MAX, "SSIZE_MAX");

static int fmt_helper(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

static void wait_for_vfs(void) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 20000000L};
    for (int i = 0; i < 100; i++) {
        if (access("/tmp", F_OK) == 0 && access("/boot", F_OK) == 0) {
            return;
        }
        (void)nanosleep(&ts, 0);
    }
}

static int test_buffered_stdio(void) {
    wait_for_vfs();

    FILE *fp = fopen("/tmp/stdio.dat", "w+");
    if (!fp || fileno(fp) < 0) {
        printf("[FAIL] stdiotest fopen(/tmp/stdio.dat, w+) failed errno=%d\n",
               errno);
        return -1;
    }

    const char *hdr = "line1:hello\n";
    if (fwrite(hdr, 1, strlen(hdr), fp) != strlen(hdr)) {
        printf("[FAIL] stdiotest fwrite failed\n");
        (void)fclose(fp);
        return -1;
    }
    int64_t val64 = 0x123456789ABCL;
    int fn = fprintf(fp, "%04d:%-6s:%" PRIx64 "\n", 42, "ok", val64);
    if (fn <= 0) {
        printf("[FAIL] stdiotest fprintf failed\n");
        (void)fclose(fp);
        return -1;
    }
    long pos_after = ftell(fp);
    if (pos_after != (long)(strlen(hdr) + (size_t)fn)) {
        printf("[FAIL] stdiotest ftell=%ld expected=%ld\n", pos_after,
               (long)(strlen(hdr) + (size_t)fn));
        (void)fclose(fp);
        return -1;
    }
    if (fflush(fp) != 0) {
        printf("[FAIL] stdiotest fflush failed\n");
        (void)fclose(fp);
        return -1;
    }

    rewind(fp);
    char line[64];
    if (!fgets(line, (int)sizeof(line), fp) || strcmp(line, hdr) != 0) {
        printf("[FAIL] stdiotest fgets line1 mismatch\n");
        (void)fclose(fp);
        return -1;
    }
    if (!fgets(line, (int)sizeof(line), fp) ||
        strcmp(line, "0042:ok    :123456789abc\n") != 0) {
        printf("[FAIL] stdiotest fgets line2 got '%s'\n", line);
        (void)fclose(fp);
        return -1;
    }
    if (fgetc(fp) != EOF || !feof(fp)) {
        printf("[FAIL] stdiotest expected EOF indicator\n");
        (void)fclose(fp);
        return -1;
    }
    clearerr(fp);
    if (feof(fp) || ferror(fp)) {
        printf("[FAIL] stdiotest clearerr did not reset flags\n");
        (void)fclose(fp);
        return -1;
    }

    if (fseeko(fp, 6, SEEK_SET) != 0) {
        printf("[FAIL] stdiotest fseeko failed\n");
        (void)fclose(fp);
        return -1;
    }
    int c1 = fgetc(fp);
    if (c1 != 'h' || ungetc('Z', fp) != 'Z' || fgetc(fp) != 'Z') {
        printf("[FAIL] stdiotest fgetc/ungetc failed (c1=%d)\n", c1);
        (void)fclose(fp);
        return -1;
    }
    char word[8] = {0};
    if (fread(word, 1, 4, fp) != 4 || strcmp(word, "ello") != 0) {
        printf("[FAIL] stdiotest fread after ungetc got '%s'\n", word);
        (void)fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || ftello(fp) != (off_t)pos_after) {
        printf("[FAIL] stdiotest fseek(SEEK_END)/ftello mismatch\n");
        (void)fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) {
        printf("[FAIL] stdiotest fclose failed\n");
        return -1;
    }

    /* Buffered read from read-only bootfs ELF image. */
    FILE *bf = fopen("/boot/sysproc", "rb");
    if (!bf) {
        printf("[FAIL] stdiotest fopen(/boot/sysproc) failed errno=%d\n",
               errno);
        return -1;
    }
    unsigned char elf[4] = {0};
    if (fread(elf, 1, 4, bf) != 4 || elf[0] != 0x7Fu || elf[1] != 'E' ||
        elf[2] != 'L' || elf[3] != 'F') {
        printf("[FAIL] stdiotest /boot/sysproc ELF magic mismatch\n");
        (void)fclose(bf);
        return -1;
    }
    (void)fclose(bf);

    /* fdopen() round trip on a freshly created VFS descriptor. */
    int fd = open("/tmp/fdopen.txt", O_CREAT | O_RDWR | O_TRUNC, 0644u);
    if (fd < 0) {
        printf("[FAIL] stdiotest open(/tmp/fdopen.txt) failed\n");
        return -1;
    }
    FILE *df = fdopen(fd, "w+");
    if (!df || fileno(df) != fd) {
        printf("[FAIL] stdiotest fdopen failed\n");
        (void)close(fd);
        return -1;
    }
    if (fputs("fdopen-line\n", df) == EOF || fflush(df) != 0) {
        printf("[FAIL] stdiotest fdopen fputs/fflush failed\n");
        (void)fclose(df);
        return -1;
    }
    rewind(df);
    if (!fgets(line, (int)sizeof(line), df) ||
        strcmp(line, "fdopen-line\n") != 0) {
        printf("[FAIL] stdiotest fdopen fgets mismatch\n");
        (void)fclose(df);
        return -1;
    }
    (void)fclose(df);
    (void)unlink("/tmp/stdio.dat");
    (void)unlink("/tmp/fdopen.txt");

    /* vsnprintf size query (n=0) and formatting. */
    char fbuf[32];
    int sz = fmt_helper(0, 0, "n=%d:%s", 7, "ok");
    int wr = fmt_helper(fbuf, sizeof(fbuf), "n=%d:%s", 7, "ok");
    if (sz != 6 || wr != 6 || strcmp(fbuf, "n=7:ok") != 0) {
        printf("[FAIL] stdiotest vsnprintf mismatch sz=%d wr=%d\n", sz, wr);
        return -1;
    }

    printf("[OK] buffered stdio fopen, fread/fwrite, fgets, ungetc, fseek, and "
           "fdopen verified\n");
    return 0;
}

static int test_env_and_execvp(void) {
    errno = 0;
    if (setenv("", "v", 1) != -1 || errno != EINVAL) {
        printf("[FAIL] stdiotest setenv('') expected EINVAL\n");
        return -1;
    }
    errno = 0;
    if (setenv("A=B", "v", 1) != -1 || errno != EINVAL) {
        printf("[FAIL] stdiotest setenv('A=B') expected EINVAL\n");
        return -1;
    }
    errno = 0;
    if (unsetenv("A=B") != -1 || errno != EINVAL) {
        printf("[FAIL] stdiotest unsetenv('A=B') expected EINVAL\n");
        return -1;
    }

    if (setenv("STDIOTEST_KEY", "first", 1) != 0 ||
        strcmp(getenv("STDIOTEST_KEY"), "first") != 0) {
        printf("[FAIL] stdiotest setenv initial failed\n");
        return -1;
    }
    if (setenv("STDIOTEST_KEY", "second", 0) != 0 ||
        strcmp(getenv("STDIOTEST_KEY"), "first") != 0) {
        printf("[FAIL] stdiotest setenv overwrite=0 mutated value\n");
        return -1;
    }
    if (setenv("STDIOTEST_KEY", "second", 1) != 0 ||
        strcmp(getenv("STDIOTEST_KEY"), "second") != 0) {
        printf("[FAIL] stdiotest setenv overwrite=1 failed\n");
        return -1;
    }
    if (unsetenv("STDIOTEST_KEY") != 0 || getenv("STDIOTEST_KEY") != 0) {
        printf("[FAIL] stdiotest unsetenv failed\n");
        return -1;
    }

    /* Verify execvp PATH search + mutated environ propagation. */
    if (setenv("TEST_ENV", "posix2024", 1) != 0 ||
        setenv("PATH", "/no_such_dir:/boot", 1) != 0) {
        printf("[FAIL] stdiotest setenv TEST_ENV/PATH failed\n");
        return -1;
    }

    int pfd[2] = {-1, -1};
    if (pipe2(pfd, 0) != 0) {
        printf("[FAIL] stdiotest pipe2 failed errno=%d\n", errno);
        return -1;
    }
    int clo = dup3(pfd[1], 14, O_CLOEXEC);
    if (clo < 0) {
        printf("[FAIL] stdiotest dup3 O_CLOEXEC failed errno=%d\n", errno);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] stdiotest fork failed errno=%d\n", errno);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)close(clo);
        return -1;
    }
    if (pid == 0) {
        (void)close(pfd[0]);
        char wfd_s[16];
        char clo_s[16];
        (void)snprintf(wfd_s, sizeof(wfd_s), "%d", pfd[1]);
        (void)snprintf(clo_s, sizeof(clo_s), "%d", clo);
        char *const args[] = {"exechelper", wfd_s, clo_s, "execvp-ok", 0};
        (void)execvp("exechelper", args);
        _exit(127);
    }

    (void)close(pfd[1]);
    (void)close(clo);
    char rbuf[32] = {0};
    ssize_t n = read(pfd[0], rbuf, sizeof(rbuf) - 1u);
    (void)close(pfd[0]);

    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) ||
        WEXITSTATUS(st) != 77) {
        printf("[FAIL] stdiotest execvp child st=0x%x\n", st);
        return -1;
    }
    if (n != 9 || strcmp(rbuf, "execvp-ok") != 0) {
        printf("[FAIL] stdiotest execvp pipe got '%s' (n=%ld)\n", rbuf,
               (long)n);
        return -1;
    }

    /* Missing executable over PATH returns ENOENT. */
    char *const miss[] = {"no_such_binary_xyz", 0};
    errno = 0;
    if (execvp("no_such_binary_xyz", miss) != -1 || errno != ENOENT) {
        printf("[FAIL] stdiotest missing execvp errno=%d\n", errno);
        return -1;
    }

    printf("[OK] setenv/unsetenv and execvp PATH lookup with environ "
           "propagation verified\n");
    return 0;
}

static int test_core_headers_and_strings(void) {
    assert(1 + 1 == 2);

    /* <ctype.h> */
    if (!isalpha('A') || isalpha('5') || !isdigit('9') || !isxdigit('f') ||
        !isblank('\t') || !isspace('\n') || !ispunct('!') || !iscntrl(1) ||
        !isprint(' ') || isgraph(' ') || tolower('Z') != 'z' ||
        toupper('a') != 'A') {
        printf("[FAIL] stdiotest ctype checks failed\n");
        return -1;
    }

    /* <strings.h> */
    if (strcasecmp("Posix2024", "pOsIx2024") != 0 ||
        strcasecmp("abc", "abd") >= 0 ||
        strncasecmp("MicroKernel", "mIcRoX", 5) != 0) {
        printf("[FAIL] stdiotest strcasecmp/strncasecmp failed\n");
        return -1;
    }

    /* <inttypes.h> */
    char *end = 0;
    imaxdiv_t dv = imaxdiv(17, 5);
    intmax_t v1 = strtoimax("  -0x2a!", &end, 0);
    uintmax_t v2 = strtoumax("077", 0, 0);
    if (imaxabs(-42) != 42 || dv.quot != 3 || dv.rem != 2 || v1 != -42 ||
        !end || *end != '!' || v2 != 63u) {
        printf("[FAIL] stdiotest inttypes checks failed\n");
        return -1;
    }

    /* <string.h> & <stdlib.h> */
    char *dup1 = strdup("hello-world");
    char *dup2 = strndup("hello-world", 5);
    if (!dup1 || !dup2 || strcmp(dup1, "hello-world") != 0 ||
        strcmp(dup2, "hello") != 0) {
        printf("[FAIL] stdiotest strdup/strndup failed\n");
        free(dup1);
        free(dup2);
        return -1;
    }
    const char *sub = strstr(dup1, "world");
    const char *rc = strrchr(dup1, 'l');
    const void *mc = memchr(dup1, '-', strlen(dup1));
    if (!sub || strcmp(sub, "world") != 0 || !rc || strcmp(rc, "ld") != 0 ||
        !mc || *(const char *)mc != '-') {
        printf("[FAIL] stdiotest strstr/strrchr/memchr failed\n");
        free(dup1);
        free(dup2);
        return -1;
    }

    char tokbuf[32];
    (void)strlcpy(tokbuf, "a:b::c", sizeof(tokbuf));
    char *st = 0;
    char *t1 = strtok_r(tokbuf, ":", &st);
    char *t2 = strtok_r(0, ":", &st);
    char *t3 = strtok_r(0, ":", &st);
    char *t4 = strtok_r(0, ":", &st);
    if (!t1 || strcmp(t1, "a") != 0 || !t2 || strcmp(t2, "b") != 0 || !t3 ||
        strcmp(t3, "c") != 0 || t4 != 0) {
        printf("[FAIL] stdiotest strtok_r failed\n");
        free(dup1);
        free(dup2);
        return -1;
    }

    char catbuf[32] = "ab";
    (void)strcat(catbuf, "cd");
    (void)strncat(catbuf, "efgh", 2);
    if (strcmp(catbuf, "abcdef") != 0 || strnlen(catbuf, 4) != 4 ||
        strspn("aaabbb", "a") != 3 || strcspn("abc123", "0123456789") != 3 ||
        *strpbrk("abc123", "321") != '1') {
        printf("[FAIL] stdiotest strcat/strspn/strpbrk failed\n");
        free(dup1);
        free(dup2);
        return -1;
    }

    const char *emsg = strerror(ENOENT);
    if (!emsg || !strstr(emsg, "No such file") ||
        strtoul("0xFF", 0, 0) != 255UL || abs(-9) != 9 || labs(-99L) != 99L ||
        llabs(-999LL) != 999LL) {
        printf("[FAIL] stdiotest strerror/strtoul/abs failed\n");
        free(dup1);
        free(dup2);
        return -1;
    }

    free(dup1);
    free(dup2);
    printf("[OK] ctype, strings, inttypes, limits, assert, strdup, strstr, and "
           "strerror verified\n");
    return 0;
}

static int vscan_helper(const char *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

static int vfscan_helper(FILE *fp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfscanf(fp, fmt, ap);
    va_end(ap);
    return r;
}

static int vdprintf_helper(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vdprintf(fd, fmt, ap);
    va_end(ap);
    return r;
}

static int vasprintf_helper(char **out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(out, fmt, ap);
    va_end(ap);
    return r;
}

static int test_stdio_base_completion(void) {
    /* 1. sscanf / vsscanf: ints, floats, strings, scansets, %m, %n, width. */
    int d = 0;
    int ix = 0;
    unsigned int u = 0;
    unsigned int oct = 0;
    unsigned int hx = 0;
    double lf = 0.0;
    float sf = 0.0f;
    char word[16] = {0};
    char setbuf_s[16] = {0};
    char chars[4] = {0};
    char *mstr = 0;
    void *ptr = 0;
    int npos = 0;

    int rc = sscanf(" -42 0x1A 99 077 beef 3.25 -0.5 hello abc123 XY 0x1234 "
                    "alloc %",
                    "%d %i %u %o %x %lf %f %4s%*s %[a-z0-9] %2c %p %ms %%%n",
                    &d, &ix, &u, &oct, &hx, &lf, &sf, word, setbuf_s, chars,
                    &ptr, &mstr, &npos);
    if (rc != 12 || d != -42 || ix != 26 || u != 99u || oct != 63u ||
        hx != 0xBEEFu || fabs(lf - 3.25) > 1e-12 || fabs(sf + 0.5f) > 1e-6f ||
        strcmp(word, "hell") != 0 || strcmp(setbuf_s, "abc123") != 0 ||
        chars[0] != 'X' || chars[1] != 'Y' ||
        (uintptr_t)ptr != (uintptr_t)0x1234u || !mstr ||
        strcmp(mstr, "alloc") != 0 || npos <= 0) {
        printf("[FAIL] stdiotest sscanf mismatch rc=%d d=%d ix=%d\n", rc, d,
               ix);
        free(mstr);
        return -1;
    }
    free(mstr);

    int v1 = 0;
    int v2 = 0;
    if (vscan_helper("  7,11", "%d,%d", &v1, &v2) != 2 || v1 != 7 || v2 != 11 ||
        sscanf("   ", "%d", &v1) != EOF) {
        printf("[FAIL] stdiotest vsscanf/EOF failed\n");
        return -1;
    }

    /* 2. fmemopen + fscanf / vfscanf + fgetpos / fsetpos + getline / getdelim.
     */
    char mbuf[64] = "alpha:100\nbeta:200\n";
    FILE *mf = fmemopen(mbuf, strlen(mbuf), "r");
    if (!mf || fileno(mf) != -1) {
        printf("[FAIL] stdiotest fmemopen(r) failed\n");
        if (mf) {
            (void)fclose(mf);
        }
        return -1;
    }
    char k1[16] = {0};
    int n1 = 0;
    fpos_t pos = {0};
    if (fscanf(mf, "%[^:]:%d\n", k1, &n1) != 2 || strcmp(k1, "alpha") != 0 ||
        n1 != 100 || fgetpos(mf, &pos) != 0) {
        printf("[FAIL] stdiotest fscanf/fgetpos failed\n");
        (void)fclose(mf);
        return -1;
    }
    char k2[16] = {0};
    int n2 = 0;
    if (vfscan_helper(mf, "%[^:]:%d\n", k2, &n2) != 2 ||
        strcmp(k2, "beta") != 0 || n2 != 200) {
        printf("[FAIL] stdiotest vfscanf failed\n");
        (void)fclose(mf);
        return -1;
    }
    if (fsetpos(mf, &pos) != 0) {
        printf("[FAIL] stdiotest fsetpos failed\n");
        (void)fclose(mf);
        return -1;
    }
    char *gline = 0;
    size_t gcap = 0u;
    ssize_t glen = getdelim(&gline, &gcap, ':', mf);
    if (glen != 5 || !gline || strcmp(gline, "beta:") != 0) {
        printf("[FAIL] stdiotest getdelim failed glen=%ld\n", (long)glen);
        free(gline);
        (void)fclose(mf);
        return -1;
    }
    glen = getline(&gline, &gcap, mf);
    if (glen != 4 || strcmp(gline, "200\n") != 0 ||
        getline(&gline, &gcap, mf) != -1) {
        printf("[FAIL] stdiotest getline failed glen=%ld\n", (long)glen);
        free(gline);
        (void)fclose(mf);
        return -1;
    }
    free(gline);
    (void)fclose(mf);

    /* fmemopen(NULL, 32, "w+") internal buffer round-trip. */
    FILE *mw = fmemopen(0, 32u, "w+");
    if (!mw || fprintf(mw, "mem=%d", 77) != 6 || fseek(mw, 0, SEEK_SET) != 0) {
        printf("[FAIL] stdiotest fmemopen(NULL, w+) write failed\n");
        if (mw) {
            (void)fclose(mw);
        }
        return -1;
    }
    char rmem[16] = {0};
    if (!fgets(rmem, (int)sizeof(rmem), mw) || strcmp(rmem, "mem=77") != 0) {
        printf("[FAIL] stdiotest fmemopen(NULL, w+) read got '%s'\n", rmem);
        (void)fclose(mw);
        return -1;
    }
    (void)fclose(mw);

    /* 3. open_memstream dynamic growth, flush, seek, and close. */
    char *ms_buf = 0;
    size_t ms_len = 0u;
    FILE *ms = open_memstream(&ms_buf, &ms_len);
    if (!ms) {
        printf("[FAIL] stdiotest open_memstream failed\n");
        return -1;
    }
    for (int i = 0; i < 10; i++) {
        (void)fprintf(ms, "item-%02d;", i);
    }
    if (fflush(ms) != 0 || !ms_buf || ms_len != 80u ||
        strncmp(ms_buf, "item-00;item-01;", 16u) != 0) {
        printf("[FAIL] stdiotest open_memstream flush len=%zu\n", ms_len);
        (void)fclose(ms);
        free(ms_buf);
        return -1;
    }
    (void)fseek(ms, 0, SEEK_SET);
    (void)fputs("HEAD", ms);
    (void)fseek(ms, 0, SEEK_END);
    (void)fputs("END", ms);
    if (fclose(ms) != 0 || ms_len != 83u ||
        strncmp(ms_buf, "HEAD-00;", 8u) != 0 ||
        strcmp(ms_buf + 80u, "END") != 0) {
        printf("[FAIL] stdiotest open_memstream close len=%zu\n", ms_len);
        free(ms_buf);
        return -1;
    }
    free(ms_buf);

    /* 4. dprintf / vdprintf and asprintf / vasprintf (with float printf). */
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, 0) != 0) {
        printf("[FAIL] stdiotest pipe2 for dprintf failed\n");
        return -1;
    }
    int d1 = dprintf(pfd[1], "d=%d,", 123);
    int d2 = vdprintf_helper(pfd[1], "f=%.2f", 3.5);
    (void)close(pfd[1]);
    char pbuf[32] = {0};
    ssize_t pn = read(pfd[0], pbuf, sizeof(pbuf) - 1u);
    (void)close(pfd[0]);
    if (d1 != 6 || d2 != 6 || pn != 12 || strcmp(pbuf, "d=123,f=3.50") != 0) {
        printf("[FAIL] stdiotest dprintf/vdprintf got '%s'\n", pbuf);
        return -1;
    }

    char *as1 = 0;
    char *as2 = 0;
    int an1 = asprintf(&as1, "%#x:%.3f:%e", 0x2au, 1.25, 12.5);
    int an2 = vasprintf_helper(&as2, "ok-%s", "2024");
    if (an1 <= 0 || !as1 || strncmp(as1, "0x2a:1.250:1.250000e+01", 23u) != 0 ||
        an2 != 7 || !as2 || strcmp(as2, "ok-2024") != 0) {
        printf("[FAIL] stdiotest asprintf got '%s' / '%s'\n",
               as1 ? as1 : "(null)", as2 ? as2 : "(null)");
        free(as1);
        free(as2);
        return -1;
    }
    free(as1);
    free(as2);

    /* 5. freopen + scanf / vscanf in a forked child on redirected stdin. */
    FILE *rf = fopen("/tmp/reopen1.txt", "w+");
    if (!rf || fputs("first\n", rf) == EOF) {
        printf("[FAIL] stdiotest freopen setup failed\n");
        if (rf) {
            (void)fclose(rf);
        }
        return -1;
    }
    rf = freopen("/tmp/reopen2.txt", "w+", rf);
    if (!rf || fputs("987 654\n", rf) == EOF || fflush(rf) != 0) {
        printf("[FAIL] stdiotest freopen(/tmp/reopen2.txt) failed\n");
        if (rf) {
            (void)fclose(rf);
        }
        return -1;
    }
    (void)fclose(rf);

    pid_t sc_pid = fork();
    if (sc_pid == 0) {
        if (!freopen("/tmp/reopen2.txt", "r", stdin)) {
            _exit(2);
        }
        int a = 0;
        int b = 0;
        if (scanf("%d %d", &a, &b) != 2 || a != 987 || b != 654) {
            _exit(3);
        }
        _exit(0);
    }
    int sc_st = 0;
    if (sc_pid < 0 || waitpid(sc_pid, &sc_st, 0) != sc_pid ||
        !WIFEXITED(sc_st) || WEXITSTATUS(sc_st) != 0) {
        printf("[FAIL] stdiotest freopen(stdin)/scanf child st=0x%x\n", sc_st);
        return -1;
    }
    (void)unlink("/tmp/reopen1.txt");
    (void)unlink("/tmp/reopen2.txt");

    printf("[OK] Base stdio scanf*, getline/getdelim, dprintf, asprintf, "
           "fmemopen, open_memstream, freopen, and fgetpos/fsetpos verified\n");
    return 0;
}

static int g_qe_fd = -1;

static void qe_handler_1(void) {
    if (g_qe_fd >= 0) {
        (void)write(g_qe_fd, "1", 1u);
    }
}

static void qe_handler_2(void) {
    if (g_qe_fd >= 0) {
        (void)write(g_qe_fd, "2", 1u);
    }
}

static void qe_atexit_forbidden(void) {
    if (g_qe_fd >= 0) {
        (void)write(g_qe_fd, "X", 1u);
    }
}

static int cmp_int_r(const void *a, const void *b, void *arg) {
    int dir = *(const int *)arg;
    int va = *(const int *)a;
    int vb = *(const int *)b;
    return dir * ((va > vb) - (va < vb));
}

static int test_stdlib_and_string_completion(void) {
    /* 1. strtod, strtof, strtold, atof. */
    char *end = 0;
    errno = 0;
    double d1 = strtod("  -3.141592653589793e0!", &end);
    if (fabs(d1 + 3.141592653589793) > 1e-15 || !end || *end != '!' ||
        errno != 0) {
        printf("[FAIL] stdiotest strtod pi mismatch\n");
        return -1;
    }
    double dhex = strtod("0x1.8p+1", &end);
    if (fabs(dhex - 3.0) > 1e-15 || !end || *end != '\0') {
        printf("[FAIL] stdiotest strtod hex mismatch\n");
        return -1;
    }
    double dinf = strtod("-Infinity", &end);
    double dnan = strtod("NaN(42)", &end);
    if (!isinf(dinf) || !signbit(dinf) || !isnan(dnan) || !end ||
        *end != '\0') {
        printf("[FAIL] stdiotest strtod inf/nan failed\n");
        return -1;
    }
    errno = 0;
    double dovf = strtod("1e500", &end);
    if (!isinf(dovf) || errno != ERANGE) {
        printf("[FAIL] stdiotest strtod overflow errno=%d\n", errno);
        return -1;
    }
    errno = 0;
    double duf = strtod("1e-500", &end);
    if (duf != 0.0 || errno != ERANGE) {
        printf("[FAIL] stdiotest strtod underflow errno=%d\n", errno);
        return -1;
    }
    double drb = strtod("12e+xyz", &end);
    if (drb != 12.0 || !end || strcmp(end, "e+xyz") != 0) {
        printf("[FAIL] stdiotest strtod rollback end='%s'\n",
               end ? end : "(null)");
        return -1;
    }
    const char *bad_s = "  abc";
    errno = 0;
    if (strtod(bad_s, &end) != 0.0 || end != bad_s || errno != 0) {
        printf("[FAIL] stdiotest strtod invalid input failed\n");
        return -1;
    }
    float f1 = strtof("1.5e2", 0);
    long double ld1 = strtold("0.25", 0);
    if (fabsf(f1 - 150.0f) > 1e-5f || fabsl(ld1 - 0.25L) > 1e-12L ||
        fabs(atof("-42.5") + 42.5) > 1e-12) {
        printf("[FAIL] stdiotest strtof/strtold/atof failed\n");
        return -1;
    }

    /* 2. aligned_alloc + realloc + free preserving errno. */
    errno = 0;
    if (aligned_alloc(24u, 64u) != 0 || errno != EINVAL) {
        printf("[FAIL] stdiotest aligned_alloc(24) expected EINVAL\n");
        return -1;
    }
    static const size_t aligns[] = {16u, 32u, 64u, 256u, 4096u};
    for (size_t i = 0; i < 5u; i++) {
        size_t sz = 37u + i * 19u;
        unsigned char *p = (unsigned char *)aligned_alloc(aligns[i], sz);
        if (!p || (((uintptr_t)p & (aligns[i] - 1u)) != 0u)) {
            printf("[FAIL] stdiotest aligned_alloc(%zu) misaligned\n",
                   aligns[i]);
            return -1;
        }
        memset(p, (int)(0x40u + i), sz);
        unsigned char *g = (unsigned char *)realloc(p, sz + 64u);
        if (!g || g[0] != (unsigned char)(0x40u + i) ||
            g[sz - 1u] != (unsigned char)(0x40u + i)) {
            printf("[FAIL] stdiotest realloc(aligned_alloc) lost data\n");
            free(g ? g : p);
            return -1;
        }
        errno = EAGAIN;
        free(g);
        if (errno != EAGAIN) {
            printf("[FAIL] stdiotest free(aligned_alloc) modified errno\n");
            return -1;
        }
    }

    /* 3. quick_exit / at_quick_exit in forked child (LIFO, skips atexit). */
    int qfd[2] = {-1, -1};
    if (pipe2(qfd, 0) != 0) {
        printf("[FAIL] stdiotest pipe2 for quick_exit failed\n");
        return -1;
    }
    pid_t qpid = fork();
    if (qpid == 0) {
        (void)close(qfd[0]);
        g_qe_fd = qfd[1];
        (void)atexit(qe_atexit_forbidden);
        (void)at_quick_exit(qe_handler_1);
        (void)at_quick_exit(qe_handler_2);
        quick_exit(42);
    }
    (void)close(qfd[1]);
    char qbuf[8] = {0};
    size_t qn = 0u;
    while (qn < sizeof(qbuf) - 1u) {
        ssize_t rr = read(qfd[0], qbuf + qn, sizeof(qbuf) - 1u - qn);
        if (rr <= 0) {
            break;
        }
        qn += (size_t)rr;
    }
    (void)close(qfd[0]);
    int qst = 0;
    if (qpid < 0 || waitpid(qpid, &qst, 0) != qpid || !WIFEXITED(qst) ||
        WEXITSTATUS(qst) != 42 || qn != 2u || strcmp(qbuf, "21") != 0) {
        printf("[FAIL] stdiotest quick_exit qbuf='%s' st=0x%x\n", qbuf, qst);
        return -1;
    }

    /* 4. div / ldiv / lldiv, rand / srand, qsort_r, getsubopt. */
    div_t d_res = div(-17, 5);
    ldiv_t ld_res = ldiv(1000000L, 7L);
    lldiv_t lld_res = lldiv(10000000000LL, 9LL);
    if (d_res.quot != -3 || d_res.rem != -2 || ld_res.quot != 142857L ||
        ld_res.rem != 1L || lld_res.quot != 1111111111LL ||
        lld_res.rem != 1LL) {
        printf("[FAIL] stdiotest div/ldiv/lldiv failed\n");
        return -1;
    }

    srand(12345u);
    int r1 = rand();
    int r2 = rand();
    srand(12345u);
    int r3 = rand();
    if (r1 < 0 || r1 > RAND_MAX || r2 < 0 || r1 != r3 || r1 == r2) {
        printf("[FAIL] stdiotest rand/srand repeatability failed\n");
        return -1;
    }

    int arr[5] = {4, 1, 5, 2, 3};
    int desc = -1;
    qsort_r(arr, 5u, sizeof(int), cmp_int_r, &desc);
    if (arr[0] != 5 || arr[1] != 4 || arr[2] != 3 || arr[3] != 2 ||
        arr[4] != 1) {
        printf("[FAIL] stdiotest qsort_r descending failed\n");
        return -1;
    }

    char opt_str[] = "ro,size=64,bad=1,rw";
    char *optp = opt_str;
    char *const keys[] = {"ro", "rw", "size", 0};
    char *val = 0;
    int s0 = getsubopt(&optp, keys, &val);
    int ok_sub = (s0 == 0 && val == 0);
    int s1 = getsubopt(&optp, keys, &val);
    ok_sub = ok_sub && (s1 == 2 && val && strcmp(val, "64") == 0);
    int s2 = getsubopt(&optp, keys, &val);
    ok_sub = ok_sub && (s2 == -1 && val && strcmp(val, "bad=1") == 0);
    int s3 = getsubopt(&optp, keys, &val);
    ok_sub = ok_sub && (s3 == 1 && val == 0);
    int s4 = getsubopt(&optp, keys, &val);
    ok_sub = ok_sub && (s4 == -1 && val == 0);
    if (!ok_sub) {
        printf("[FAIL] stdiotest getsubopt failed\n");
        return -1;
    }

    /* 5. stpcpy, stpncpy, memmem, strerror_r. */
    char sbuf[16];
    char *ep = stpcpy(sbuf, "abc");
    if (ep != sbuf + 3 || strcmp(sbuf, "abc") != 0) {
        printf("[FAIL] stdiotest stpcpy failed\n");
        return -1;
    }
    memset(sbuf, 'Z', sizeof(sbuf));
    char *en = stpncpy(sbuf, "hi", 5u);
    if (en != sbuf + 2 || sbuf[2] != '\0' || sbuf[4] != '\0' ||
        sbuf[5] != 'Z') {
        printf("[FAIL] stdiotest stpncpy failed\n");
        return -1;
    }
    const unsigned char hay[] = {'A', 0, 'B', 'C', 0, 'D'};
    const unsigned char ndl[] = {'C', 0, 'D'};
    const unsigned char *hit =
        (const unsigned char *)memmem(hay, sizeof(hay), ndl, sizeof(ndl));
    if (hit != hay + 3 || memmem(hay, sizeof(hay), "XYZ", 3u) != 0 ||
        memmem(hay, sizeof(hay), "", 0u) != hay) {
        printf("[FAIL] stdiotest memmem failed\n");
        return -1;
    }
    char ebuf[64] = {0};
    errno = EAGAIN;
    if (strerror_r(ENOENT, ebuf, sizeof(ebuf)) != 0 ||
        !strstr(ebuf, "No such file") || errno != EAGAIN) {
        printf("[FAIL] stdiotest strerror_r(ENOENT) failed\n");
        return -1;
    }
    if (strerror_r(ENOENT, ebuf, 4u) != ERANGE ||
        strerror_r(99999, ebuf, sizeof(ebuf)) != EINVAL) {
        printf("[FAIL] stdiotest strerror_r ERANGE/EINVAL failed\n");
        return -1;
    }

    printf(
        "[OK] Base stdlib strtod*, aligned_alloc, quick_exit, div*, rand, "
        "qsort_r, getsubopt, and string stpcpy/memmem/strerror_r verified\n");
    return 0;
}

static int vswprintf_helper(wchar_t *ws, size_t n, const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vswprintf(ws, n, fmt, ap);
    va_end(ap);
    return r;
}

static int vswscanf_helper(const wchar_t *ws, const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vswscanf(ws, fmt, ap);
    va_end(ap);
    return r;
}

static int vfwscanf_helper(FILE *fp, const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfwscanf(fp, fmt, ap);
    va_end(ap);
    return r;
}

static int test_wide_stdio_and_wmemstream(void) {
    if (!setlocale(LC_ALL, "C.UTF-8")) {
        printf("[FAIL] stdiotest setlocale(C.UTF-8) failed\n");
        return -1;
    }

    /* 1. fwide, fputwc/putwc/fputws, fwprintf, fgetwc/ungetwc/getwc/fgetws. */
    FILE *fp = fopen("/tmp/wide_io.txt", "w+");
    if (!fp) {
        printf("[FAIL] stdiotest fopen(/tmp/wide_io.txt) errno=%d\n", errno);
        return -1;
    }
    if (fwide(fp, 0) != 0 || fwide(fp, 1) <= 0 || fwide(fp, -1) <= 0) {
        printf("[FAIL] stdiotest fwide sticky orientation failed\n");
        (void)fclose(fp);
        return -1;
    }
    int nwritten = 0;
    if (fputwc(L'A', fp) != (wint_t)L'A' ||
        putwc((wchar_t)0x00E9, fp) != (wint_t)0x00E9 ||
        fputws(L" \u20ac\n", fp) < 0 ||
        fwprintf(fp, L"val=%d hex=%#x flt=%.2f s=%s ls=%ls%n\n", -42, 0x2au,
                 3.25, "utf\xC3\xA9", L"\u03c0", &nwritten) < 0 ||
        nwritten <= 0) {
        printf("[FAIL] stdiotest wide file output failed\n");
        (void)fclose(fp);
        return -1;
    }
    rewind(fp);

    wint_t c0 = fgetwc(fp);
    wint_t c1 = getwc(fp);
    if (c0 != (wint_t)L'A' || c1 != (wint_t)0x00E9 || ungetwc(c1, fp) != c1 ||
        fgetwc(fp) != (wint_t)0x00E9) {
        printf("[FAIL] stdiotest fgetwc/getwc/ungetwc failed\n");
        (void)fclose(fp);
        return -1;
    }
    wchar_t line[64] = {0};
    if (!fgetws(line, 64, fp) || wcscmp(line, L" \u20ac\n") != 0) {
        printf("[FAIL] stdiotest fgetws line 1 mismatch\n");
        (void)fclose(fp);
        return -1;
    }
    int iv = 0;
    unsigned int hv = 0u;
    double dv = 0.0;
    char ns[16] = {0};
    wchar_t ws_in[16] = {0};
    int sc = vfwscanf_helper(fp, L"val=%d hex=%x flt=%lf s=%15s ls=%15ls", &iv,
                             &hv, &dv, ns, ws_in);
    (void)fclose(fp);
    (void)unlink("/tmp/wide_io.txt");
    if (sc != 5 || iv != -42 || hv != 0x2au || fabs(dv - 3.25) > 1e-6 ||
        strcmp(ns, "utf\xC3\xA9") != 0 || wcscmp(ws_in, L"\u03c0") != 0) {
        printf("[FAIL] stdiotest vfwscanf sc=%d iv=%d hv=%x\n", sc, iv, hv);
        return -1;
    }

    /* 2. swprintf / vswprintf formatting and EOVERFLOW on small buffer. */
    wchar_t wout[32] = {0};
    int sn = vswprintf_helper(wout, 32u, L"[%04d %lc %ls]", 7, (wint_t)0x00E9u,
                              L"ok");
    if (sn != 11 || wcscmp(wout, L"[0007 \u00e9 ok]") != 0) {
        printf("[FAIL] stdiotest vswprintf sn=%d\n", sn);
        return -1;
    }
    wchar_t tiny[4] = {0};
    errno = 0;
    if (swprintf(tiny, 4u, L"abcdef") >= 0 || errno != EOVERFLOW ||
        tiny[3] != L'\0') {
        printf("[FAIL] stdiotest swprintf overflow check failed\n");
        return -1;
    }

    /* 3. swscanf / vswscanf with scanset, %mls allocation, and %n. */
    wchar_t *alloc_ws = NULL;
    wchar_t set_out[16] = {0};
    int npos = 0;
    int s2 = vswscanf_helper(L"  abc123 caf\u00e9", L" %l[a-z]%*d %mls%n",
                             set_out, &alloc_ws, &npos);
    if (s2 != 2 || wcscmp(set_out, L"abc") != 0 || !alloc_ws ||
        wcscmp(alloc_ws, L"caf\u00e9") != 0 || npos != 13) {
        printf("[FAIL] stdiotest vswscanf s2=%d npos=%d\n", s2, npos);
        free(alloc_ws);
        return -1;
    }
    free(alloc_ws);

    /* 4. open_wmemstream growth, fseek, flush, and close. */
    wchar_t *wmbuf = NULL;
    size_t wmsize = 0u;
    FILE *wm = open_wmemstream(&wmbuf, &wmsize);
    if (!wm || fwide(wm, 0) <= 0) {
        printf("[FAIL] stdiotest open_wmemstream failed\n");
        return -1;
    }
    if (fwprintf(wm, L"hello %ls", L"\u03c0") < 0 || fflush(wm) != 0 ||
        !wmbuf || wmsize != 7u || wcscmp(wmbuf, L"hello \u03c0") != 0) {
        printf("[FAIL] stdiotest open_wmemstream flush mismatch size=%zu\n",
               wmsize);
        (void)fclose(wm);
        free(wmbuf);
        return -1;
    }
    if (fseek(wm, 0L, SEEK_SET) != 0 || fputwc(L'H', wm) != (wint_t)L'H' ||
        fseek(wm, 0L, SEEK_END) != 0 || fputws(L"!", wm) < 0 ||
        fclose(wm) != 0 || wmsize != 8u ||
        wcscmp(wmbuf, L"Hello \u03c0!") != 0) {
        printf("[FAIL] stdiotest open_wmemstream seek/close mismatch\n");
        free(wmbuf);
        return -1;
    }
    free(wmbuf);
    (void)setlocale(LC_ALL, "C");

    printf("[OK] Wide stdio fwide, fgetwc/fputwc/ungetwc, fwprintf/swprintf, "
           "fwscanf/swscanf, and open_wmemstream verified\n");
    return 0;
}

int main(void) {
    if (test_buffered_stdio() != 0) {
        return 1;
    }
    if (test_env_and_execvp() != 0) {
        return 1;
    }
    if (test_core_headers_and_strings() != 0) {
        return 1;
    }
    if (test_stdio_base_completion() != 0) {
        return 1;
    }
    if (test_stdlib_and_string_completion() != 0) {
        return 1;
    }
    if (test_wide_stdio_and_wmemstream() != 0) {
        return 1;
    }
    return 0;
}
