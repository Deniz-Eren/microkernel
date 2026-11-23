/**
 * @file     nc.c
 * @brief    Ring 3 TCP/UDP netcat utility (client, -l listener, and -e exec)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: COMP_IPC_0019 */

static int write_all_fd(int fd, const uint8_t *buf, size_t len) {
    size_t off = 0u;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static int relay_stream(int sock, int timeout_ms) {
    int in_open = 1;
    int sock_open = 1;
    uint8_t buf[1024];

    while (sock_open) {
        struct pollfd pfds[2];
        nfds_t nfds = 0;
        int sock_idx = -1;
        int in_idx = -1;

        sock_idx = (int)nfds;
        pfds[nfds].fd = sock;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;

        if (in_open) {
            in_idx = (int)nfds;
            pfds[nfds].fd = STDIN_FILENO;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            nfds++;
        }

        int rc = poll(pfds, nfds, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 1;
        }
        if (rc == 0) {
            break;
        }

        if (sock_idx >= 0 &&
            (pfds[sock_idx].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            ssize_t n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                sock_open = 0;
            } else if (write_all_fd(STDOUT_FILENO, buf, (size_t)n) != 0) {
                return 1;
            }
        }
        if (in_idx >= 0 &&
            (pfds[in_idx].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                in_open = 0;
                (void)shutdown(sock, SHUT_WR);
            } else if (write_all_fd(sock, buf, (size_t)n) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int exec_on_sock(int sock, const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("nc: fork");
        return 1;
    }
    if (pid == 0) {
        (void)dup2(sock, STDIN_FILENO);
        (void)dup2(sock, STDOUT_FILENO);
        (void)dup2(sock, STDERR_FILENO);
        if (sock > STDERR_FILENO) {
            (void)close(sock);
        }
        char *av[] = {"/boot/sh", "-c", (char *)cmd, NULL};
        (void)execv("/boot/sh", av);
        _exit(127);
    }
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {
    }
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : 1;
}

static int run_tcp_listen(uint16_t port, int keep, const char *exec_cmd,
                          int timeout_ms) {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("nc: socket");
        return 1;
    }
    int one = 1;
    (void)setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(srv, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        perror("nc: bind");
        (void)close(srv);
        return 1;
    }
    if (listen(srv, 4) != 0) {
        perror("nc: listen");
        (void)close(srv);
        return 1;
    }

    int rc = 0;
    do {
        int cli = accept(srv, NULL, NULL);
        if (cli < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("nc: accept");
            rc = 1;
            break;
        }
        if (exec_cmd) {
            rc = exec_on_sock(cli, exec_cmd);
        } else {
            rc = relay_stream(cli, timeout_ms);
        }
        (void)close(cli);
    } while (keep);

    (void)close(srv);
    return rc;
}

static int run_udp_listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("nc: socket");
        return 1;
    }
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        perror("nc: bind");
        (void)close(fd);
        return 1;
    }
    uint8_t buf[1472];
    struct sockaddr_in peer = {0};
    socklen_t plen = sizeof(peer);
    ssize_t n =
        recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&peer, &plen);
    if (n > 0) {
        (void)write_all_fd(STDOUT_FILENO, buf, (size_t)n);
    }
    (void)close(fd);
    return (n < 0) ? 1 : 0;
}

static int run_client(const char *host, const char *port_s, int udp,
                      int timeout_ms) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;
    struct addrinfo *res = NULL;
    int gai = getaddrinfo(host, port_s, &hints, &res);
    if (gai != 0 || !res) {
        (void)fprintf(stderr, "nc: %s: %s\n", host, gai_strerror(gai));
        return 1;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        (void)close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        perror("nc: connect");
        return 1;
    }

    int rc = 0;
    if (udp) {
        uint8_t buf[1472];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n > 0 && send(fd, buf, (size_t)n, 0) < 0) {
            perror("nc: send");
            rc = 1;
        }
    } else {
        rc = relay_stream(fd, timeout_ms);
    }
    (void)close(fd);
    return rc;
}

int main(int argc, char **argv) {
    int listen_mode = 0;
    int keep = 0;
    int udp = 0;
    int timeout_ms = -1;
    const char *port_opt = NULL;
    const char *exec_cmd = NULL;
    int opt = 0;

    while ((opt = getopt(argc, argv, "e:klp:uw:")) != -1) {
        switch (opt) {
        case 'e':
            exec_cmd = optarg;
            break;
        case 'k':
            keep = 1;
            break;
        case 'l':
            listen_mode = 1;
            break;
        case 'p':
            port_opt = optarg;
            break;
        case 'u':
            udp = 1;
            break;
        case 'w':
            timeout_ms = atoi(optarg) * 1000;
            if (timeout_ms <= 0) {
                timeout_ms = 1000;
            }
            break;
        default:
            (void)fprintf(stderr,
                          "usage: nc [-u] [-w sec] host port | nc -l [-ku] "
                          "[-e cmd] [-p port] [port]\n");
            return 1;
        }
    }

    if (listen_mode) {
        const char *ps = port_opt;
        if (!ps && optind < argc) {
            ps = argv[optind];
        }
        if (!ps) {
            (void)fprintf(stderr, "nc: missing listen port\n");
            return 1;
        }
        long pv = strtol(ps, NULL, 10);
        if (pv <= 0 || pv > 65535) {
            (void)fprintf(stderr, "nc: invalid port: %s\n", ps);
            return 1;
        }
        return udp ? run_udp_listen((uint16_t)pv)
                   : run_tcp_listen((uint16_t)pv, keep, exec_cmd, timeout_ms);
    }

    if (argc - optind != 2) {
        (void)fprintf(stderr, "usage: nc [-u] [-w sec] host port\n");
        return 1;
    }
    return run_client(argv[optind], argv[optind + 1], udp, timeout_ms);
}
