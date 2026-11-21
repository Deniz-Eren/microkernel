/**
 * @file     test_tty.c
 * @brief    QEMU test for Ring 3 ttyserv, <termios.h>, stdin, and multi-ppoll
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_IPC_0006 */

static int g_tty_conn = -1;

static int inject_tty_bytes(const char *s, size_t n) {
    if (g_tty_conn < 0) {
        for (int tries = 0; tries < 200; tries++) {
            long ns = OpenConnection(1, 1);
            if (ns >= 0) {
                name_msg_t q = {0};
                q.type = NAME_MSG_QUERY;
                (void)strlcpy(q.path, "/dev/tty", sizeof(q.path));
                name_reply_t nr = {0};
                long r = SendMsg((int)ns, &q, sizeof(q), &nr, sizeof(nr));
                CloseConnection((int)ns);
                if (r >= 0 && nr.err == 0 && nr.pid > 0 && nr.chanid > 0) {
                    g_tty_conn = (int)OpenConnection((uint32_t)nr.pid,
                                                     (uint32_t)nr.chanid);
                    break;
                }
            }
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
        }
    }
    if (g_tty_conn < 0) {
        return -1;
    }
    tty_req_t req = {0};
    tty_rep_t rep = {0};
    req.op = TTY_OP_INJECT;
    req.len = (uint32_t)n;
    memcpy(req.u.data, s, n);
    long r = SendMsg(g_tty_conn, &req, sizeof(req), &rep, sizeof(rep));
    return (r < 0 || rep.err < 0) ? -1 : 0;
}

typedef struct {
    char buf[64];
    ssize_t nread;
} reader_arg_t;

static void *stdin_reader_thread(void *arg) {
    reader_arg_t *ra = (reader_arg_t *)arg;
    ra->nread = read(STDIN_FILENO, ra->buf, sizeof(ra->buf) - 1u);
    if (ra->nread > 0) {
        ra->buf[ra->nread] = '\0';
    }
    return NULL;
}

static int test_isatty_and_termios_basics(int pipe_rfd, struct termios *orig) {
    if (isatty(STDIN_FILENO) != 1 || isatty(STDOUT_FILENO) != 1) {
        printf("[FAIL] isatty(stdin/stdout) expected 1\n");
        return 1;
    }
    errno = 0;
    if (isatty(pipe_rfd) != 0 || errno != ENOTTY) {
        printf("[FAIL] isatty(pipe) expected ENOTTY, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (isatty(-1) != 0 || errno != EBADF) {
        printf("[FAIL] isatty(-1) expected EBADF, got %d\n", errno);
        return 1;
    }

    struct termios dummy = {0};
    errno = 0;
    if (tcgetattr(pipe_rfd, &dummy) != -1 || errno != ENOTTY) {
        printf("[FAIL] tcgetattr(pipe) expected ENOTTY, got %d\n", errno);
        return 1;
    }
    if (tcgetattr(STDIN_FILENO, orig) != 0 || !(orig->c_lflag & ICANON) ||
        cfgetispeed(orig) != B115200 || cfgetospeed(orig) != B115200) {
        printf("[FAIL] tcgetattr(stdin) defaults mismatch\n");
        return 1;
    }
    struct termios copy = *orig;
    if (cfsetispeed(&copy, B9600) != 0 || cfsetospeed(&copy, B9600) != 0 ||
        cfgetispeed(&copy) != B9600 || cfgetospeed(&copy) != B9600) {
        printf("[FAIL] cfset/cfget speed mismatch\n");
        return 1;
    }
    errno = 0;
    if (tcsetattr(STDIN_FILENO, 99, orig) != -1 || errno != EINVAL) {
        printf("[FAIL] tcsetattr bad action expected EINVAL\n");
        return 1;
    }
    if (tcdrain(STDIN_FILENO) != 0 || tcflush(STDIN_FILENO, TCIFLUSH) != 0) {
        printf("[FAIL] tcdrain/tcflush failed errno=%d\n", errno);
        return 1;
    }

    int tfd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (tfd < 0 || isatty(tfd) != 1) {
        printf("[FAIL] open(/dev/tty) failed errno=%d\n", errno);
        return 1;
    }
    struct stat st = {0};
    if (fstat(tfd, &st) != 0 || !S_ISCHR(st.st_mode)) {
        printf("[FAIL] fstat(/dev/tty) expected S_ISCHR\n");
        close(tfd);
        return 1;
    }
    errno = 0;
    if (lseek(tfd, 0, SEEK_CUR) != (off_t)-1 || errno != ESPIPE) {
        printf("[FAIL] lseek(/dev/tty) expected ESPIPE\n");
        close(tfd);
        return 1;
    }
    close(tfd);
    printf("[OK] isatty, /dev/tty, tcgetattr/tcsetattr, and speeds verified\n");
    return 0;
}

static int test_canonical_line_editing_and_blocking_read(void) {
    reader_arg_t ra = {0};
    pthread_t th;
    if (pthread_create(&th, NULL, stdin_reader_thread, &ra) != 0) {
        printf("[FAIL] pthread_create stdin_reader failed\n");
        return 1;
    }
    struct timespec d = {.tv_sec = 0, .tv_nsec = 20000000L};
    clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);

    /* VKILL (^U) discards "bad", VERASE (0x7F) erases 'x', ICRNL maps \r->\n.
     */
    const char input[] = "bad\x15helx\x7flo\r";
    if (inject_tty_bytes(input, sizeof(input) - 1u) != 0) {
        printf("[FAIL] TTY_OP_INJECT failed\n");
        return 1;
    }
    if (pthread_join(th, NULL) != 0 || ra.nread != 6 ||
        strcmp(ra.buf, "hello\n") != 0) {
        printf("[FAIL] canonical read got n=%ld buf='%s'\n", (long)ra.nread,
               ra.buf);
        return 1;
    }

    /* VEOF (^D) on empty line must produce 0-byte EOF read. */
    const char eof_ch = 0x04;
    if (inject_tty_bytes(&eof_ch, 1u) != 0) {
        printf("[FAIL] TTY_OP_INJECT EOF failed\n");
        return 1;
    }
    char eb[8] = {0};
    if (read(STDIN_FILENO, eb, sizeof(eb)) != 0) {
        printf("[FAIL] VEOF expected 0-byte read\n");
        return 1;
    }
    printf("[OK] canonical line discipline (VERASE, VKILL, ICRNL, VEOF) "
           "verified\n");
    return 0;
}

