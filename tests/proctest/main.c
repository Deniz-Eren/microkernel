/**
 * @file     main.c
 * @brief    Ring 3 Milestone 5 test for ProcCreate, VMMap, VMCloneCow, waitpid
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/vm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int register_service(int chanid, const char *path) {
    int nconn = (int)OpenConnection(1, 1);
    if (nconn < 0) {
        return -1;
    }
    name_msg_t nm = {0};
    nm.type = NAME_MSG_ATTACH;
    nm.chanid = (uint32_t)chanid;
    size_t n = strlen(path) + 1;
    if (n > sizeof(nm.path)) {
        n = sizeof(nm.path);
    }
    memcpy(nm.path, path, n);
    name_reply_t nr = {0};
    long s = SendMsg(nconn, &nm, sizeof(nm), &nr, sizeof(nr));
    CloseConnection(nconn);
    return (s < 0 || nr.err != 0) ? -1 : 0;
}

int main(void) {
    /* 1. Test anonymous mmap, read/write, mprotect, msync, W^X, and munmap. */
    void *scratch = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (scratch == MAP_FAILED) {
        printf("[FAIL] proctest mmap RW failed errno=%d\n", errno);
        return 1;
    }
    memcpy(scratch, "MMAP_OK", 8);
    if (strcmp((const char *)scratch, "MMAP_OK") != 0) {
        printf("[FAIL] proctest mmap content mismatch\n");
        return 1;
    }
    if (mprotect(scratch, 4096, PROT_READ) != 0 ||
        mprotect(scratch, 4096, PROT_READ | PROT_WRITE) != 0) {
        printf("[FAIL] proctest mprotect RO/RW failed errno=%d\n", errno);
        return 1;
    }
    if (msync(scratch, 4096, MS_SYNC) != 0) {
        printf("[FAIL] proctest msync failed errno=%d\n", errno);
        return 1;
    }

    errno = 0;
    void *wx = mmap(NULL, 4096, PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (wx != MAP_FAILED || errno != EACCES) {
        printf("[FAIL] proctest W^X mmap not rejected (errno=%d)\n", errno);
        return 1;
    }
    errno = 0;
    if (mprotect(scratch, 4096, PROT_WRITE | PROT_EXEC) == 0 ||
        errno != EACCES) {
        printf("[FAIL] proctest W^X mprotect not rejected (errno=%d)\n", errno);
        return 1;
    }
    if (munmap(scratch, 4096) != 0) {
        printf("[FAIL] proctest munmap failed errno=%d\n", errno);
        return 1;
    }
    printf("[OK] VMMap/mmap W^X, mprotect, msync, and munmap verified\n");

    /* 2. Prepare COW page and IPC channel before spawning procchild. */
    char *cow_page = (char *)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cow_page == MAP_FAILED) {
        printf("[FAIL] proctest COW mmap failed errno=%d\n", errno);
        return 1;
    }
    memcpy(cow_page, "PARENT_COW_DATA", 16);

    int chan = (int)CreateChannel();
    if (chan < 0 || register_service(chan, "/dev/proctest") != 0) {
        printf("[FAIL] proctest channel/register failed\n");
        return 1;
    }

    /* 3. Spawn child via ProcCreate and verify WNOHANG while alive. */
    long child_pid = ProcCreate("procchild", 0, 0);
    if (child_pid <= 0) {
        printf("[FAIL] proctest ProcCreate failed -> %ld\n", child_pid);
        return 1;
    }
    int st = 0;
    pid_t poll_r = waitpid((pid_t)child_pid, &st, WNOHANG);
    if (poll_r != 0) {
        printf("[FAIL] proctest waitpid(WNOHANG) expected 0 got %d\n", poll_r);
        return 1;
    }
    printf("[OK] ProcCreate spawned procchild pid=%ld\n", child_pid);

    /* 4. Receive handshake from child, clone COW page into child, reply VA. */
    uint32_t req = 0;
    ipc_recv_info_t info = {0};
    long msgid = ReceiveMsg(chan, &req, sizeof(req), &info);
    if (msgid <= 0 || info.pid != (uint32_t)child_pid) {
        printf("[FAIL] proctest handshake bad msgid=%ld pid=%u\n", msgid,
               info.pid);
        return 1;
    }
    long crc = VMCloneCow((uint32_t)child_pid, (uintptr_t)cow_page, 4096);
    if (crc != 0) {
        printf("[FAIL] proctest VMCloneCow failed -> %ld\n", crc);
        return 1;
    }
    uint64_t cow_va = (uint64_t)(uintptr_t)cow_page;
    if (ReplyMsg((int)msgid, &cow_va, sizeof(cow_va)) < 0) {
        printf("[FAIL] proctest ReplyMsg failed\n");
        return 1;
    }

    /* 5. Reap child and check exit code 42 via waitpid. */
    st = 0;
    pid_t reaped = waitpid((pid_t)child_pid, &st, 0);
    if (reaped != (pid_t)child_pid || !WIFEXITED(st) || WEXITSTATUS(st) != 42) {
        printf("[FAIL] proctest waitpid got pid=%d st=0x%x\n", reaped, st);
        return 1;
    }
    printf("[OK] waitpid reaped procchild pid=%d exit=%d\n", reaped,
           WEXITSTATUS(st));

    /* 6. Verify COW isolation in parent and single-ref in-place COW write. */
    if (strcmp(cow_page, "PARENT_COW_DATA") != 0) {
        printf("[FAIL] proctest parent COW page corrupted: '%s'\n", cow_page);
        return 1;
    }
    memcpy(cow_page, "PARENT_AFTER_COW", 17);
    if (strcmp(cow_page, "PARENT_AFTER_COW") != 0) {
        printf("[FAIL] proctest parent post-COW write failed\n");
        return 1;
    }
    if (munmap(cow_page, 4096) != 0) {
        printf("[FAIL] proctest COW munmap failed\n");
        return 1;
    }
    printf("[OK] VMCloneCow isolated parent and child writes cleanly\n");

    pid_t wchild = fork();
    if (wchild == 0) {
        _exit(7);
    }
    siginfo_t wi = {0};
    if (waitid(P_PID, wchild, &wi, WEXITED) != 0 || wi.si_pid != wchild ||
        wi.si_code != CLD_EXITED || wi.si_status != 7 ||
        wi.si_signo != SIGCHLD) {
        printf("[FAIL] waitid pid=%d code=%d status=%d signo=%d\n",
               (int)wi.si_pid, wi.si_code, wi.si_status, wi.si_signo);
        return 1;
    }
    printf("[OK] waitid(P_PID, WEXITED) reports exit status 7\n");

    pid_t nw = fork();
    if (nw == 0) {
        _exit(9);
    }
    if (waitid(P_PID, nw, &wi, WEXITED | WNOWAIT) != 0 || wi.si_status != 9) {
        printf("[FAIL] waitid WNOWAIT\n");
        return 1;
    }
    int nst = 0;
    if (waitpid(nw, &nst, 0) != nw || WEXITSTATUS(nst) != 9) {
        printf("[FAIL] waitpid after WNOWAIT\n");
        return 1;
    }
    printf("[OK] waitid WNOWAIT left the child waitable for waitpid\n");

    pid_t alive = fork();
    if (alive == 0) {
        struct timespec ts = {2, 0};
        (void)nanosleep(&ts, NULL);
        _exit(0);
    }
    memset(&wi, 0, sizeof(wi));
    if (waitid(P_PID, alive, &wi, WEXITED | WNOHANG) != 0 || wi.si_pid != 0) {
        printf("[FAIL] waitid WNOHANG pid=%d\n", (int)wi.si_pid);
        return 1;
    }
    (void)kill(alive, SIGKILL);
    while (waitpid(alive, &nst, 0) < 0 && errno == EINTR) {
    }
    printf("[OK] waitid WNOHANG reports no event with si_pid 0\n");

    pid_t ff = _Fork();
    if (ff == 0) {
        _exit(11);
    }
    if (ff < 0 || waitpid(ff, &nst, 0) != ff || WEXITSTATUS(nst) != 11) {
        printf("[FAIL] _Fork child status\n");
        return 1;
    }
    printf("[OK] _Fork spawned and reaped a child (exit 11)\n");

    pid_t ec = fork();
    if (ec == 0) {
        (void)execl("/bin/true", "true", (char *)0);
        _exit(126);
    }
    if (waitpid(ec, &nst, 0) != ec || WEXITSTATUS(nst) != 0) {
        printf("[FAIL] execl /bin/true st=0x%x\n", nst);
        return 1;
    }
    pid_t ee = fork();
    if (ee == 0) {
        char *envp[] = {"PROCTEST=1", NULL};
        (void)execle("/bin/true", "true", (char *)0, envp);
        _exit(126);
    }
    if (waitpid(ee, &nst, 0) != ee || WEXITSTATUS(nst) != 0) {
        printf("[FAIL] execle /bin/true st=0x%x\n", nst);
        return 1;
    }
    pid_t ep = fork();
    if (ep == 0) {
        (void)execlp("false", "false", (char *)0);
        _exit(126);
    }
    if (waitpid(ep, &nst, 0) != ep || WEXITSTATUS(nst) != 1) {
        printf("[FAIL] execlp false st=0x%x\n", nst);
        return 1;
    }
    printf("[OK] execl, execle, and execlp executed bootfs utilities\n");

    int efd = open("/bin/true", O_RDONLY | O_CLOEXEC);
    if (efd < 0) {
        printf("[FAIL] fexecve open /bin/true errno=%d\n", errno);
        return 1;
    }
    pid_t fe = fork();
    if (fe == 0) {
        char *fargv[] = {"true", NULL};
        char *fenvp[] = {NULL};
        (void)fexecve(efd, fargv, fenvp);
        _exit(126);
    }
    (void)close(efd);
    if (waitpid(fe, &nst, 0) != fe || WEXITSTATUS(nst) != 0) {
        printf("[FAIL] fexecve /bin/true st=0x%x\n", nst);
        return 1;
    }
    printf("[OK] fexecve executed an open bootfs descriptor\n");

    if (system(NULL) == 0) {
        printf("[FAIL] system(NULL) shell probe\n");
        return 1;
    }
    int sst = system("exit 3");
    if (!WIFEXITED(sst) || WEXITSTATUS(sst) != 3) {
        printf("[FAIL] system exit 3 st=0x%x\n", sst);
        return 1;
    }
    sst = system("true");
    if (!WIFEXITED(sst) || WEXITSTATUS(sst) != 0) {
        printf("[FAIL] system true st=0x%x\n", sst);
        return 1;
    }
    printf("[OK] system ran /bin/sh -c and reported the exit status\n");

    FILE *pr = popen("echo popen_ok", "r");
    if (!pr) {
        printf("[FAIL] popen reader errno=%d\n", errno);
        return 1;
    }
    char line[64] = {0};
    if (!fgets(line, sizeof(line), pr) || strcmp(line, "popen_ok\n") != 0) {
        printf("[FAIL] popen output '%s'\n", line);
        return 1;
    }
    int prc = pclose(pr);
    if (!WIFEXITED(prc) || WEXITSTATUS(prc) != 0) {
        printf("[FAIL] pclose reader st=0x%x\n", prc);
        return 1;
    }
    FILE *pw = popen("cat > /tmp/popenw.txt", "w");
    if (!pw) {
        printf("[FAIL] popen writer errno=%d\n", errno);
        return 1;
    }
    if (fputs("popen_write", pw) == EOF || fflush(pw) == EOF) {
        printf("[FAIL] popen writer fputs\n");
        return 1;
    }
    prc = pclose(pw);
    if (!WIFEXITED(prc) || WEXITSTATUS(prc) != 0) {
        printf("[FAIL] pclose writer st=0x%x\n", prc);
        return 1;
    }
    FILE *rf = fopen("/tmp/popenw.txt", "r");
    if (!rf) {
        printf("[FAIL] popen writer output open errno=%d\n", errno);
        return 1;
    }
    char buf[32] = {0};
    if (!fgets(buf, sizeof(buf), rf) || strcmp(buf, "popen_write") != 0) {
        printf("[FAIL] popen writer content '%s'\n", buf);
        return 1;
    }
    (void)fclose(rf);
    (void)remove("/tmp/popenw.txt");
    printf("[OK] popen/pclose reader and writer pipes verified\n");

    pid_t sj = fork();
    if (sj == 0) {
        struct timespec loop = {0, 200000000L};
        for (;;) {
            (void)nanosleep(&loop, NULL);
        }
    }
    if (kill(sj, SIGSTOP) != 0) {
        printf("[FAIL] kill SIGSTOP errno=%d\n", errno);
        return 1;
    }
    memset(&wi, 0, sizeof(wi));
    if (waitid(P_PID, sj, &wi, WSTOPPED) != 0 || wi.si_pid != sj ||
        wi.si_code != CLD_STOPPED || wi.si_status != SIGSTOP) {
        printf("[FAIL] waitid WSTOPPED pid=%d code=%d st=%d\n", (int)wi.si_pid,
               wi.si_code, wi.si_status);
        return 1;
    }
    printf("[OK] SIGSTOP parked the child and waitid(WSTOPPED) reported it\n");
    if (kill(sj, SIGCONT) != 0) {
        printf("[FAIL] kill SIGCONT errno=%d\n", errno);
        return 1;
    }
    memset(&wi, 0, sizeof(wi));
    if (waitid(P_PID, sj, &wi, WCONTINUED) != 0 || wi.si_pid != sj ||
        wi.si_code != CLD_CONTINUED || wi.si_status != SIGCONT) {
        printf("[FAIL] waitid WCONTINUED pid=%d code=%d st=%d\n",
               (int)wi.si_pid, wi.si_code, wi.si_status);
        return 1;
    }
    printf(
        "[OK] SIGCONT resumed the child and waitid(WCONTINUED) reported it\n");
    int jst = 0;
    (void)kill(sj, SIGKILL);
    while (waitpid(sj, &jst, 0) < 0 && errno == EINTR) {
    }
    if (!WIFSIGNALED(jst) || WTERMSIG(jst) != SIGKILL) {
        printf("[FAIL] SIGKILL reap st=0x%x\n", jst);
        return 1;
    }
    printf("[OK] SIGKILL terminated the resumed child\n");
    return 0;
}
