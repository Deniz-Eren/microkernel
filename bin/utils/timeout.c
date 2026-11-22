/**
 * @file     timeout.c
 * @brief    POSIX.1-2024 XCU timeout utility (run command with a time limit)
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_LIBC_0008 */

static int parse_duration(const char *arg, struct timespec *out, int *is_zero) {
    if (!arg || !*arg) {
        return -1;
    }
    const char *p = arg;
    unsigned long long whole = 0ULL;
    int any_digit = 0;
    while (*p >= '0' && *p <= '9') {
        any_digit = 1;
        unsigned long long d = (unsigned long long)(*p - '0');
        if (whole > (ULLONG_MAX - d) / 10ULL) {
            return -1;
        }
        whole = whole * 10ULL + d;
        p++;
    }
    unsigned long long frac_ns = 0ULL;
    if (*p == '.') {
        p++;
        unsigned long long place = 100000000ULL;
        while (*p >= '0' && *p <= '9') {
            any_digit = 1;
            if (place > 0ULL) {
                frac_ns += (unsigned long long)(*p - '0') * place;
                place /= 10ULL;
            }
            p++;
        }
    }
    if (!any_digit) {
        return -1;
    }
    unsigned long long mult = 1ULL;
    if (*p != '\0') {
        if (p[1] != '\0') {
            return -1;
        }
        switch (*p) {
        case 's':
            mult = 1ULL;
            break;
        case 'm':
            mult = 60ULL;
            break;
        case 'h':
            mult = 3600ULL;
            break;
        case 'd':
            mult = 86400ULL;
            break;
        default:
            return -1;
        }
    }
    if (whole > (unsigned long long)LONG_MAX / mult) {
        return -1;
    }
    unsigned long long total_ns = frac_ns * mult;
    unsigned long long extra_sec = total_ns / 1000000000ULL;
    unsigned long long rem_ns = total_ns % 1000000000ULL;
    unsigned long long sec = whole * mult;
    if (sec > (unsigned long long)LONG_MAX - extra_sec) {
        return -1;
    }
    sec += extra_sec;
    out->tv_sec = (time_t)sec;
    out->tv_nsec = (long)rem_ns;
    *is_zero = (sec == 0ULL && rem_ns == 0ULL);
    return 0;
}

static void send_target_signal(pid_t pid, int foreground, int sig) {
    if (!foreground) {
        if (kill(-pid, sig) == 0 || errno != ESRCH) {
            return;
        }
    }
    (void)kill(pid, sig);
}

static int wait_child_timed(pid_t pid, const sigset_t *chld_set,
                            const struct timespec *limit, int *wstatus) {
    struct timespec start = {0};
    if (limit) {
        (void)clock_gettime(CLOCK_MONOTONIC, &start);
    }
    for (;;) {
        pid_t r = waitpid(pid, wstatus, WNOHANG);
        if (r == pid) {
            return 1;
        }
        if (r < 0 && errno != EINTR) {
            return -1;
        }
        if (!limit) {
            int sr = sigwaitinfo(chld_set, (siginfo_t *)0);
            if (sr < 0 && errno != EINTR) {
                return -1;
            }
            continue;
        }
        struct timespec now = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &now);
        long long elapsed_ns =
            (long long)(now.tv_sec - start.tv_sec) * 1000000000LL +
            (long long)(now.tv_nsec - start.tv_nsec);
        long long total_ns =
            (long long)limit->tv_sec * 1000000000LL + (long long)limit->tv_nsec;
        if (elapsed_ns >= total_ns) {
            return 0;
        }
        long long rem_ns = total_ns - elapsed_ns;
        struct timespec rem = {.tv_sec = (time_t)(rem_ns / 1000000000LL),
                               .tv_nsec = (long)(rem_ns % 1000000000LL)};
        int sr = sigtimedwait(chld_set, (siginfo_t *)0, &rem);
        if (sr < 0) {
            if (errno == EAGAIN) {
                pid_t r2 = waitpid(pid, wstatus, WNOHANG);
                return (r2 == pid) ? 1 : 0;
            }
            if (errno != EINTR) {
                return -1;
            }
        }
    }
}