static int test_raw_mode_and_multi_server_ppoll(int pfd[2],
                                                const struct termios *orig) {
    struct termios raw = *orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1u;
    raw.c_cc[VTIME] = 0u;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        printf("[FAIL] tcsetattr(raw) failed errno=%d\n", errno);
        return 1;
    }

    struct pollfd pfds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0},
        {.fd = pfd[0], .events = POLLIN, .revents = 0},
    };
    struct timespec short_to = {.tv_sec = 0, .tv_nsec = 20000000L};
    int r0 = ppoll(pfds, 2u, &short_to, NULL);
    if (r0 != 0) {
        printf("[FAIL] multi-server ppoll timeout expected 0, got %d\n", r0);
        return 1;
    }

    /* Raw 1-byte input wakes multi-server ppoll on TTY without newline. */
    const char z = 'Z';
    if (inject_tty_bytes(&z, 1u) != 0) {
        printf("[FAIL] raw inject failed\n");
        return 1;
    }
    struct timespec wait_to = {.tv_sec = 0, .tv_nsec = 100000000L};
    int r1 = ppoll(pfds, 2u, &wait_to, NULL);
    if (r1 != 1 || !(pfds[0].revents & POLLIN) || pfds[1].revents != 0) {
        printf("[FAIL] multi-server ppoll TTY ready mismatch r=%d (%x,%x)\n",
               r1, pfds[0].revents, pfds[1].revents);
        return 1;
    }
    char got = 0;
    if (read(STDIN_FILENO, &got, 1u) != 1 || got != 'Z') {
        printf("[FAIL] raw stdin read got '%c'\n", got);
        return 1;
    }

    /* Pipe write wakes multi-server ppoll on pipe FD while TTY is idle. */
    if (write(pfd[1], "P", 1u) != 1) {
        printf("[FAIL] pipe write failed\n");
        return 1;
    }
    int r2 = ppoll(pfds, 2u, &wait_to, NULL);
    if (r2 != 1 || pfds[0].revents != 0 || !(pfds[1].revents & POLLIN)) {
        printf("[FAIL] multi-server ppoll pipe ready mismatch r=%d (%x,%x)\n",
               r2, pfds[0].revents, pfds[1].revents);
        return 1;
    }
    char pgot = 0;
    if (read(pfd[0], &pgot, 1u) != 1 || pgot != 'P') {
        printf("[FAIL] pipe readback mismatch\n");
        return 1;
    }

    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
    printf("[OK] raw mode and multi-server ppoll (TTY + pipe) verified\n");
    return 0;
}

int main(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] ttytest pipe2 failed errno=%d\n", errno);
        return 1;
    }
    struct termios orig = {0};
    int rc = test_isatty_and_termios_basics(pfd[0], &orig);
    if (rc == 0) {
        rc = test_canonical_line_editing_and_blocking_read();
    }
    if (rc == 0) {
        rc = test_raw_mode_and_multi_server_ppoll(pfd, &orig);
    }
    close(pfd[0]);
    close(pfd[1]);
    return rc;
}
