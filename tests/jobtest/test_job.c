/**
 * @file     test_job.c
 * @brief    QEMU test for TTY ISIG Ctrl-C, process groups, and core utilities
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/vm.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_PROC_0006 */

static void sleep_ms(long ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000L,
        .tv_nsec = (ms % 1000L) * 1000000L,
    };
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

static int connect_ttyserv(void) {
    long ns = OpenConnection(1u, 1u);
    if (ns < 0) {
        return -1;
    }
    name_msg_t q = {0};
    q.type = NAME_MSG_QUERY;
    (void)strlcpy(q.path, "/dev/tty", sizeof(q.path));
    name_reply_t rep = {0};
    long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
    CloseConnection((int)ns);
    if (r < 0 || rep.err != 0) {
        return -1;
    }
    return (int)OpenConnection(rep.pid, rep.chanid);
}

static int inject_tty_bytes(const void *data, size_t len) {
    int conn = connect_ttyserv();
    if (conn < 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_INJECT;
    req.len = (uint32_t)len;
    memcpy(req.u.data, data, len);
    long r = SendMsg(conn, &req, sizeof(req), &rep, sizeof(rep));
    CloseConnection(conn);
    return (r >= 0 && rep.err == 0) ? 0 : -1;
}

static int run_sh_capture(const char *cmd, char *out, size_t cap) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
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
        (void)dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        char *argv[] = {"sh", "-c", (char *)cmd, NULL};
        execvp("sh", argv);
        _exit(127);
    }
    close(pfd[1]);
    size_t pos = 0;
    while (pos + 1u < cap) {
        ssize_t n = read(pfd[0], out + pos, cap - 1u - pos);
        if (n <= 0) {
            break;
        }
        pos += (size_t)n;
    }
    out[pos] = '\0';
    close(pfd[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        return -1;
    }
    return WEXITSTATUS(st);
}

static int test_pgid_setsid_wait_kill(void) {
    pid_t self = getpid();
    if (getpgrp() != self || getpgid(0) != self || getsid(0) != self) {
        printf("[FAIL] initial pgid/sid mismatch\n");
        return -1;
    }

    /* Child 1 creates a new process group; Child 2 stays in parent's pgrp. */
    pid_t c1 = fork();
    if (c1 < 0) {
        return -1;
    }
    if (c1 == 0) {
        if (setpgid(0, 0) != 0) {
            _exit(2);
        }
        for (;;) {
            sleep_ms(10);
        }
    }
    (void)setpgid(c1, c1);

    pid_t c2 = fork();
    if (c2 < 0) {
        (void)kill(c1, SIGKILL);
        (void)waitpid(c1, NULL, 0);
        return -1;
    }
    if (c2 == 0) {
        sleep_ms(20);
        _exit(17);
    }

    /* Wait until c1 has updated its pgid, then verify waitpid(0) reaps c2. */
    for (int i = 0; i < 50 && getpgid(c1) != c1; i++) {
        sleep_ms(5);
    }
    if (getpgid(c1) != c1 || getpgid(c2) != self) {
        printf("[FAIL] child pgid setup failed\n");
        (void)kill(c1, SIGKILL);
        (void)waitpid(c1, NULL, 0);
        (void)waitpid(c2, NULL, 0);
        return -1;
    }

    int st2 = 0;
    pid_t w2 = waitpid(0, &st2, 0);
    if (w2 != c2 || !WIFEXITED(st2) || WEXITSTATUS(st2) != 17) {
        printf("[FAIL] waitpid(0) expected c2 exit 17 got %d st=%d\n", (int)w2,
               st2);
        (void)kill(c1, SIGKILL);
        (void)waitpid(c1, NULL, 0);
        return -1;
    }

    /* Signal c1's process group with kill(-c1, SIGTERM) and reap via -c1. */
    if (kill(-c1, SIGTERM) != 0) {
        printf("[FAIL] kill(-pgid, SIGTERM) failed errno=%d\n", errno);
        (void)kill(c1, SIGKILL);
        (void)waitpid(c1, NULL, 0);
        return -1;
    }
    int st1 = 0;
    pid_t w1 = waitpid(-c1, &st1, 0);
    if (w1 != c1 || !WIFSIGNALED(st1) || WTERMSIG(st1) != SIGTERM) {
        printf("[FAIL] waitpid(-pgid) expected SIGTERM got %d st=%d\n", (int)w1,
               st1);
        return -1;
    }

    /* Verify setsid() in a forked non-leader child. */
    pid_t cs = fork();
    if (cs < 0) {
        return -1;
    }
    if (cs == 0) {
        pid_t sid = setsid();
        if (sid != getpid() || getsid(0) != sid || getpgrp() != sid) {
            _exit(1);
        }
        _exit(0);
    }
    int sts = 0;
    if (waitpid(cs, &sts, 0) != cs || !WIFEXITED(sts) ||
        WEXITSTATUS(sts) != 0) {
        printf("[FAIL] setsid child failed\n");
        return -1;
    }

    printf("[OK] setpgid, getpgid, setsid, waitpid(-pgid), and kill(-pgid) "
           "verified\n");
    return 0;
}