int main(int argc, char **argv) {
    int foreground = 0;
    int preserve_status = 0;
    int send_sig = SIGTERM;
    int has_kill_after = 0;
    int kill_zero = 0;
    struct timespec kill_ts = {0};

    int argi = 1;
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] != '\0') {
        if (strcmp(argv[argi], "--") == 0) {
            argi++;
            break;
        }
        const char *p = argv[argi] + 1;
        while (*p) {
            char opt = *p++;
            if (opt == 'f') {
                foreground = 1;
            } else if (opt == 'p') {
                preserve_status = 1;
            } else if (opt == 's' || opt == 'k') {
                const char *val = (*p != '\0') ? p : (const char *)0;
                if (!val) {
                    if (argi + 1 >= argc) {
                        (void)fprintf(stderr, "timeout: option -%c needs arg\n",
                                      opt);
                        return 125;
                    }
                    val = argv[++argi];
                }
                if (opt == 's') {
                    if (str2sig(val, &send_sig) != 0) {
                        (void)fprintf(stderr, "timeout: invalid signal: %s\n",
                                      val);
                        return 125;
                    }
                } else {
                    if (parse_duration(val, &kill_ts, &kill_zero) != 0) {
                        (void)fprintf(stderr, "timeout: invalid duration: %s\n",
                                      val);
                        return 125;
                    }
                    has_kill_after = !kill_zero;
                }
                break;
            } else {
                (void)fprintf(stderr, "timeout: invalid option: -%c\n", opt);
                return 125;
            }
        }
        argi++;
    }

    if (argi + 1 >= argc) {
        (void)fprintf(
            stderr,
            "usage: timeout [-f] [-k time] [-p] [-s signal] time utility "
            "[arg...]\n");
        return 125;
    }

    struct timespec limit_ts = {0};
    int limit_zero = 0;
    if (parse_duration(argv[argi], &limit_ts, &limit_zero) != 0) {
        (void)fprintf(stderr, "timeout: invalid duration: %s\n", argv[argi]);
        return 125;
    }
    argi++;

    sigset_t chld_set;
    sigset_t old_set;
    (void)sigemptyset(&chld_set);
    (void)sigaddset(&chld_set, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &chld_set, &old_set) < 0) {
        perror("timeout: sigprocmask");
        return 125;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("timeout: fork");
        return 125;
    }
    if (pid == 0) {
        (void)sigprocmask(SIG_SETMASK, &old_set, (sigset_t *)0);
        if (!foreground) {
            (void)setpgid(0, 0);
        }
        (void)execvp(argv[argi], &argv[argi]);
        int exec_err = errno;
        (void)fprintf(stderr, "timeout: cannot execute %s\n", argv[argi]);
        _exit(exec_err == ENOENT ? 127 : 126);
    }
    if (!foreground) {
        (void)setpgid(pid, pid);
    }

    int wstatus = 0;
    int timed_out = 0;
    int sent_kill = 0;
    int wr = wait_child_timed(
        pid, &chld_set, limit_zero ? (const struct timespec *)0 : &limit_ts,
        &wstatus);
    if (wr < 0) {
        return 125;
    }
    if (wr == 0) {
        timed_out = 1;
        send_target_signal(pid, foreground, send_sig);
        if (has_kill_after) {
            int kr = wait_child_timed(pid, &chld_set, &kill_ts, &wstatus);
            if (kr < 0) {
                return 125;
            }
            if (kr == 0) {
                sent_kill = 1;
                send_target_signal(pid, foreground, SIGKILL);
                if (wait_child_timed(pid, &chld_set, (const struct timespec *)0,
                                     &wstatus) < 0) {
                    return 125;
                }
            }
        } else {
            if (wait_child_timed(pid, &chld_set, (const struct timespec *)0,
                                 &wstatus) < 0) {
                return 125;
            }
        }
    }

    if (timed_out && !preserve_status && !sent_kill) {
        return 124;
    }
    if (WIFEXITED(wstatus)) {
        return WEXITSTATUS(wstatus);
    }
    if (WIFSIGNALED(wstatus)) {
        return 128 + WTERMSIG(wstatus);
    }
    return timed_out ? 124 : 0;
}
