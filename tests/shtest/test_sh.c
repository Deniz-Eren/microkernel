/**
 * @file     test_sh.c
 * @brief    QEMU test for Ring 3 POSIX.1-2024 XCU sh and core utilities
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_LIBC_0007 VER_PROC_0008 VER_PROC_0009 VER_IPC_0010 */

static void wait_for_vfs(void) {
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 20000000L};
    for (int i = 0; i < 100; i++) {
        if (access("/tmp", F_OK) == 0 && access("/boot", F_OK) == 0) {
            return;
        }
        (void)nanosleep(&ts, NULL);
    }
}

static int run_sh_capture(const char *script, const void *stdin_data,
                          size_t stdin_len, char *out, size_t out_cap,
                          ssize_t *out_len) {
    int out_pfd[2] = {-1, -1};
    int in_pfd[2] = {-1, -1};
    if (pipe2(out_pfd, O_CLOEXEC) != 0) {
        return -1;
    }
    if (stdin_data && pipe2(in_pfd, O_CLOEXEC) != 0) {
        (void)close(out_pfd[0]);
        (void)close(out_pfd[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        (void)close(out_pfd[0]);
        (void)close(out_pfd[1]);
        if (stdin_data) {
            (void)close(in_pfd[0]);
            (void)close(in_pfd[1]);
        }
        return -1;
    }
    if (pid == 0) {
        (void)dup2(out_pfd[1], STDOUT_FILENO);
        (void)close(out_pfd[0]);
        (void)close(out_pfd[1]);
        if (stdin_data) {
            (void)dup2(in_pfd[0], STDIN_FILENO);
            (void)close(in_pfd[0]);
            (void)close(in_pfd[1]);
        }
        char *const argv[] = {"sh", "-c", (char *)script, NULL};
        (void)execv("/boot/sh", argv);
        _exit(127);
    }

    (void)close(out_pfd[1]);
    if (stdin_data) {
        (void)close(in_pfd[0]);
        size_t off = 0;
        while (off < stdin_len) {
            ssize_t w = write(in_pfd[1], (const char *)stdin_data + off,
                              stdin_len - off);
            if (w <= 0) {
                break;
            }
            off += (size_t)w;
        }
        (void)close(in_pfd[1]);
    }

    size_t got = 0;
    for (;;) {
        if (got + 1u < out_cap) {
            ssize_t n = read(out_pfd[0], out + got, out_cap - 1u - got);
            if (n <= 0) {
                break;
            }
            got += (size_t)n;
        } else {
            char discard[128];
            ssize_t n = read(out_pfd[0], discard, sizeof(discard));
            if (n <= 0) {
                break;
            }
        }
    }
    out[got] = '\0';
    (void)close(out_pfd[0]);
    if (out_len) {
        *out_len = (ssize_t)got;
    }

    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(st)) {
        return -1;
    }
    return WEXITSTATUS(st);
}

static int test_pipelines_redirs_and_utils(void) {
    char out[512];
    ssize_t len = 0;

    /* Pipeline + redirection (> and >>) + external /boot/echo and /boot/cat. */
    int rc = run_sh_capture(
        "/boot/echo alpha | /boot/cat > /tmp/sh_p.txt && "
        "/boot/echo beta >> /tmp/sh_p.txt && /boot/cat < /tmp/sh_p.txt",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "alpha\nbeta\n") != 0) {
        printf("[FAIL] shtest pipeline/redir rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* External /boot/ls and /boot/test utility checks. */
    rc = run_sh_capture("/boot/ls /boot | /boot/cat", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || !strstr(out, "sh\n") || !strstr(out, "echo\n") ||
        !strstr(out, "cat\n") || !strstr(out, "ls\n")) {
        printf("[FAIL] shtest ls /boot rc=%d\n", rc);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/test -f /tmp/sh_p.txt && /boot/test -d /tmp && "
        "! /boot/test -f /tmp/no_such_sh_file && /boot/test 7 -gt 3",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0) {
        printf("[FAIL] shtest test utility rc=%d\n", rc);
        return -1;
    }

    (void)unlink("/tmp/sh_p.txt");
    printf("[OK] sh pipelines, redirections, and core utilities (echo, cat, "
           "ls, test) verified\n");
    return 0;
}

static int test_quoting_pipefail_and_builtins(void) {
    char out[256];
    ssize_t len = 0;

    /* POSIX.1-2024 Issue 8 ANSI-C $'...' quoting. */
    int rc = run_sh_capture("/boot/echo -n $'A\\tB\\nC\\x44'", NULL, 0, out,
                            sizeof(out), &len);
    if (rc != 0 || len != 6 || memcmp(out, "A\tB\nCD", 6) != 0) {
        printf("[FAIL] shtest $'...' quoting rc=%d len=%ld\n", rc, (long)len);
        return -1;
    }

    /* Built-ins: cd, pwd, export, variable expansion, $?. */
    rc = run_sh_capture(
        "cd /tmp && export GREET=posix24 && echo \"$GREET:$?:\" && pwd", NULL,
        0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "posix24:0:\n/tmp\n") != 0) {
        printf("[FAIL] shtest builtins rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* POSIX.1-2024 Issue 8 set -o pipefail vs default +o pipefail. */
    rc = run_sh_capture("/boot/false | /boot/true", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0) {
        printf("[FAIL] shtest default pipeline expected 0 got %d\n", rc);
        return -1;
    }
    rc = run_sh_capture("set -o pipefail; /boot/false | /boot/true", NULL, 0,
                        out, sizeof(out), &len);
    if (rc != 1) {
        printf("[FAIL] shtest pipefail false|true expected 1 got %d\n", rc);
        return -1;
    }
    rc =
        run_sh_capture("set -o pipefail; /boot/true | /boot/false | /boot/true",
                       NULL, 0, out, sizeof(out), &len);
    if (rc != 1) {
        printf("[FAIL] shtest pipefail 3-stage expected 1 got %d\n", rc);
        return -1;
    }
    rc = run_sh_capture("set -o pipefail; /boot/true | /boot/true", NULL, 0,
                        out, sizeof(out), &len);
    if (rc != 0) {
        printf("[FAIL] shtest pipefail all-true expected 0 got %d\n", rc);
        return -1;
    }

    printf("[OK] sh Issue 8 ANSI-C quoting, set -o pipefail, and built-ins "
           "verified\n");
    return 0;
}

static int test_read_delim(void) {
    char out[256];
    ssize_t len = 0;

    /* POSIX.1-2024 Issue 8 read -d ':' custom delimiter. */
    const char *colon_in = "alpha:beta:gamma:";
    int rc = run_sh_capture(
        "read -d : a && read -d : b && read -d : c && echo \"$a|$b|$c\"",
        colon_in, strlen(colon_in), out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "alpha|beta|gamma\n") != 0) {
        printf("[FAIL] shtest read -d ':' rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* POSIX.1-2024 Issue 8 read -d '' NUL-byte delimiter. */
    const char nul_in[] = "foo\0bar\0";
    rc = run_sh_capture("read -r -d '' x && read -r -d '' y && echo \"$x+$y\"",
                        nul_in, sizeof(nul_in) - 1u, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "foo+bar\n") != 0) {
        printf("[FAIL] shtest read -d '' NUL rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] sh Issue 8 read -d custom and NUL delimiters verified\n");
    return 0;
}

static int test_cwd_across_execve(void) {
    char out[2048];
    ssize_t len = 0;

    /* cd + bare ls must list the working directory, not root '/'. */
    int rc =
        run_sh_capture("cd /boot && /boot/ls", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "sh\n") || !strstr(out, "ls\n") ||
        strstr(out, "tmp\n") != NULL || strstr(out, "boot\n") != NULL) {
        printf("[FAIL] shtest cd /boot && ls rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* Relative file creation, ls, and cat across execve in a subdirectory. */
    rc = run_sh_capture(
        "cd /tmp && /boot/mkdir cwd_sub && cd cwd_sub && "
        "/boot/echo cwd_ok > note.txt && /boot/ls && /boot/cat note.txt && "
        "/boot/rm note.txt",
        NULL, 0, out, sizeof(out), &len);
    (void)rmdir("/tmp/cwd_sub");
    if (rc != 0 || strcmp(out, "note.txt\ncwd_ok\n") != 0) {
        printf("[FAIL] shtest relative cwd ops rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* ls -l in working directory must lstat relative ./<entry> paths. */
    rc = run_sh_capture("cd /boot && /boot/ls -l", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || !strstr(out, " ls\n") || strstr(out, "?") != NULL) {
        printf("[FAIL] shtest cd /boot && ls -l rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* Process umask must persist across fork and execve. */
    mode_t prev_mask = umask(0077u);
    rc = run_sh_capture("cd /tmp && /boot/echo mask_ok > umask_check.txt", NULL,
                        0, out, sizeof(out), &len);
    (void)umask(prev_mask);
    struct stat st = {0};
    int sr = stat("/tmp/umask_check.txt", &st);
    (void)unlink("/tmp/umask_check.txt");
    if (rc != 0 || sr != 0 || (st.st_mode & 0777u) != 0600u) {
        printf("[FAIL] shtest umask across execve rc=%d sr=%d mode=%04o\n", rc,
               sr, (unsigned)(st.st_mode & 0777u));
        return -1;
    }

    printf("[OK] sh cwd and umask persistence across execve verified\n");
    return 0;
}

static int test_ps_getconf_tty_stty(void) {
    char out[2048];
    ssize_t len = 0;

    /* 1. ps -e, ps -f, ps -l, ps -p, ps -o, and invalid flag. */
    int rc = run_sh_capture("/boot/ps -e", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "PID") || !strstr(out, "CMD") ||
        !strstr(out, "sysproc") || !strstr(out, "ttyserv") ||
        !strstr(out, "vfsserv") || !strstr(out, "ps")) {
        printf("[FAIL] shtest ps -e rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/ps -f -e", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "PPID") || !strstr(out, "STIME") ||
        !strstr(out, "/boot/ps -f -e")) {
        printf("[FAIL] shtest ps -f -e rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/ps -p 1 -o pid=ID,comm=NAME", NULL, 0, out,
                        sizeof(out), &len);
    if (rc != 0 || !strstr(out, "ID") || !strstr(out, "NAME") ||
        !strstr(out, "sysproc") || strstr(out, "vfsserv") != NULL) {
        printf("[FAIL] shtest ps -p 1 -o rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/ps -p 1 -o pid=", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || strstr(out, "PID") != NULL || !strstr(out, "1")) {
        printf("[FAIL] shtest ps -o pid= header suppress rc=%d out='%s'\n", rc,
               out);
        return -1;
    }
    rc = run_sh_capture("/boot/ps -Z", NULL, 0, out, sizeof(out), &len);
    if (rc == 0) {
        printf("[FAIL] shtest ps -Z unexpectedly succeeded\n");
        return -1;
    }

    /* 2. getconf system, path, indeterminate, and strict POSIX rejection. */
    rc = run_sh_capture("/boot/getconf PAGE_SIZE && /boot/getconf PATH && "
                        "/boot/getconf PATH_MAX / && "
                        "/boot/getconf _POSIX_VERSION && "
                        "/boot/getconf _POSIX_SPAWN",
                        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "4096\n/boot\n256\n202405\nundefined\n") != 0) {
        printf("[FAIL] shtest getconf rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/getconf _NPROCESSORS_ONLN", NULL, 0, out,
                        sizeof(out), &len);
    if (rc == 0) {
        printf("[FAIL] shtest getconf accepted non-POSIX _NPROCESSORS_ONLN\n");
        return -1;
    }

    /* 3. tty (console vs pipe) and stty (-a and -g). */
    rc = run_sh_capture("/boot/tty", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "/dev/tty\n") != 0) {
        printf("[FAIL] shtest tty on console rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/tty", "x", 1u, out, sizeof(out), &len);
    if (rc != 1 || strcmp(out, "not a tty\n") != 0) {
        printf("[FAIL] shtest tty on pipe rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/stty -a", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "baud") || !strstr(out, "rows") ||
        !strstr(out, "columns") || !strstr(out, "icanon")) {
        printf("[FAIL] shtest stty -a rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/stty -g", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, ":")) {
        printf("[FAIL] shtest stty -g rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities ps, getconf, tty, and stty verified\n");
    return 0;
}

static int test_id_logname_nice_umask_ulimit(void) {
    char out[2048];
    ssize_t len = 0;

    /* 1. id and logname utilities. */
    int rc = run_sh_capture("/boot/id", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 ||
        strcmp(out, "uid=0(root) gid=0(root) groups=0(root)\n") != 0) {
        printf("[FAIL] shtest id rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture(
        "/boot/id -un && /boot/id -gn && /boot/id -u nobody && /boot/logname",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "root\nroot\n65534\nroot\n") != 0) {
        printf("[FAIL] shtest id flags/logname rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. nice and renice utilities. */
    rc = run_sh_capture("/boot/nice -n 5 /boot/nice", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || strcmp(out, "5\n") != 0) {
        printf("[FAIL] shtest nice -n 5 rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("/boot/renice -n 4 -p 1 && /boot/ps -p 1 -o nice= && "
                        "/boot/renice -n -4 -p 1",
                        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "4")) {
        printf("[FAIL] shtest renice rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 3. sh built-ins umask and ulimit. */
    rc = run_sh_capture("umask 0077 && umask && umask -S", NULL, 0, out,
                        sizeof(out), &len);
    if (rc != 0 || strcmp(out, "0077\nu=rwx,g=,o=\n") != 0) {
        printf("[FAIL] shtest umask builtin rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("ulimit -n && ulimit -S -n 16 && ulimit -n", NULL, 0,
                        out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "32\n16\n") != 0) {
        printf("[FAIL] shtest ulimit -n builtin rc=%d out='%s'\n", rc, out);
        return -1;
    }
    rc = run_sh_capture("ulimit -a", NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "open files") || !strstr(out, "stack size")) {
        printf("[FAIL] shtest ulimit -a rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU id, logname, nice, renice, umask, and ulimit verified\n");
    return 0;
}

static int test_xcu_file_utilities(void) {
    char out[2048];
    ssize_t len = 0;

    /* 1. touch, chmod, chown, chgrp, ln -s, readlink, realpath, rmdir. */
    int rc =
        run_sh_capture("/boot/mkdir /tmp/u33 && /boot/touch /tmp/u33/a.txt && "
                       "/boot/chmod 0750 /tmp/u33/a.txt && "
                       "/boot/chown nobody:wheel /tmp/u33/a.txt && "
                       "/boot/chgrp root /tmp/u33/a.txt && "
                       "/boot/ln -s /tmp/u33/a.txt /tmp/u33/link && "
                       "/boot/readlink /tmp/u33/link && "
                       "/boot/realpath /tmp/u33/link && "
                       "/boot/mkfifo /tmp/u33/pipe && "
                       "/boot/rm /tmp/u33/link /tmp/u33/pipe /tmp/u33/a.txt && "
                       "/boot/rmdir /tmp/u33",
                       NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "/tmp/u33/a.txt\n/tmp/u33/a.txt\n") != 0) {
        printf("[FAIL] shtest file utils 1 rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. basename, dirname, pathchk, df, du, dd. */
    rc = run_sh_capture(
        "/boot/basename /usr/bin/sh.c .c && /boot/dirname /usr/bin/sh && "
        "/boot/pathchk -p -P /tmp/good_name.txt && "
        "! /boot/pathchk -P /tmp/-bad",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "sh\n/usr/bin\n") != 0) {
        printf("[FAIL] shtest basename/dirname/pathchk rc=%d out='%s'\n", rc,
               out);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/df -k /tmp && /boot/du -s /boot && "
        "/boot/echo hello | /boot/dd bs=5 count=1 conv=ucase 2>/tmp/dd.err && "
        "/boot/rm /tmp/dd.err",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "tmpfs") || !strstr(out, "/boot") ||
        !strstr(out, "HELLO")) {
        printf("[FAIL] shtest df/du/dd rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU chmod, chown, chgrp, ln, mkfifo, readlink, realpath, "
           "touch, rmdir, df, du, dd, basename, dirname, and pathchk "
           "verified\n");
    return 0;
}

int main(void) {
    wait_for_vfs();
    if (test_pipelines_redirs_and_utils() != 0) {
        return 1;
    }
    if (test_quoting_pipefail_and_builtins() != 0) {
        return 1;
    }
    if (test_read_delim() != 0) {
        return 1;
    }
    if (test_cwd_across_execve() != 0) {
        return 1;
    }
    if (test_ps_getconf_tty_stty() != 0) {
        return 1;
    }
    if (test_id_logname_nice_umask_ulimit() != 0) {
        return 1;
    }
    if (test_xcu_file_utilities() != 0) {
        return 1;
    }
    return 0;
}
