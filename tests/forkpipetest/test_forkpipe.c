/**
 * @file     test_forkpipe.c
 * @brief    QEMU integration test for fork, execve, pipe2, dup3, and ppoll
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_PROC_0005 VER_IPC_0003 */

static volatile sig_atomic_t g_ppoll_sig;
static int g_cow_shared = 0x11112222;

static void ppoll_sig_handler(int sig) {
    if (sig == SIGUSR1) {
        g_ppoll_sig = 1;
    }
}

static int test_fork_cow(void) {
    pid_t parent_pid = getpid();
    int stack_val = 1234;
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork() failed (errno=%d)\n", errno);
        return 1;
    }
    if (pid == 0) {
        if (getppid() != parent_pid || getpid() == parent_pid) {
            _exit(10);
        }
        g_cow_shared = 0x55556666;
        stack_val = 9876;
        _exit((g_cow_shared == 0x55556666 && stack_val == 9876) ? 33 : 11);
    }

    int st = 0;
    pid_t reaped = waitpid(pid, &st, 0);
    if (reaped != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 33) {
        printf("[FAIL] fork COW waitpid reaped=%d st=0x%x\n", (int)reaped, st);
        return 1;
    }
    if (g_cow_shared != 0x11112222 || stack_val != 1234) {
        printf("[FAIL] fork COW leaked child writes into parent\n");
        return 1;
    }
    printf("[OK] fork COW isolated parent and child (child=%d exit=33)\n",
           (int)pid);
    return 0;
}

static int test_pipe2_dup3_execve(void) {
    /* 1. pipe2 with O_CLOEXEC and O_NONBLOCK. */
    int nb[2] = {-1, -1};
    if (pipe2(nb, O_CLOEXEC | O_NONBLOCK) != 0) {
        printf("[FAIL] pipe2(O_CLOEXEC|O_NONBLOCK) failed (errno=%d)\n", errno);
        return 1;
    }
    if (fcntl(nb[0], F_GETFD) != FD_CLOEXEC ||
        fcntl(nb[1], F_GETFD) != FD_CLOEXEC) {
        printf("[FAIL] pipe2 did not set FD_CLOEXEC atomically\n");
        return 1;
    }
    char tmp = 0;
    errno = 0;
    if (read(nb[0], &tmp, 1) != -1 || errno != EAGAIN) {
        printf("[FAIL] O_NONBLOCK empty pipe read did not yield EAGAIN\n");
        return 1;
    }

    /* 2. dup3 error contracts (same fd and invalid flags -> EINVAL). */
    errno = 0;
    if (dup3(nb[0], nb[0], 0) != -1 || errno != EINVAL) {
        printf("[FAIL] dup3(same, same) did not yield EINVAL\n");
        return 1;
    }
    errno = 0;
    if (dup3(nb[0], 15, O_NONBLOCK) != -1 || errno != EINVAL) {
        printf("[FAIL] dup3(invalid flags) did not yield EINVAL\n");
        return 1;
    }
    if (dup3(nb[0], 15, O_CLOEXEC) != 15 || fcntl(15, F_GETFD) != FD_CLOEXEC) {
        printf("[FAIL] dup3(O_CLOEXEC) failed\n");
        return 1;
    }

    /* 3. Create data pipe, dup2 write end to fd 10 (no CLOEXEC), fork+execve.
     */
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] pipe2(data) failed\n");
        return 1;
    }
    if (dup2(pfd[1], 10) != 10 || fcntl(10, F_GETFD) != 0) {
        printf("[FAIL] dup2(pfd[1], 10) failed\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork for execve failed\n");
        return 1;
    }
    if (pid == 0) {
        close(pfd[0]);
        close(pfd[1]);
        char *argv[] = {"exechelper", "10", "15", "hello-exec-pipe", NULL};
        char *envp[] = {"TEST_ENV=posix2024", NULL};
        execve("/bin/exechelper", argv, envp);
        _exit(99);
    }

    close(pfd[1]);
    close(10);
    close(15);
    close(nb[0]);
    close(nb[1]);

    struct pollfd pf = {.fd = pfd[0], .events = POLLIN, .revents = 0};
    int pr = poll(&pf, 1, 2000);
    if (pr != 1 || !(pf.revents & POLLIN)) {
        printf("[FAIL] poll on exechelper pipe -> %d revents=0x%x\n", pr,
               pf.revents);
        return 1;
    }

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(pfd[0], buf, sizeof(buf) - 1);
    if (n <= 0 || strcmp(buf, "hello-exec-pipe") != 0) {
        printf("[FAIL] pipe read mismatch: n=%ld buf='%s'\n", (long)n, buf);
        return 1;
    }

    /* Child exits and kernel PULSE_CODE_FD_CLOSE drops last writer -> EOF. */
    ssize_t eof = read(pfd[0], buf, sizeof(buf));
    if (eof != 0) {
        printf("[FAIL] expected EOF after exechelper exit, got %ld\n",
               (long)eof);
        return 1;
    }
    pf.events = POLLIN;
    pf.revents = 0;
    if (poll(&pf, 1, 0) != 1 || !(pf.revents & POLLHUP)) {
        printf("[FAIL] expected POLLHUP on closed-writer pipe\n");
        return 1;
    }
    close(pfd[0]);

    int st = 0;
    pid_t reaped = waitpid(pid, &st, 0);
    if (reaped != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 77) {
        printf("[FAIL] exechelper waitpid reaped=%d st=0x%x\n", (int)reaped,
               st);
        return 1;
    }
    printf("[OK] pipe2, dup3 O_CLOEXEC, and execve argv/envp/pipe verified\n");
    return 0;
}

