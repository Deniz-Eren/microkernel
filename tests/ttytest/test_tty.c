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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_IPC_0006 VER_IPC_0013 */

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

static volatile sig_atomic_t g_winch_seen;

static void on_winch(int sig) {
    (void)sig;
    g_winch_seen = 1;
}

static int test_pty_and_ioctl(void) {
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        printf("[FAIL] PTY master open/grant/unlock failed errno=%d\n", errno);
        if (master >= 0) {
            (void)close(master);
        }
        return 1;
    }

    char pts_path[32] = {0};
    if (ptsname_r(master, pts_path, sizeof(pts_path)) != 0 ||
        strncmp(pts_path, "/dev/pts/", 9u) != 0) {
        printf("[FAIL] ptsname_r failed path='%s'\n", pts_path);
        (void)close(master);
        return 1;
    }

    int slave = open(pts_path, O_RDWR | O_CLOEXEC);
    if (slave < 0) {
        printf("[FAIL] open(%s) failed errno=%d\n", pts_path, errno);
        (void)close(master);
        return 1;
    }

    struct stat st = {0};
    char tname[32] = {0};
    if (!isatty(master) || !isatty(slave) || fstat(slave, &st) != 0 ||
        !S_ISCHR(st.st_mode) || ttyname_r(slave, tname, sizeof(tname)) != 0 ||
        strcmp(tname, pts_path) != 0) {
        printf("[FAIL] PTY isatty/fstat/ttyname mismatch\n");
        (void)close(slave);
        (void)close(master);
        return 1;
    }

    /* Test ioctl TIOCSCTTY, TIOCSWINSZ/TIOCGWINSZ, and SIGWINCH delivery. */
    g_winch_seen = 0;
    struct sigaction sa = {0};
    sa.sa_handler = on_winch;
    (void)sigaction(SIGWINCH, &sa, NULL);
    if (ioctl(slave, TIOCSCTTY, 0) != 0) {
        printf("[FAIL] ioctl(TIOCSCTTY) failed errno=%d\n", errno);
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    struct winsize ws = {.ws_row = 42u, .ws_col = 132u};
    struct winsize got_ws = {0};
    if (ioctl(master, TIOCSWINSZ, &ws) != 0 ||
        ioctl(slave, TIOCGWINSZ, &got_ws) != 0 || got_ws.ws_row != 42u ||
        got_ws.ws_col != 132u || !g_winch_seen) {
        printf("[FAIL] PTY winsize/SIGWINCH mismatch (%ux%u winch=%d)\n",
               got_ws.ws_row, got_ws.ws_col, (int)g_winch_seen);
        (void)close(slave);
        (void)close(master);
        return 1;
    }

    /* Slave -> master write with ONLCR, FIONREAD, and poll. */
    if (write(slave, "hi\n", 3u) != 3) {
        printf("[FAIL] PTY slave write failed\n");
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    int avail = 0;
    if (ioctl(master, FIONREAD, &avail) != 0 || avail != 4) {
        printf("[FAIL] PTY FIONREAD expected 4, got %d\n", avail);
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    struct pollfd mp = {.fd = master, .events = POLLIN, .revents = 0};
    if (poll(&mp, 1u, 100) != 1 || !(mp.revents & POLLIN)) {
        printf("[FAIL] PTY master poll POLLIN failed\n");
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    char mbuf[16] = {0};
    if (read(master, mbuf, sizeof(mbuf) - 1u) != 4 ||
        strcmp(mbuf, "hi\r\n") != 0) {
        printf("[FAIL] PTY master read got '%s'\n", mbuf);
        (void)close(slave);
        (void)close(master);
        return 1;
    }

    /* Master -> slave canonical input + echo back to master. */
    if (write(master, "ok\r", 3u) != 3) {
        printf("[FAIL] PTY master write failed\n");
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    char sbuf[16] = {0};
    if (read(slave, sbuf, sizeof(sbuf) - 1u) != 3 ||
        strcmp(sbuf, "ok\n") != 0) {
        printf("[FAIL] PTY slave read got '%s'\n", sbuf);
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    /* Drain echo from master. */
    memset(mbuf, 0, sizeof(mbuf));
    if (read(master, mbuf, sizeof(mbuf) - 1u) != 4 ||
        strcmp(mbuf, "ok\r\n") != 0) {
        printf("[FAIL] PTY echo mismatch got '%s'\n", mbuf);
        (void)close(slave);
        (void)close(master);
        return 1;
    }

    /* Fork child on PTY slave: Ctrl-C triggers ISIG SIGINT, exit yields EOF. */
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] PTY fork failed\n");
        (void)close(slave);
        (void)close(master);
        return 1;
    }
    if (pid == 0) {
        (void)close(master);
        (void)setsid();
        (void)ioctl(slave, TIOCSCTTY, 0);
        (void)write(slave, "R", 1u);
        for (;;) {
            (void)pause();
        }
    }
    (void)close(slave);

    char rdy = 0;
    if (read(master, &rdy, 1u) != 1 || rdy != 'R') {
        printf("[FAIL] PTY child ready byte failed\n");
        (void)kill(pid, SIGKILL);
        (void)waitpid(pid, NULL, 0);
        (void)close(master);
        return 1;
    }
    const char ctrl_c = 0x03;
    (void)write(master, &ctrl_c, 1u);
    int wst = 0;
    if (waitpid(pid, &wst, 0) != pid || !WIFSIGNALED(wst) ||
        WTERMSIG(wst) != SIGINT) {
        printf("[FAIL] PTY ISIG child status=0x%x\n", wst);
        (void)close(master);
        return 1;
    }

    /* Drain echoed "^C\r\n", then master must observe POLLHUP and 0-byte EOF.
     */
    int nb = 1;
    (void)ioctl(master, FIONBIO, &nb);
    char drain[16];
    while (read(master, drain, sizeof(drain)) > 0) {
    }
    nb = 0;
    (void)ioctl(master, FIONBIO, &nb);
    mp.events = POLLIN;
    mp.revents = 0;
    if (poll(&mp, 1u, 100) != 1 || !(mp.revents & POLLHUP) ||
        read(master, drain, sizeof(drain)) != 0) {
        printf("[FAIL] PTY master EOF/POLLHUP after slave exit failed\n");
        (void)close(master);
        return 1;
    }

    (void)close(master);
    printf("[OK] Ring 3 PTY (/dev/ptmx, /dev/pts/N), ioctl, SIGWINCH, ISIG, "
           "and POLLHUP verified\n");
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
    if (rc == 0) {
        rc = test_pty_and_ioctl();
    }
    close(pfd[0]);
    close(pfd[1]);
    return rc;
}
