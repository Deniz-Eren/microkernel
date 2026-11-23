/**
 * @file     test_sock.c
 * @brief    QEMU test for Ring 3 AF_UNIX sockets, accept4, and ppoll
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_IPC_0007 VER_IPC_0013 */

static int test_socketpair_and_shutdown(void) {
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0,
                   sv) != 0) {
        printf("[FAIL] socktest socketpair failed errno=%d\n", errno);
        return -1;
    }

    if ((fcntl(sv[0], F_GETFD) & FD_CLOEXEC) == 0 ||
        (fcntl(sv[1], F_GETFD) & FD_CLOEXEC) == 0 ||
        (fcntl(sv[0], F_GETFL) & O_NONBLOCK) == 0 ||
        (fcntl(sv[1], F_GETFL) & O_NONBLOCK) == 0) {
        printf("[FAIL] socktest socketpair flags missing\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    int stype = 0;
    socklen_t slen = sizeof(stype);
    if (getsockopt(sv[0], SOL_SOCKET, SO_TYPE, &stype, &slen) != 0 ||
        stype != SOCK_STREAM) {
        printf("[FAIL] socktest getsockopt SO_TYPE=%d\n", stype);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    char buf[32] = {0};
    errno = 0;
    if (recv(sv[1], buf, sizeof(buf), 0) != -1 || errno != EAGAIN) {
        printf("[FAIL] socktest nonblock empty recv errno=%d\n", errno);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    if (send(sv[0], "ping", 4, 0) != 4) {
        printf("[FAIL] socktest send failed errno=%d\n", errno);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    if (recv(sv[1], buf, 4, MSG_PEEK) != 4 || memcmp(buf, "ping", 4) != 0) {
        printf("[FAIL] socktest MSG_PEEK mismatch\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    if (read(sv[1], buf, 4) != 4 || memcmp(buf, "ping", 4) != 0) {
        printf("[FAIL] socktest read after peek mismatch\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    if (write(sv[1], "pong", 4) != 4 || recv(sv[0], buf, 4, 0) != 4 ||
        memcmp(buf, "pong", 4) != 0) {
        printf("[FAIL] socktest bidirectional write/recv failed\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    if (shutdown(sv[0], SHUT_WR) != 0) {
        printf("[FAIL] socktest shutdown SHUT_WR failed errno=%d\n", errno);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    if (recv(sv[1], buf, sizeof(buf), 0) != 0) {
        printf("[FAIL] socktest expected EOF after peer SHUT_WR\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    errno = 0;
    if (send(sv[0], "x", 1, MSG_NOSIGNAL) != -1 || errno != EPIPE) {
        printf("[FAIL] socktest send after SHUT_WR expected EPIPE got %d\n",
               errno);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    (void)close(sv[0]);
    (void)close(sv[1]);
    printf("[OK] AF_UNIX socketpair, SOCK_CLOEXEC/SOCK_NONBLOCK, MSG_PEEK, and "
           "shutdown verified\n");
    return 0;
}

static int test_stream_accept4_fork(void) {
    int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (srv < 0) {
        printf("[FAIL] socktest socket() failed errno=%d\n", errno);
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    (void)strlcpy(addr.sun_path, "/tmp/echo.sock", sizeof(addr.sun_path));

    if (bind(srv, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[FAIL] socktest bind failed errno=%d\n", errno);
        (void)close(srv);
        return -1;
    }

    /* Duplicate bind on same path must fail with EADDRINUSE. */
    int dup_s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dup_s >= 0) {
        errno = 0;
        if (bind(dup_s, (const struct sockaddr *)&addr, sizeof(addr)) != -1 ||
            errno != EADDRINUSE) {
            printf("[FAIL] socktest duplicate bind expected EADDRINUSE got "
                   "%d\n",
                   errno);
            (void)close(dup_s);
            (void)close(srv);
            return -1;
        }
        (void)close(dup_s);
    }

    if (listen(srv, 4) != 0) {
        printf("[FAIL] socktest listen failed errno=%d\n", errno);
        (void)close(srv);
        return -1;
    }

    int acc_opt = 0;
    socklen_t olen = sizeof(acc_opt);
    if (getsockopt(srv, SOL_SOCKET, SO_ACCEPTCONN, &acc_opt, &olen) != 0 ||
        acc_opt != 1) {
        printf("[FAIL] socktest SO_ACCEPTCONN=%d\n", acc_opt);
        (void)close(srv);
        return -1;
    }

    struct sockaddr_un got = {0};
    socklen_t glen = sizeof(got);
    if (getsockname(srv, (struct sockaddr *)&got, &glen) != 0 ||
        got.sun_family != AF_UNIX ||
        strcmp(got.sun_path, "/tmp/echo.sock") != 0) {
        printf("[FAIL] socktest getsockname mismatch '%s'\n", got.sun_path);
        (void)close(srv);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] socktest fork failed errno=%d\n", errno);
        (void)close(srv);
        return -1;
    }
    if (pid == 0) {
        (void)close(srv);
        int cli = socket(AF_UNIX, SOCK_STREAM, 0);
        if (cli < 0) {
            _exit(10);
        }
        struct sockaddr_un caddr = {0};
        caddr.sun_family = AF_UNIX;
        (void)strlcpy(caddr.sun_path, "/tmp/cli.sock", sizeof(caddr.sun_path));
        if (bind(cli, (const struct sockaddr *)&caddr, sizeof(caddr)) != 0) {
            _exit(11);
        }
        if (connect(cli, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
            _exit(12);
        }
        struct sockaddr_un paddr = {0};
        socklen_t plen = sizeof(paddr);
        if (getpeername(cli, (struct sockaddr *)&paddr, &plen) != 0 ||
            strcmp(paddr.sun_path, "/tmp/echo.sock") != 0) {
            _exit(13);
        }
        if (send(cli, "hello-unix", 10, 0) != 10) {
            _exit(14);
        }
        char rbuf[32] = {0};
        if (recv(cli, rbuf, sizeof(rbuf), 0) != 10 ||
            strcmp(rbuf, "HELLO-UNIX") != 0) {
            _exit(15);
        }
        (void)close(cli);
        _exit(0);
    }

    struct sockaddr_un peer = {0};
    socklen_t peerlen = sizeof(peer);
    int cfd = accept4(srv, (struct sockaddr *)&peer, &peerlen,
                      SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (cfd < 0) {
        printf("[FAIL] socktest accept4 failed errno=%d\n", errno);
        (void)close(srv);
        return -1;
    }
    if ((fcntl(cfd, F_GETFD) & FD_CLOEXEC) == 0 ||
        (fcntl(cfd, F_GETFL) & O_NONBLOCK) == 0) {
        printf("[FAIL] socktest accept4 atomic flags missing\n");
        (void)close(cfd);
        (void)close(srv);
        return -1;
    }
    if (peer.sun_family != AF_UNIX ||
        strcmp(peer.sun_path, "/tmp/cli.sock") != 0) {
        printf("[FAIL] socktest accept4 peer path '%s'\n", peer.sun_path);
        (void)close(cfd);
        (void)close(srv);
        return -1;
    }

    struct pollfd pfd = {.fd = cfd, .events = POLLIN, .revents = 0};
    if (poll(&pfd, 1, 2000) != 1 || !(pfd.revents & POLLIN)) {
        printf("[FAIL] socktest poll on accepted fd failed rev=0x%x\n",
               pfd.revents);
        (void)close(cfd);
        (void)close(srv);
        return -1;
    }

    char msg[32] = {0};
    ssize_t rn = recv(cfd, msg, sizeof(msg) - 1u, 0);
    if (rn != 10 || strcmp(msg, "hello-unix") != 0) {
        printf("[FAIL] socktest server recv '%s' (%ld)\n", msg, (long)rn);
        (void)close(cfd);
        (void)close(srv);
        return -1;
    }
    if (send(cfd, "HELLO-UNIX", 10, 0) != 10) {
        printf("[FAIL] socktest server send failed errno=%d\n", errno);
        (void)close(cfd);
        (void)close(srv);
        return -1;
    }
    (void)close(cfd);
    (void)close(srv);

    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        printf("[FAIL] socktest client child failed st=0x%x\n", st);
        return -1;
    }

    printf("[OK] AF_UNIX bind, listen, connect, and accept4 atomic flags "
           "across fork verified\n");
    return 0;
}

static int test_dgram_and_multi_ppoll(void) {
    int d1 = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int d2 = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (d1 < 0 || d2 < 0) {
        printf("[FAIL] socktest SOCK_DGRAM socket failed\n");
        if (d1 >= 0) {
            (void)close(d1);
        }
        if (d2 >= 0) {
            (void)close(d2);
        }
        return -1;
    }

    struct sockaddr_un a1 = {.sun_family = AF_UNIX};
    struct sockaddr_un a2 = {.sun_family = AF_UNIX};
    (void)strlcpy(a1.sun_path, "/tmp/dg1.sock", sizeof(a1.sun_path));
    (void)strlcpy(a2.sun_path, "/tmp/dg2.sock", sizeof(a2.sun_path));

    if (bind(d1, (const struct sockaddr *)&a1, sizeof(a1)) != 0 ||
        bind(d2, (const struct sockaddr *)&a2, sizeof(a2)) != 0) {
        printf("[FAIL] socktest SOCK_DGRAM bind failed errno=%d\n", errno);
        (void)close(d1);
        (void)close(d2);
        return -1;
    }

    if (sendto(d1, "dgram-msg", 9, 0, (const struct sockaddr *)&a2,
               sizeof(a2)) != 9) {
        printf("[FAIL] socktest sendto failed errno=%d\n", errno);
        (void)close(d1);
        (void)close(d2);
        return -1;
    }

    /* Multi-server ppoll across AF_UNIX socket + /dev/pipe descriptor. */
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] socktest pipe2 failed\n");
        (void)close(d1);
        (void)close(d2);
        return -1;
    }

    struct pollfd pfds[2] = {
        {.fd = pfd[0], .events = POLLIN, .revents = 0},
        {.fd = d2, .events = POLLIN, .revents = 0},
    };
    struct timespec to = {.tv_sec = 0, .tv_nsec = 500000000L};
    int pr = ppoll(pfds, 2, &to, NULL);
    if (pr != 1 || pfds[0].revents != 0 || !(pfds[1].revents & POLLIN)) {
        printf("[FAIL] socktest multi-server ppoll pr=%d r0=0x%x r1=0x%x\n", pr,
               pfds[0].revents, pfds[1].revents);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)close(d1);
        (void)close(d2);
        return -1;
    }

    char rbuf[32] = {0};
    struct sockaddr_un from = {0};
    socklen_t flen = sizeof(from);
    ssize_t dn = recvfrom(d2, rbuf, sizeof(rbuf) - 1u, 0,
                          (struct sockaddr *)&from, &flen);
    if (dn != 9 || strcmp(rbuf, "dgram-msg") != 0 ||
        strcmp(from.sun_path, "/tmp/dg1.sock") != 0) {
        printf("[FAIL] socktest recvfrom got '%s' from '%s'\n", rbuf,
               from.sun_path);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)close(d1);
        (void)close(d2);
        return -1;
    }

    (void)close(pfd[0]);
    (void)close(pfd[1]);
    (void)close(d1);
    (void)close(d2);

    printf("[OK] AF_UNIX SOCK_DGRAM sendto/recvfrom and multi-server ppoll "
           "(sock + pipe) verified\n");
    return 0;
}

static int test_scm_rights_fd_passing(void) {
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        printf("[FAIL] scm_rights socketpair failed\n");
        return -1;
    }
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] scm_rights pipe2 failed\n");
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] scm_rights fork failed\n");
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)close(sv[0]);
        (void)close(sv[1]);
        return -1;
    }
    if (pid == 0) {
        (void)close(sv[0]);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        char tag = 0;
        struct iovec iov = {.iov_base = &tag, .iov_len = 1u};
        union {
            char buf[CMSG_SPACE(sizeof(int))];
            struct cmsghdr align;
        } u = {0};
        struct msghdr msg = {0};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = u.buf;
        msg.msg_controllen = (socklen_t)sizeof(u.buf);
        ssize_t n = recvmsg(sv[1], &msg, MSG_CMSG_CLOEXEC);
        if (n != 1 || tag != 'F') {
            _exit(2);
        }
        struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
        if (!cm || cm->cmsg_level != SOL_SOCKET ||
            cm->cmsg_type != SCM_RIGHTS) {
            _exit(3);
        }
        int rfd = -1;
        memcpy(&rfd, CMSG_DATA(cm), sizeof(int));
        if (rfd < 0 || (fcntl(rfd, F_GETFD) & FD_CLOEXEC) == 0) {
            _exit(4);
        }
        char buf[16] = {0};
        ssize_t rn = read(rfd, buf, sizeof(buf) - 1u);
        (void)close(rfd);
        (void)close(sv[1]);
        _exit((rn == 10 && strcmp(buf, "FD_PASS_OK") == 0) ? 0 : 5);
    }

    (void)close(sv[1]);
    if (write(pfd[1], "FD_PASS_OK", 10u) != 10) {
        printf("[FAIL] scm_rights pipe write failed\n");
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)close(sv[0]);
        (void)waitpid(pid, NULL, 0);
        return -1;
    }
    char tag = 'F';
    struct iovec iov = {.iov_base = &tag, .iov_len = 1u};
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u = {0};
    struct msghdr msg = {0};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = (socklen_t)sizeof(u.buf);
    struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
    cm->cmsg_level = SOL_SOCKET;
    cm->cmsg_type = SCM_RIGHTS;
    cm->cmsg_len = (socklen_t)CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cm), &pfd[0], sizeof(int));

    ssize_t sn = sendmsg(sv[0], &msg, 0);
    /* Close sender's copy immediately to prove server-side refcount dup. */
    (void)close(pfd[0]);
    (void)close(pfd[1]);
    (void)close(sv[0]);

    int st = 0;
    if (sn != 1 || waitpid(pid, &st, 0) != pid || !WIFEXITED(st) ||
        WEXITSTATUS(st) != 0) {
        printf("[FAIL] scm_rights failed sn=%ld st=0x%x\n", (long)sn, st);
        return -1;
    }
    printf(
        "[OK] Ring 3 AF_UNIX SCM_RIGHTS cross-process FD passing verified\n");
    return 0;
}

int main(void) {
    if (test_socketpair_and_shutdown() != 0) {
        return 1;
    }
    if (test_stream_accept4_fork() != 0) {
        return 1;
    }
    if (test_dgram_and_multi_ppoll() != 0) {
        return 1;
    }
    if (test_scm_rights_fd_passing() != 0) {
        return 1;
    }
    return 0;
}
