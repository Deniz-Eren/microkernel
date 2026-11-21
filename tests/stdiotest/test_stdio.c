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
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_LIBC_0006 */

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
    return 0;
}
