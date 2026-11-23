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

/* need:: VER_LIBC_0007 VER_LIBC_0018 VER_LIBC_0019 VER_LIBC_0021 */
/* need:: VER_LIBC_0022 VER_LIBC_0023 VER_LIBC_0024 VER_LIBC_0025 */
/* need:: VER_LIBC_0026 VER_PROC_0008 VER_PROC_0009 VER_IPC_0010 */

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
    char out[2048];
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

static int test_wait_builtin(void) {
    char out[256];
    ssize_t len = 0;

    int rc = run_sh_capture("/bin/true; wait; echo rc=$?", NULL, 0, out,
                            sizeof(out), &len);
    if (rc != 0 || strcmp(out, "rc=0\n") != 0) {
        printf("[FAIL] shtest wait-all rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture("wait -- 99999; echo rc=$?", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || strcmp(out, "rc=127\n") != 0) {
        printf("[FAIL] shtest wait-unknown rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture("wait notapid; echo rc=$?", NULL, 0, out, sizeof(out),
                        &len);
    if (rc != 0 || strcmp(out, "rc=127\n") != 0) {
        printf("[FAIL] shtest wait-invalid rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] sh wait builtin reaped children and reported statuses\n");
    return 0;
}

static int test_shell_grammar_and_expansions(void) {
    char out[1024];
    ssize_t len = 0;

    /* 1. if/elif/else, for/break/continue, while/until, $((arith)), case. */
    int rc = run_sh_capture(
        "x=2; "
        "if /boot/test $x -eq 1; then echo one; "
        "elif /boot/test $x -eq 2; then echo two; "
        "else echo other; fi; "
        "sum=0; i=1; "
        "while /boot/test $i -le 5; do "
        "  sum=$((sum + i)); i=$((i + 1)); "
        "done; echo sum=$sum; "
        "u=3; until /boot/test $u -eq 0; do u=$((u - 1)); done; echo u=$u; "
        "res=''; for v in a b c d; do "
        "  if /boot/test $v = b; then continue; fi; "
        "  if /boot/test $v = d; then break; fi; "
        "  res=\"$res$v\"; "
        "done; echo for=$res; "
        "case foo.c in *.h) echo hdr;; *.c|*.cc) echo csrc;; *) echo no;; esac",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "two\nsum=15\nu=0\nfor=ac\ncsrc\n") != 0) {
        printf("[FAIL] shtest compound grammar rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. Subshell isolation, brace group redirection, functions + return. */
    rc = run_sh_capture(
        "v=outer; (v=inner; echo $v); echo $v; "
        "{ echo g1; echo g2; } > /tmp/sh_grp.txt; /boot/cat /tmp/sh_grp.txt; "
        "/boot/rm /tmp/sh_grp.txt; "
        "add() { echo \"$#:$1+$2=$(($1 + $2))\"; return 7; }; "
        "add 3 4; echo fnrc=$?",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 ||
        strcmp(out, "inner\nouter\ng1\ng2\n2:3+4=7\nfnrc=7\n") != 0) {
        printf("[FAIL] shtest subshell/brace/func rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 3. $(...), parameter expansions (#, :-, :=, :+, #, ##, %, %%). */
    rc = run_sh_capture(
        "p=/usr/local/bin/sh; unset u; "
        "echo $(echo sub) ${#p} ${u:-def} ${u:=set} $u ${u:+alt} "
        "${p#*/} ${p##*/} ${p%/*} [${p%%/*}]",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "sub 17 def set set alt usr/local/bin/sh sh "
                               "/usr/local/bin []\n") != 0) {
        printf("[FAIL] shtest param/cmdsub rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 4. Here-documents (expanded, quoted, tab-stripped) + glob + trap. */
    (void)mkdir("/tmp/shg", 0755u);
    int f1 = open("/tmp/shg/a.txt", O_CREAT | O_WRONLY | O_CLOEXEC, 0644u);
    int f2 = open("/tmp/shg/b.txt", O_CREAT | O_WRONLY | O_CLOEXEC, 0644u);
    if (f1 >= 0) {
        (void)close(f1);
    }
    if (f2 >= 0) {
        (void)close(f2);
    }

    rc = run_sh_capture("who=mk\n"
                        "/boot/cat <<EOF\n"
                        "hi $who\n"
                        "EOF\n"
                        "/boot/cat <<'EOF'\n"
                        "lit $who\n"
                        "EOF\n"
                        "/boot/cat <<-EOF\n"
                        "\t\tindented\n"
                        "\tEOF\n"
                        "echo /tmp/shg/*.txt\n"
                        "set -f; echo /tmp/shg/*.txt; set +f\n"
                        "trap 'echo exit_trap_ok' EXIT\n",
                        NULL, 0, out, sizeof(out), &len);
    (void)unlink("/tmp/shg/a.txt");
    (void)unlink("/tmp/shg/b.txt");
    (void)rmdir("/tmp/shg");
    if (rc != 0 || strcmp(out, "hi mk\nlit $who\nindented\n"
                               "/tmp/shg/a.txt /tmp/shg/b.txt\n"
                               "/tmp/shg/*.txt\nexit_trap_ok\n") != 0) {
        printf("[FAIL] shtest heredoc/glob/trap rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] sh compound grammar, functions, expansions, heredocs, glob, "
           "and trap verified\n");
    return 0;
}

static int test_shell_builtins_complete(void) {
    char out[1024];
    ssize_t len = 0;

    /* Create dot-sourced script in /tmp. */
    FILE *df = fopen("/tmp/sh_dot.sh", "w");
    if (!df) {
        printf("[FAIL] create /tmp/sh_dot.sh failed\n");
        return -1;
    }
    fputs("DOT_VAL=from_dot\nreturn 0\nDOT_VAL=unreachable\n", df);
    (void)fclose(df);

    int rc = run_sh_capture(
        ": ignored; echo colon=$?; "
        ". /tmp/sh_dot.sh; echo dot=$DOT_VAL; "
        "alias hi='echo aliased'; eval 'hi ok'; unalias hi; "
        "echo() { command echo \"fn:$1\"; }; "
        "echo a; command echo b; unset -f echo; "
        "command -v /boot/echo; "
        "hash echo; hash -r; "
        "set -- -a val -b rest; "
        "out=''; while getopts ':a:b' opt; do out=\"$out$opt:$OPTARG,\"; done; "
        "shift $((OPTIND - 1)); echo \"go=$out rem=$1\"; "
        "(exec > /tmp/sh_ex.txt; echo exec_redir); /boot/cat /tmp/sh_ex.txt; "
        "/boot/rm /tmp/sh_ex.txt; "
        "(exec /boot/echo exec_cmd); "
        "readonly RO=1; RO=2 2>/tmp/sh_ro.err; echo ro_rc=$?; "
        "/boot/rm /tmp/sh_ro.err; "
        "times",
        NULL, 0, out, sizeof(out), &len);
    (void)unlink("/tmp/sh_dot.sh");
    if (rc != 0 || !strstr(out, "colon=0\n") ||
        !strstr(out, "dot=from_dot\n") || !strstr(out, "aliased ok\n") ||
        !strstr(out, "fn:a\nb\n") || !strstr(out, "/boot/echo\n") ||
        !strstr(out, "go=a:val,b:, rem=rest\n") ||
        !strstr(out, "exec_redir\nexec_cmd\nro_rc=1\n") || !strstr(out, "m") ||
        !strstr(out, "s\n")) {
        printf("[FAIL] shtest builtins rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] sh built-ins dot, colon, alias, command, eval, exec, "
           "getopts, hash, shift, times verified\n");
    return 0;
}

static int test_regex_find_stream_utils(void) {
    char out[1024];
    ssize_t len = 0;

    /* 1. grep (-E, -F, -i, -n, -v, -c, -q, -x). */
    int rc =
        run_sh_capture("/boot/printf 'Alpha 10\\nbeta 20\\nALPHA 30\\n' | "
                       "/boot/grep -inE '^alpha [0-9]+$' && "
                       "/boot/printf 'a.b\\naxb\\n' | /boot/grep -Fc 'a.b' && "
                       "/boot/printf 'keep\\ndrop\\n' | /boot/grep -vx drop && "
                       "/boot/printf 'ok\\n' | /boot/grep -q ok && echo q=0",
                       NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "1:Alpha 10\n3:ALPHA 30\n1\nkeep\nq=0\n") != 0) {
        printf("[FAIL] shtest grep rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. sed (s///g, backref, -E, -n/p, y///, $, d). */
    rc = run_sh_capture(
        "/boot/printf 'foo=1\\nbar=2\\nbaz=3\\n' | "
        "/boot/sed -E -e 's/^([a-z]+)=([0-9]+)$/\\2:\\1/' -e '2d' "
        "-e '$s/baz/END/' && "
        "/boot/printf 'abc\\n' | /boot/sed 'y/abc/XYZ/'",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "1:foo\n3:END\nXYZ\n") != 0) {
        printf("[FAIL] shtest sed rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 3. find (-name, -type, -print0, -exec +) and xargs (-0, -n, -I). */
    rc = run_sh_capture(
        "/boot/mkdir /tmp/f43 /tmp/f43/sub && "
        "/boot/touch /tmp/f43/a.c /tmp/f43/sub/b.c /tmp/f43/c.txt && "
        "/boot/find /tmp/f43 -type f -name '*.c' -print0 | "
        "/boot/xargs -0 -n 1 /boot/basename && "
        "/boot/find /tmp/f43 -name '*.txt' -exec /boot/basename {} + && "
        "/boot/printf 'x y z\\n' | /boot/xargs -n 2 /boot/echo && "
        "/boot/printf 'one\\ntwo\\n' | /boot/xargs -I '{}' /boot/echo '[{}]' "
        "&& "
        "/boot/rm /tmp/f43/a.c /tmp/f43/sub/b.c /tmp/f43/c.txt && "
        "/boot/rmdir /tmp/f43/sub /tmp/f43",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || !strstr(out, "a.c\n") || !strstr(out, "b.c\n") ||
        !strstr(out, "c.txt\n") || !strstr(out, "x y\nz\n") ||
        !strstr(out, "[one]\n[two]\n")) {
        printf("[FAIL] shtest find/xargs rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 4. expr (arithmetic, comparison, BRE : match/capture) and printf. */
    rc = run_sh_capture(
        "/boot/expr '(' 2 + 3 ')' '*' 4 && "
        "/boot/expr 'abc123' : '[a-z]*\\([0-9]*\\)' && "
        "/boot/expr 5 '<' 9 && "
        "/boot/printf '[%04d:%x:%.1f:%d]\\n' 7 255 3.5 \"'A\" && "
        "/boot/printf '%b' 'hi\\tthere\\cIGNORED'",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 ||
        strcmp(out, "20\n123\n1\n[0007:ff:3.5:65]\nhi\tthere") != 0) {
        printf("[FAIL] shtest expr/printf rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities grep, sed, find, xargs, expr, and printf "
           "verified\n");
    return 0;
}

static int test_sort_field_checksum_utils(void) {
    char out[256];
    ssize_t len = 0;

    /* 1. sort (-n, -r, -u, -t/-k, -c) and uniq (-c, -d, -u). */
    int rc = run_sh_capture(
        "/boot/printf 'b:10\\na:2\\nb:10\\nc:3\\n' | "
        "/boot/sort -t: -k2,2n -u && "
        "/boot/printf 'x\\nx\\ny\\nz\\nz\\nz\\n' | /boot/uniq -c && "
        "/boot/printf '1\\n2\\n3\\n' | /boot/sort -c",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "a:2\nc:3\nb:10\n2 x\n1 y\n3 z\n") != 0) {
        printf("[FAIL] shtest sort/uniq rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. cut (-d/-f/-s, -c) and tr (ranges, classes, -d, -s). */
    rc = run_sh_capture(
        "/boot/printf 'a:b:c\\nnodlm\\nx:y:z\\n' | /boot/cut -d: -f1,3 -s && "
        "/boot/printf 'abcdef\\n' | /boot/cut -c2-4 && "
        "/boot/printf 'hello   123\\n' | /boot/tr '[:lower:]' '[:upper:]' | "
        "/boot/tr -d '0-9' | /boot/tr -s ' '",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "a:c\nx:z\nbcd\nHELLO \n") != 0) {
        printf("[FAIL] shtest cut/tr rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 3. tail (-n, -n +2, -c), tee (-a), paste (-d, -s), and join. */
    rc = run_sh_capture(
        "/boot/printf '1\\n2\\n3\\n4\\n' | /boot/tail -n 2 && "
        "/boot/printf '1\\n2\\n3\\n' | /boot/tail -n +3 && "
        "/boot/printf 'abcd' | /boot/tail -c 2 && "
        "/boot/printf '\\n' && "
        "/boot/printf 'k1 v1\\n' | /boot/tee /tmp/j1.txt && "
        "/boot/printf 'k2 v2\\n' | /boot/tee -a /tmp/j1.txt && "
        "/boot/printf 'k1 x1\\nk3 x3\\n' > /tmp/j2.txt && "
        "/boot/join -a 1 -e NONE -o 0,1.2,2.2 /tmp/j1.txt /tmp/j2.txt && "
        "/boot/printf 'p\\nq\\n' | /boot/paste -s -d ',' && "
        "/boot/rm /tmp/j1.txt /tmp/j2.txt",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 ||
        strcmp(out, "3\n4\n3\ncd\nk1 v1\nk2 v2\nk1 v1 x1\nk2 v2 NONE\np,q\n") !=
            0) {
        printf("[FAIL] shtest tail/tee/paste/join rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 4. cksum POSIX.1-2024 CRC-32 golden vectors (empty + 123456789). */
    rc = run_sh_capture("/boot/printf '' | /boot/cksum && "
                        "/boot/printf '123456789' | /boot/cksum",
                        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "4294967295 0\n930766865 9\n") != 0) {
        printf("[FAIL] shtest cksum rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities sort, uniq, cut, tr, tail, tee, paste, join, "
           "and cksum verified\n");
    return 0;
}

static int test_diff_cmp_comm_patch_utils(void) {
    char out[512];
    ssize_t len = 0;

    /* 1. cmp (-s, -l) and comm (-12, -23, default). */
    int rc = run_sh_capture(
        "/boot/printf 'a\\nb\\nc\\n' > /tmp/c1.txt && "
        "/boot/printf 'a\\nc\\nd\\n' > /tmp/c2.txt && "
        "/boot/cmp -s /tmp/c1.txt /tmp/c1.txt && "
        "if /boot/cmp -s /tmp/c1.txt /tmp/c2.txt; then exit 1; fi && "
        "/boot/comm -12 /tmp/c1.txt /tmp/c2.txt && "
        "/boot/comm -23 /tmp/c1.txt /tmp/c2.txt && "
        "/boot/comm -13 /tmp/c1.txt /tmp/c2.txt && "
        "/boot/rm /tmp/c1.txt /tmp/c2.txt",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "a\nc\nb\nd\n") != 0) {
        printf("[FAIL] shtest cmp/comm rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 2. diff (-u, -c, normal) + patch (-o, -R) full round-trips. */
    rc = run_sh_capture(
        "/boot/printf 'one\\ntwo\\nthree\\nfour\\n' > /tmp/da.txt && "
        "/boot/printf 'one\\nTWO\\nthree\\nfive\\nsix\\n' > /tmp/db.txt && "
        "/boot/diff -u /tmp/da.txt /tmp/db.txt > /tmp/d.patch; "
        "/boot/patch -o /tmp/du.out /tmp/da.txt < /tmp/d.patch && "
        "/boot/cmp -s /tmp/db.txt /tmp/du.out && "
        "/boot/patch -R -o /tmp/dr.out /tmp/du.out < /tmp/d.patch && "
        "/boot/cmp -s /tmp/da.txt /tmp/dr.out && "
        "/boot/diff /tmp/da.txt /tmp/db.txt > /tmp/dn.patch; "
        "/boot/patch -o /tmp/dn.out /tmp/da.txt < /tmp/dn.patch && "
        "/boot/cmp -s /tmp/db.txt /tmp/dn.out && "
        "/boot/diff -c /tmp/da.txt /tmp/db.txt > /tmp/dc.patch; "
        "/boot/patch -o /tmp/dc.out /tmp/da.txt < /tmp/dc.patch && "
        "/boot/cmp -s /tmp/db.txt /tmp/dc.out && "
        "/boot/rm /tmp/da.txt /tmp/db.txt /tmp/d.patch /tmp/du.out "
        "/tmp/dr.out /tmp/dn.patch /tmp/dn.out /tmp/dc.patch /tmp/dc.out && "
        "/boot/echo PATCH_OK",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "PATCH_OK\n") != 0) {
        printf("[FAIL] shtest diff/patch rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities diff, cmp, comm, and patch verified\n");
    return 0;
}

static int test_pax_archive_utility(void) {
    char out[256];
    ssize_t len = 0;

    int rc = run_sh_capture(
        "/boot/mkdir /tmp/pxs /tmp/pxs/sub /tmp/pxu /tmp/pxc /tmp/pxp && "
        "/boot/printf 'alpha\\n' > /tmp/pxs/a.txt && "
        "/boot/printf 'beta\\n' > /tmp/pxs/sub/b.txt && "
        "/boot/ln -s a.txt /tmp/pxs/link.txt && "
        "(cd /tmp/pxs && /boot/pax -w -f /tmp/p.tar .) && "
        "(cd /tmp/pxu && /boot/pax -r -f /tmp/p.tar) && "
        "/boot/cmp -s /tmp/pxs/a.txt /tmp/pxu/a.txt && "
        "/boot/cmp -s /tmp/pxs/sub/b.txt /tmp/pxu/sub/b.txt && "
        "/boot/readlink /tmp/pxu/link.txt && "
        "(cd /tmp/pxs && /boot/pax -w -x cpio -f /tmp/p.cpio a.txt sub) && "
        "(cd /tmp/pxc && /boot/pax -r -s '/a\\.txt/renamed.txt/' -f "
        "/tmp/p.cpio) "
        "&& "
        "/boot/cmp -s /tmp/pxs/a.txt /tmp/pxc/renamed.txt && "
        "/boot/cmp -s /tmp/pxs/sub/b.txt /tmp/pxc/sub/b.txt && "
        "(cd /tmp/pxs && /boot/pax -r -w a.txt sub /tmp/pxp) && "
        "/boot/cmp -s /tmp/pxs/sub/b.txt /tmp/pxp/sub/b.txt && "
        "/boot/rm -r /tmp/pxs /tmp/pxu /tmp/pxc /tmp/pxp /tmp/p.tar "
        "/tmp/p.cpio "
        "&& /boot/echo PAX_OK",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "a.txt\nPAX_OK\n") != 0) {
        printf("[FAIL] shtest pax rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utility pax (ustar, cpio, -s, and -rw copy-pass) "
           "verified\n");
    return 0;
}

static int test_split_format_inspect_utils(void) {
    char out[512];
    ssize_t len = 0;

    int rc = run_sh_capture(
        "/boot/printf 'l1\\nl2\\nl3\\nl4\\nl5\\n' > /tmp/sp.txt && "
        "/boot/split -l 2 /tmp/sp.txt /tmp/sp_ && "
        "/boot/cat /tmp/sp_aa /tmp/sp_ab /tmp/sp_ac > /tmp/sp_all.txt && "
        "/boot/cmp -s /tmp/sp.txt /tmp/sp_all.txt && "
        "/boot/printf 'abcdefgh' > /tmp/sb.txt && "
        "/boot/split -b 3 -a 3 /tmp/sb.txt /tmp/sb_ && "
        "/boot/cat /tmp/sb_aaa /tmp/sb_aab /tmp/sb_aac > /tmp/sb_all.txt && "
        "/boot/cmp -s /tmp/sb.txt /tmp/sb_all.txt && "
        "/boot/printf 'hdr\\n---\\nbody\\n' > /tmp/cs.txt && "
        "/boot/csplit -s -f /tmp/cs_ /tmp/cs.txt '/---/' && "
        "/boot/cat /tmp/cs_00 && /boot/cat /tmp/cs_01 && "
        "/boot/rm /tmp/sp.txt /tmp/sp_aa /tmp/sp_ab /tmp/sp_ac /tmp/sp_all.txt "
        "/tmp/sb.txt /tmp/sb_aaa /tmp/sb_aab /tmp/sb_aac /tmp/sb_all.txt "
        "/tmp/cs.txt /tmp/cs_00 /tmp/cs_01",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "hdr\n---\nbody\n") != 0) {
        printf("[FAIL] shtest split/csplit rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/printf 'ab cd efgh\\n' | /boot/fold -s -w 6 && "
        "/boot/printf 'a\\tb\\n' | /boot/expand -t 4 && "
        "/boot/printf '    x    y\\n' | /boot/unexpand -a -t 4 | "
        "/boot/tr '\\t' 'T'",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "ab cd \nefgh\na   b\nTxT y\n") != 0) {
        printf("[FAIL] shtest fold/expand/unexpand rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/printf '\\001\\002hello\\000ab\\000world!\\003' | "
        "/boot/strings -n 4 -t x && "
        "/boot/printf 'b c\\na b\\nc d\\n' | /boot/tsort",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "2 hello\nb world!\na\nb\nc\nd\n") != 0) {
        printf("[FAIL] shtest strings/tsort rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/printf '#!/boot/sh\\necho hi\\n' > /tmp/f_sh.txt && "
        "/boot/printf 'hello world\\n' > /tmp/f_txt.txt && "
        "/boot/file /boot/echo /tmp /tmp/f_sh.txt /tmp/f_txt.txt && "
        "/boot/printf 'x\\ny\\n' > /tmp/pr1.txt && "
        "/boot/printf '1\\n2\\n' > /tmp/pr2.txt && "
        "/boot/pr -t -n:2 /tmp/pr1.txt && "
        "/boot/pr -t -m -s, /tmp/pr1.txt /tmp/pr2.txt && "
        "/boot/rm /tmp/f_sh.txt /tmp/f_txt.txt /tmp/pr1.txt /tmp/pr2.txt",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 ||
        strstr(out, "/boot/echo: ELF 64-bit LSB executable, x86-64\n") ==
            NULL ||
        strstr(out, "/tmp: directory\n") == NULL ||
        strstr(out, "/tmp/f_sh.txt: POSIX shell script text\n") == NULL ||
        strstr(out, "/tmp/f_txt.txt: ASCII text\n") == NULL ||
        strstr(out, " 1:x\n 2:y\nx,1\ny,2\n") == NULL) {
        printf("[FAIL] shtest file/pr rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities split, csplit, fold, expand, unexpand, "
           "strings, tsort, file, and pr verified\n");
    return 0;
}

static int test_awk_and_ed_utils(void) {
    char out[512];
    ssize_t len = 0;

    int rc =
        run_sh_capture("/boot/printf 'a:10\\nb:20\\na:5\\n' | "
                       "/boot/awk -F: -v bonus=1 '"
                       "function sq(n){return n*n} "
                       "{sum[$1]+=$2; tot+=$2} "
                       "END{printf \"%d %d %d %d\\n\", sum[\"a\"], sum[\"b\"], "
                       "tot+bonus, int(sqrt(sq(6)))}' && "
                       "/boot/printf 'foo123bar\\n' | "
                       "/boot/awk '{gsub(/[0-9]+/, \"[&]\"); "
                       "m=match($0, /\\[123\\]/); "
                       "print $0, m, RLENGTH}'",
                       NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "15 20 36 6\nfoo[123]bar 4 5\n") != 0) {
        printf("[FAIL] shtest awk rc=%d out='%s'\n", rc, out);
        return -1;
    }

    rc = run_sh_capture(
        "/boot/printf 'alpha\\nbeta\\ngamma\\n' > /tmp/ed_t.txt && "
        "/boot/printf '2s/beta/BETA/\\n"
        "1a\\ninserted\\n.\\n"
        "g/gamma/s/gamma/GAMMA/\\n"
        "1,2j\\n"
        "u\\n"
        "w\\nq\\n' | /boot/ed -s /tmp/ed_t.txt && "
        "/boot/cat /tmp/ed_t.txt && "
        "/boot/rm /tmp/ed_t.txt",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "alpha\ninserted\nBETA\nGAMMA\n") != 0) {
        printf("[FAIL] shtest ed rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities awk and ed verified\n");
    return 0;
}

static int test_bc_and_m4_utils(void) {
    char out[512];
    ssize_t len = 0;

    int rc = run_sh_capture(
        "/boot/printf '"
        "2^100\\n"
        "scale=10; 1/7\\n"
        "scale=6; sqrt(2)\\n"
        "define f(n) { if (n <= 1) return (1); return (n * f(n - 1)); }\\n"
        "f(10)\\n"
        "obase=16; 255\\n' | /boot/bc && "
        "/boot/printf 'scale=6; 4*a(1)\\n' | /boot/bc -l",
        NULL, 0, out, sizeof(out), &len);
    if (rc != 0 || strcmp(out, "1267650600228229401496703205376\n"
                               ".1428571428\n"
                               "1.414213\n"
                               "3628800\n"
                               "FF\n"
                               "3.141592\n") != 0) {
        printf("[FAIL] shtest bc rc=%d out='%s'\n", rc, out);
        return -1;
    }

    const char *m4_in = "define(`HI', `hello $1')dnl\n"
                        "pushdef(`X', `1')pushdef(`X', `2')X popdef(`X')X\n"
                        "HI(world) VER eval((2+3)*4) incr(9) len(abcd) "
                        "translit(abc, ac, AC)\n"
                        "divert(1)TAIL\n"
                        "divert(0)HEAD\n";
    rc = run_sh_capture("/boot/m4 -D VER=8", m4_in, strlen(m4_in), out,
                        sizeof(out), &len);
    if (rc != 0 ||
        strcmp(out, "2 1\nhello world 8 20 10 4 AbC\nHEAD\nTAIL\n") != 0) {
        printf("[FAIL] shtest m4 rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] XCU utilities bc and m4 verified\n");
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
    if (test_wait_builtin() != 0) {
        return 1;
    }
    if (test_shell_grammar_and_expansions() != 0) {
        return 1;
    }
    if (test_shell_builtins_complete() != 0) {
        return 1;
    }
    if (test_regex_find_stream_utils() != 0) {
        return 1;
    }
    if (test_sort_field_checksum_utils() != 0) {
        return 1;
    }
    if (test_diff_cmp_comm_patch_utils() != 0) {
        return 1;
    }
    if (test_pax_archive_utility() != 0) {
        return 1;
    }
    if (test_split_format_inspect_utils() != 0) {
        return 1;
    }
    if (test_awk_and_ed_utils() != 0) {
        return 1;
    }
    if (test_bc_and_m4_utils() != 0) {
        return 1;
    }
    return 0;
}