static int test_tty_isig_and_tcsetpgrp(void) {
    /* Wait until earlier prio-100 tests (including ttytest) have exited. */
    struct __k_sys_stats ks = {0};
    for (int i = 0; i < 400; i++) {
        if (SysStats(&ks) == 0 && ks.live_procs <= 9u) {
            break;
        }
        sleep_ms(10);
    }
    struct termios orig = {0};
    if (tcgetattr(STDIN_FILENO, &orig) != 0) {
        printf("[FAIL] jobtest tcgetattr failed\n");
        return -1;
    }
    struct termios tio = orig;
    tio.c_lflag |= ISIG;
    tio.c_cc[VINTR] = 0x03u;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &tio) != 0) {
        printf("[FAIL] jobtest tcsetattr failed\n");
        return -1;
    }

    pid_t orig_fg = tcgetpgrp(STDIN_FILENO);
    pid_t child = fork();
    if (child < 0) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return -1;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        for (;;) {
            sleep_ms(10);
        }
    }
    (void)setpgid(child, child);
    for (int i = 0; i < 50 && getpgid(child) != child; i++) {
        sleep_ms(5);
    }

    if (tcsetpgrp(STDIN_FILENO, child) != 0 ||
        tcgetpgrp(STDIN_FILENO) != child) {
        printf("[FAIL] tcsetpgrp/tcgetpgrp mismatch\n");
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return -1;
    }

    /* Inject Ctrl-C (0x03) into ttyserv: child pgrp must die with SIGINT. */
    uint8_t ctrl_c = 0x03u;
    if (inject_tty_bytes(&ctrl_c, 1u) != 0) {
        printf("[FAIL] ttyserv Ctrl-C inject failed\n");
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        (void)tcsetpgrp(STDIN_FILENO, orig_fg > 0 ? orig_fg : getpgrp());
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return -1;
    }

    int st = 0;
    pid_t w = waitpid(child, &st, 0);
    (void)tcsetpgrp(STDIN_FILENO, orig_fg > 0 ? orig_fg : getpgrp());
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    if (w != child || !WIFSIGNALED(st) || WTERMSIG(st) != SIGINT) {
        printf("[FAIL] ISIG Ctrl-C expected SIGINT got w=%d st=%d\n", (int)w,
               st);
        return -1;
    }

    printf("[OK] TTY ISIG Ctrl-C SIGINT and tcgetpgrp/tcsetpgrp verified\n");
    return 0;
}

static int test_core_utilities(void) {
    char out[256];

    /* 1. mkdir -p, cp, mv, rm -rf via /boot/sh. */
    int rc = run_sh_capture("/boot/mkdir -p /tmp/jt/a/b && "
                            "echo hello world > /tmp/jt/a/b/f1.txt && "
                            "/boot/cp /tmp/jt/a/b/f1.txt /tmp/jt/a/f2.txt && "
                            "/boot/mv /tmp/jt/a/f2.txt /tmp/jt/f3.txt && "
                            "/boot/cat /tmp/jt/f3.txt && "
                            "/boot/rm -rf /tmp/jt",
                            out, sizeof(out));
    if (rc != 0 || strcmp(out, "hello world\n") != 0) {
        printf("[FAIL] mkdir/cp/mv/rm pipeline rc=%d out='%s'\n", rc, out);
        return -1;
    }
    struct stat st = {0};
    if (stat("/tmp/jt", &st) == 0) {
        printf("[FAIL] rm -rf left /tmp/jt behind\n");
        return -1;
    }

    /* 2. uname -sm. */
    rc = run_sh_capture("/boot/uname -sm", out, sizeof(out));
    if (rc != 0 || strcmp(out, "Microkernel x86_64\n") != 0) {
        printf("[FAIL] uname -sm rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 3. env -i FOO=bar. */
    rc = run_sh_capture("/boot/env -i FOO=bar", out, sizeof(out));
    if (rc != 0 || strcmp(out, "FOO=bar\n") != 0) {
        printf("[FAIL] env -i rc=%d out='%s'\n", rc, out);
        return -1;
    }

    /* 4. wc -lwc and head -n 2. */
    rc = run_sh_capture(
        "echo $'one two\\nthree\\nfour five' | /boot/head -n 2 | /boot/wc -lwc",
        out, sizeof(out));
    if (rc != 0 || strcmp(out, "2 3 14\n") != 0) {
        printf("[FAIL] head/wc pipeline rc=%d out='%s'\n", rc, out);
        return -1;
    }

    printf("[OK] Ring 3 core utilities (mkdir, rm, cp, mv, uname, env, wc, "
           "head) verified\n");
    return 0;
}

int main(void) {
    if (test_pgid_setsid_wait_kill() != 0) {
        return 1;
    }
    if (test_tty_isig_and_tcsetpgrp() != 0) {
        return 1;
    }
    if (test_core_utilities() != 0) {
        return 1;
    }
    return 0;
}
