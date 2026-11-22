/**
 * @file     test_stress.c
 * @brief    QEMU test for <sys/select.h> and cross-subsystem zero-leak stress
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/sock.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/vfs.h>
#include <sys/vm.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* need:: VER_DOCS_0005 */

static volatile int g_sig1_hits;
static volatile int g_sig2_hits;

static void sig1_handler(int sig) {
    (void)sig;
    g_sig1_hits++;
}

static void sig2_handler(int sig) {
    (void)sig;
    g_sig2_hits++;
}

static void sleep_ms(long ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000L,
        .tv_nsec = (ms % 1000L) * 1000000L,
    };
    (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

static int connect_service(const char *path) {
    long ns = OpenConnection(1u, 1u);
    if (ns < 0) {
        return -1;
    }
    name_msg_t req = {0};
    req.type = NAME_MSG_QUERY;
    strlcpy(req.path, path, sizeof(req.path));
    name_reply_t rep = {0};
    long r = SendMsg((int)ns, &req, sizeof(req), &rep, sizeof(rep));
    CloseConnection((int)ns);
    if (r < 0 || rep.err != 0) {
        return -1;
    }
    return (int)OpenConnection(rep.pid, rep.chanid);
}

typedef struct {
    struct __k_sys_stats k;
    uint32_t pipes;
    uint32_t pipe_waiters;
    uint32_t vfs_handles;
    uint32_t socks;
    uint32_t sock_waiters;
} leak_snapshot_t;

static int take_snapshot(int pipe_conn, int vfs_conn, int sock_conn,
                         leak_snapshot_t *out) {
    memset(out, 0, sizeof(*out));
    if (SysStats(&out->k) < 0) {
        return -1;
    }

    pipe_req_t preq = {.op = PIPE_OP_STATS};
    pipe_rep_t prep = {0};
    if (SendMsg(pipe_conn, &preq, sizeof(preq), &prep, sizeof(prep)) < 0 ||
        prep.err != 0) {
        return -1;
    }
    out->pipes = prep.len;
    out->pipe_waiters = prep.handle_r;

    vfs_req_t vreq = {.op = VFS_OP_STATS};
    vfs_rep_t vrep = {0};
    if (SendMsg(vfs_conn, &vreq, sizeof(vreq), &vrep, sizeof(vrep)) < 0 ||
        vrep.err != 0) {
        return -1;
    }
    out->vfs_handles = vrep.len;

    sock_req_t sreq = {.op = SOCK_OP_STATS};
    sock_rep_t srep = {0};
    if (SendMsg(sock_conn, &sreq, sizeof(sreq), &srep, sizeof(srep)) < 0 ||
        srep.err != 0) {
        return -1;
    }
    out->socks = srep.len;
    out->sock_waiters = (uint32_t)srep.val;
    return 0;
}

static int test_select_and_pselect(void) {
    /* 1. FD_* macros and argument validation. */
    fd_set set;
    FD_ZERO(&set);
    if (FD_ISSET(0, &set) || FD_ISSET(5, &set)) {
        printf("[FAIL] FD_ZERO left bits set\n");
        return -1;
    }
    FD_SET(3, &set);
    FD_SET(17, &set);
    if (!FD_ISSET(3, &set) || !FD_ISSET(17, &set) || FD_ISSET(4, &set)) {
        printf("[FAIL] FD_SET/FD_ISSET mismatch\n");
        return -1;
    }
    FD_CLR(3, &set);
    if (FD_ISSET(3, &set) || !FD_ISSET(17, &set)) {
        printf("[FAIL] FD_CLR mismatch\n");
        return -1;
    }

    errno = 0;
    if (select(-1, NULL, NULL, NULL, NULL) != -1 || errno != EINVAL) {
        printf("[FAIL] select(-1) expected EINVAL got %d\n", errno);
        return -1;
    }
    struct timeval bad_tv = {.tv_sec = 0, .tv_usec = 1000000L};
    errno = 0;
    if (select(0, NULL, NULL, NULL, &bad_tv) != -1 || errno != EINVAL) {
        printf("[FAIL] select(bad tv_usec) expected EINVAL got %d\n", errno);
        return -1;
    }

    /* EBADF on closed descriptor. */
    FD_ZERO(&set);
    FD_SET(29, &set);
    struct timeval zero_tv = {.tv_sec = 0, .tv_usec = 0};
    errno = 0;
    if (select(30, &set, NULL, NULL, &zero_tv) != -1 || errno != EBADF) {
        printf("[FAIL] select(closed fd) expected EBADF got %d\n", errno);
        return -1;
    }

    /* 2. Multi-server select across pipe + AF_UNIX socketpair + VFS + TTY. */
    int pfd[2] = {-1, -1};
    int sfd[2] = {-1, -1};
    if (pipe2(pfd, O_CLOEXEC) != 0 ||
        socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sfd) != 0) {
        printf("[FAIL] select setup pipe/socketpair failed\n");
        return -1;
    }
    int vfd = open("/boot/sh", O_RDONLY | O_CLOEXEC);
    if (vfd < 0) {
        printf("[FAIL] select setup open(/boot/sh) failed\n");
        close(pfd[0]);
        close(pfd[1]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }

    /* Finite timeout with idle pipe + socket returns 0 and clears sets. */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(pfd[0], &rfds);
    FD_SET(sfd[0], &rfds);
    int maxfd = pfd[0] > sfd[0] ? pfd[0] : sfd[0];
    struct timeval short_tv = {.tv_sec = 0, .tv_usec = 20000L};
    int r = select(maxfd + 1, &rfds, NULL, NULL, &short_tv);
    if (r != 0 || FD_ISSET(pfd[0], &rfds) || FD_ISSET(sfd[0], &rfds)) {
        printf("[FAIL] select timeout expected 0 got %d\n", r);
        close(vfd);
        close(pfd[0]);
        close(pfd[1]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }

    /* Write to pipe and socket; select across pipe + socket + VFS + stdout. */
    const char msg[] = "sel";
    if (write(pfd[1], msg, 3) != 3 || send(sfd[1], msg, 3, 0) != 3) {
        printf("[FAIL] select write/send failed\n");
        close(vfd);
        close(pfd[0]);
        close(pfd[1]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(pfd[0], &rfds);
    FD_SET(sfd[0], &rfds);
    FD_SET(vfd, &rfds);
    FD_SET(STDOUT_FILENO, &wfds);
    maxfd = pfd[0];
    if (sfd[0] > maxfd) {
        maxfd = sfd[0];
    }
    if (vfd > maxfd) {
        maxfd = vfd;
    }
    if (STDOUT_FILENO > maxfd) {
        maxfd = STDOUT_FILENO;
    }
    struct timeval tv = {.tv_sec = 0, .tv_usec = 50000L};
    r = select(maxfd + 1, &rfds, &wfds, NULL, &tv);
    if (r != 4 || !FD_ISSET(pfd[0], &rfds) || !FD_ISSET(sfd[0], &rfds) ||
        !FD_ISSET(vfd, &rfds) || !FD_ISSET(STDOUT_FILENO, &wfds)) {
        printf("[FAIL] multi-server select expected 4 got %d\n", r);
        close(vfd);
        close(pfd[0]);
        close(pfd[1]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }

    char drain[8];
    (void)read(pfd[0], drain, sizeof(drain));
    (void)recv(sfd[0], drain, sizeof(drain), 0);

    /* Close pipe writer: EOF/POLLHUP must report pfd[0] readable in pselect. */
    close(pfd[1]);
    FD_ZERO(&rfds);
    FD_SET(pfd[0], &rfds);
    struct timespec zts = {.tv_sec = 0, .tv_nsec = 0};
    r = pselect(pfd[0] + 1, &rfds, NULL, NULL, &zts, NULL);
    if (r != 1 || !FD_ISSET(pfd[0], &rfds)) {
        printf("[FAIL] pselect EOF expected 1 got %d\n", r);
        close(vfd);
        close(pfd[0]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }

    /* 3. pselect atomic sigmask unblocking pending SIGUSR1 -> EINTR. */
    struct sigaction sa = {0};
    sa.sa_handler = sig1_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        printf("[FAIL] pselect sigaction failed\n");
        close(vfd);
        close(pfd[0]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }
    sigset_t blk, empty, check;
    sigemptyset(&blk);
    sigaddset(&blk, SIGUSR1);
    sigemptyset(&empty);
    g_sig1_hits = 0;
    if (sigprocmask(SIG_BLOCK, &blk, NULL) != 0 || raise(SIGUSR1) != 0) {
        printf("[FAIL] pselect block/raise failed\n");
        close(vfd);
        close(pfd[0]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }
    FD_ZERO(&rfds);
    FD_SET(sfd[0], &rfds);
    struct timespec pts = {.tv_sec = 0, .tv_nsec = 50000000L};
    errno = 0;
    r = pselect(sfd[0] + 1, &rfds, NULL, NULL, &pts, &empty);
    if (r != -1 || errno != EINTR || g_sig1_hits != 1) {
        printf("[FAIL] pselect sigmask expected EINTR hits=1 got r=%d err=%d "
               "hits=%d\n",
               r, errno, g_sig1_hits);
        sigprocmask(SIG_UNBLOCK, &blk, NULL);
        close(vfd);
        close(pfd[0]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }
    sigemptyset(&check);
    (void)sigprocmask(SIG_SETMASK, NULL, &check);
    if (!sigismember(&check, SIGUSR1)) {
        printf("[FAIL] pselect did not restore blocked sigmask\n");
        sigprocmask(SIG_UNBLOCK, &blk, NULL);
        close(vfd);
        close(pfd[0]);
        close(sfd[0]);
        close(sfd[1]);
        return -1;
    }
    (void)sigprocmask(SIG_UNBLOCK, &blk, NULL);

    close(vfd);
    close(pfd[0]);
    close(sfd[0]);
    close(sfd[1]);

    printf("[OK] <sys/select.h> select, pselect, multi-server FDs, and "
           "sigmask EINTR verified\n");
    return 0;
}

typedef struct {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    int done;
    int sum;
} worker_ctx_t;

static void *stress_worker(void *arg) {
    worker_ctx_t *ctx = (worker_ctx_t *)arg;
    uint8_t *buf = (uint8_t *)malloc(4096u);
    if (buf) {
        memset(buf, 0x5A, 4096u);
        int acc = buf[0] + buf[4095];
        free(buf);
        pthread_mutex_lock(&ctx->mu);
        ctx->sum += acc;
        ctx->done++;
        pthread_cond_signal(&ctx->cv);
        pthread_mutex_unlock(&ctx->mu);
    }
    return (void *)(uintptr_t)1u;
}

static int run_stress_round(int round) {
    /* A. Pipes + select (8 pipes per round => 48 pipes across 6 rounds). */
    for (int i = 0; i < 8; i++) {
        int p[2] = {-1, -1};
        if (pipe2(p, O_CLOEXEC) != 0) {
            printf("[FAIL] stress round %d pipe2(%d) errno=%d\n", round, i,
                   errno);
            return -1;
        }
        char c = (char)('A' + i);
        if (write(p[1], &c, 1) != 1) {
            close(p[0]);
            close(p[1]);
            return -1;
        }
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(p[0], &rf);
        struct timeval tv = {.tv_sec = 0, .tv_usec = 10000L};
        int sr = select(p[0] + 1, &rf, NULL, NULL, &tv);
        char out = 0;
        if (sr != 1 || read(p[0], &out, 1) != 1 || out != c) {
            close(p[0]);
            close(p[1]);
            return -1;
        }
        close(p[0]);
        close(p[1]);
    }

    /* B. AF_UNIX sockets (socketpair + listen/connect/accept4). */
    for (int i = 0; i < 4; i++) {
        int sv[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
            printf("[FAIL] stress round %d socketpair(%d) errno=%d\n", round, i,
                   errno);
            return -1;
        }
        char tx[4] = "ok!";
        char rx[4] = {0};
        if (send(sv[0], tx, 3, 0) != 3 || recv(sv[1], rx, 3, 0) != 3 ||
            memcmp(tx, rx, 3) != 0) {
            close(sv[0]);
            close(sv[1]);
            return -1;
        }
        close(sv[0]);
        close(sv[1]);
    }

    int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    int cli = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (srv < 0 || cli < 0) {
        if (srv >= 0) {
            close(srv);
        }
        if (cli >= 0) {
            close(cli);
        }
        return -1;
    }
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/stress_s_%d", round);
    if (bind(srv, (const struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(srv, 4) != 0 ||
        connect(cli, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(cli);
        close(srv);
        return -1;
    }
    int acc = accept4(srv, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (acc < 0) {
        close(cli);
        close(srv);
        return -1;
    }
    char ping = 'Z';
    char pong = 0;
    if (send(cli, &ping, 1, 0) != 1 || recv(acc, &pong, 1, 0) != 1 ||
        pong != 'Z') {
        close(acc);
        close(cli);
        close(srv);
        return -1;
    }
    close(acc);
    close(cli);
    close(srv);

    /* C. VFS (/tmp create/write/read/stat/unlink + /boot readdir + mkdir). */
    char dirp[48];
    char filep[64];
    snprintf(dirp, sizeof(dirp), "/tmp/stdir_%d", round);
    snprintf(filep, sizeof(filep), "/tmp/stdir_%d/f.txt", round);
    if (mkdir(dirp, 0755) != 0) {
        printf("[FAIL] stress round %d mkdir errno=%d\n", round, errno);
        return -1;
    }
    int fd = open(filep, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        (void)rmdir(dirp);
        return -1;
    }
    const char payload[] = "microkernel-stress";
    char rbuf[32] = {0};
    struct stat st = {0};
    if (write(fd, payload, sizeof(payload) - 1u) !=
            (ssize_t)(sizeof(payload) - 1u) ||
        lseek(fd, 0, SEEK_SET) != 0 ||
        read(fd, rbuf, sizeof(payload) - 1u) !=
            (ssize_t)(sizeof(payload) - 1u) ||
        fstat(fd, &st) != 0 || st.st_size != (off_t)(sizeof(payload) - 1u) ||
        ftruncate(fd, 4) != 0) {
        close(fd);
        (void)unlink(filep);
        (void)rmdir(dirp);
        return -1;
    }
    close(fd);
    DIR *dp = opendir("/boot");
    if (!dp) {
        (void)unlink(filep);
        (void)rmdir(dirp);
        return -1;
    }
    int seen = 0;
    while (readdir(dp) != NULL) {
        seen++;
    }
    closedir(dp);
    if (seen < 4 || unlink(filep) != 0 || rmdir(dirp) != 0) {
        return -1;
    }

    /* D. TTY termios query. */
    struct termios tio = {0};
    if (!isatty(STDOUT_FILENO) || tcgetattr(STDOUT_FILENO, &tio) != 0) {
        printf("[FAIL] stress round %d TTY check failed\n", round);
        return -1;
    }

    /* E. SHM shm_open + ftruncate + MAP_SHARED + munmap + shm_unlink. */
    char shname[32];
    snprintf(shname, sizeof(shname), "/st_shm_%d", round);
    int sfd = shm_open(shname, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (sfd < 0) {
        printf("[FAIL] stress round %d shm_open errno=%d\n", round, errno);
        return -1;
    }
    if (ftruncate(sfd, 8192) != 0) {
        close(sfd);
        (void)shm_unlink(shname);
        return -1;
    }
    void *mp = mmap(NULL, 8192u, PROT_READ | PROT_WRITE, MAP_SHARED, sfd, 0);
    if (mp == MAP_FAILED) {
        close(sfd);
        (void)shm_unlink(shname);
        return -1;
    }
    memset(mp, 0x33, 8192u);
    int ok_shm = (((uint8_t *)mp)[0] == 0x33 && ((uint8_t *)mp)[8191] == 0x33);
    int unmap_rc = munmap(mp, 8192u);
    close(sfd);
    int unl_rc = shm_unlink(shname);
    if (!ok_shm || unmap_rc != 0 || unl_rc != 0) {
        printf("[FAIL] stress round %d SHM verify/cleanup failed\n", round);
        return -1;
    }

    /* F. pthreads + condvar + heap churn + signal. */
    worker_ctx_t ctx = {
        .mu = PTHREAD_MUTEX_INITIALIZER,
        .cv = PTHREAD_COND_INITIALIZER,
        .done = 0,
        .sum = 0,
    };
    pthread_t t1 = 0;
    pthread_t t2 = 0;
    if (pthread_create(&t1, NULL, stress_worker, &ctx) != 0 ||
        pthread_create(&t2, NULL, stress_worker, &ctx) != 0) {
        printf("[FAIL] stress round %d pthread_create failed\n", round);
        return -1;
    }
    void *r1 = NULL;
    void *r2 = NULL;
    if (pthread_join(t1, &r1) != 0 || pthread_join(t2, &r2) != 0 ||
        ctx.done != 2 || ctx.sum != 2 * (0x5A + 0x5A)) {
        printf("[FAIL] stress round %d pthread_join mismatch\n", round);
        return -1;
    }
    int prev_hits = g_sig2_hits;
    if (raise(SIGUSR2) != 0 || g_sig2_hits != prev_hits + 1) {
        printf("[FAIL] stress round %d SIGUSR2 failed\n", round);
        return -1;
    }

    /* G. Command shell (/boot/sh) pipeline + redirection + waitpid. */
    char outpath[48];
    char cmd[128];
    snprintf(outpath, sizeof(outpath), "/tmp/st_sh_%d.out", round);
    snprintf(cmd, sizeof(cmd), "echo r%d | /boot/cat > %s", round, outpath);
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] stress round %d fork failed errno=%d\n", round, errno);
        return -1;
    }
    if (pid == 0) {
        char *argv[] = {"sh", "-c", cmd, NULL};
        execvp("sh", argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        printf("[FAIL] stress round %d sh exit status=%d\n", round, status);
        (void)unlink(outpath);
        return -1;
    }
    int ofd = open(outpath, O_RDONLY | O_CLOEXEC);
    char sbuf[16] = {0};
    ssize_t sn = (ofd >= 0) ? read(ofd, sbuf, sizeof(sbuf) - 1u) : -1;
    if (ofd >= 0) {
        close(ofd);
    }
    (void)unlink(outpath);
    char expect[16];
    snprintf(expect, sizeof(expect), "r%d\n", round);
    if (sn != (ssize_t)strlen(expect) || strcmp(sbuf, expect) != 0) {
        printf("[FAIL] stress round %d sh output mismatch\n", round);
        return -1;
    }

    return 0;
}

int main(void) {
    /* Wait until earlier concurrent boot tests have exited (7 servers + self).
     */
    struct __k_sys_stats ks = {0};
    for (int wait_it = 0; wait_it < 400; wait_it++) {
        if (SysStats(&ks) == 0 && ks.live_procs <= 8u) {
            break;
        }
        sleep_ms(20);
    }

    if (test_select_and_pselect() != 0) {
        return 1;
    }

    struct sigaction sa2 = {0};
    sa2.sa_handler = sig2_handler;
    sigemptyset(&sa2.sa_mask);
    if (sigaction(SIGUSR2, &sa2, NULL) != 0) {
        printf("[FAIL] stresstest sigaction(SIGUSR2) failed\n");
        return 1;
    }

    int pipe_conn = connect_service("/dev/pipe");
    int vfs_conn = connect_service("/dev/vfs");
    int sock_conn = connect_service("/dev/sock");
    if (pipe_conn < 0 || vfs_conn < 0 || sock_conn < 0) {
        printf("[FAIL] stresstest failed to connect to server stats\n");
        return 1;
    }

    /* Warm-up round so libc server connections and kheap reach steady state. */
    if (run_stress_round(0) != 0) {
        printf("[FAIL] stresstest warm-up round failed\n");
        return 1;
    }
    sleep_ms(30);

    leak_snapshot_t before = {0};
    if (take_snapshot(pipe_conn, vfs_conn, sock_conn, &before) != 0) {
        printf("[FAIL] stresstest before snapshot failed\n");
        return 1;
    }

    for (int r = 1; r <= 6; r++) {
        if (run_stress_round(r) != 0) {
            printf("[FAIL] stresstest round %d failed\n", r);
            return 1;
        }
    }
    sleep_ms(30);

    leak_snapshot_t after = {0};
    if (take_snapshot(pipe_conn, vfs_conn, sock_conn, &after) != 0) {
        printf("[FAIL] stresstest after snapshot failed\n");
        return 1;
    }

    CloseConnection(pipe_conn);
    CloseConnection(vfs_conn);
    CloseConnection(sock_conn);

    if (after.k.pmm_free_bytes != before.k.pmm_free_bytes ||
        after.k.open_fds != before.k.open_fds ||
        after.k.active_shm_objs != before.k.active_shm_objs ||
        after.pipes != before.pipes ||
        after.pipe_waiters != before.pipe_waiters ||
        after.vfs_handles != before.vfs_handles ||
        after.socks != before.socks ||
        after.sock_waiters != before.sock_waiters) {
        printf("[FAIL] resource leak detected: pmm_free %llu->%llu fds %u->%u "
               "shm %u->%u pipes %u/%u->%u/%u vfs %u->%u socks %u/%u->%u/%u\n",
               (unsigned long long)before.k.pmm_free_bytes,
               (unsigned long long)after.k.pmm_free_bytes, before.k.open_fds,
               after.k.open_fds, before.k.active_shm_objs,
               after.k.active_shm_objs, before.pipes, before.pipe_waiters,
               after.pipes, after.pipe_waiters, before.vfs_handles,
               after.vfs_handles, before.socks, before.sock_waiters,
               after.socks, after.sock_waiters);
        return 1;
    }

    printf("[OK] cross-subsystem stress (sh, VFS, pipes, TTY, sockets, SHM, "
           "pthreads) passed\n");
    printf("[OK] zero PMM frame, FD, SHM, pipe, VFS, and socket handle leaks "
           "verified (pmm_free=%llu)\n",
           (unsigned long long)after.k.pmm_free_bytes);
    return 0;
}