static int test_ppoll_timeout_and_sigmask(void) {
    int pfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] pipe2 for ppoll failed\n");
        return 1;
    }

    /* 1. Finite ppoll timeout returning 0 on quiet pipe. */
    struct pollfd pf = {.fd = pfd[0], .events = POLLIN, .revents = 0};
    struct timespec short_ts = {.tv_sec = 0, .tv_nsec = 20000000L}; /* 20 ms */
    int pr = ppoll(&pf, 1, &short_ts, NULL);
    if (pr != 0) {
        printf("[FAIL] ppoll finite timeout expected 0, got %d (errno=%d)\n",
               pr, errno);
        return 1;
    }

    /* 2. Atomic sigmask unmasking + restoration across ppoll EINTR. */
    struct sigaction sa = {0};
    sa.sa_handler = ppoll_sig_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("[FAIL] sigaction(SIGUSR1) failed\n");
        return 1;
    }

    sigset_t blk = 0;
    sigemptyset(&blk);
    sigaddset(&blk, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blk, NULL) != 0) {
        printf("[FAIL] sigprocmask(SIG_BLOCK) failed\n");
        return 1;
    }

    pid_t parent_pid = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork for ppoll signal test failed\n");
        return 1;
    }
    if (pid == 0) {
        struct timespec d = {.tv_sec = 0, .tv_nsec = 25000000L}; /* 25 ms */
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
        kill(parent_pid, SIGUSR1);
        _exit(0);
    }

    sigset_t empty = 0;
    sigemptyset(&empty);
    struct timespec long_ts = {.tv_sec = 2, .tv_nsec = 0};
    errno = 0;
    pr = ppoll(&pf, 1, &long_ts, &empty);
    int saved_err = errno;

    sigset_t after = 0;
    sigprocmask(SIG_SETMASK, NULL, &after);
    int st = 0;
    waitpid(pid, &st, 0);
    close(pfd[0]);
    close(pfd[1]);

    if (pr != -1 || saved_err != EINTR || !g_ppoll_sig ||
        !sigismember(&after, SIGUSR1)) {
        printf("[FAIL] ppoll sigmask test: pr=%d err=%d sig=%d masked=%d\n", pr,
               saved_err, (int)g_ppoll_sig, sigismember(&after, SIGUSR1));
        return 1;
    }
    printf("[OK] ppoll finite timeout and atomic sigmask EINTR verified\n");
    return 0;
}

int main(void) {
    if (test_fork_cow() != 0) {
        return 1;
    }
    if (test_pipe2_dup3_execve() != 0) {
        return 1;
    }
    if (test_ppoll_timeout_and_sigmask() != 0) {
        return 1;
    }
    return 0;
}
