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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/vm.h>
#include <sys/wait.h>
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
    return 0;
}
