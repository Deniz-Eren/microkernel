/**
 * @file     child.c
 * @brief    Child process spawned by proctest to exercise COW and exit status
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <unistd.h>

int main(void) {
    int nconn = (int)OpenConnection(1, 1);
    if (nconn < 0) {
        printf("[FAIL] procchild OpenConnection(sysproc) -> %d\n", nconn);
        return 10;
    }

    name_msg_t nm = {0};
    nm.type = NAME_MSG_QUERY;
    memcpy(nm.path, "/dev/proctest", sizeof("/dev/proctest"));
    name_reply_t nr = {0};
    long s = SendMsg(nconn, &nm, sizeof(nm), &nr, sizeof(nr));
    CloseConnection(nconn);
    if (s < 0 || nr.err != 0 || nr.pid <= 0) {
        printf("[FAIL] procchild resolve /dev/proctest failed\n");
        return 11;
    }

    int pconn = (int)OpenConnection((uint32_t)nr.pid, (uint32_t)nr.chanid);
    if (pconn < 0) {
        printf("[FAIL] procchild OpenConnection(proctest) -> %d\n", pconn);
        return 12;
    }

    uint32_t req = 1;
    uint64_t cow_va = 0;
    long r = SendMsg(pconn, &req, sizeof(req), &cow_va, sizeof(cow_va));
    CloseConnection(pconn);
    if (r != (long)sizeof(cow_va) || cow_va == 0) {
        printf("[FAIL] procchild handshake failed r=%ld va=%llu\n", r,
               (unsigned long long)cow_va);
        return 13;
    }

    char *page = (char *)(uintptr_t)cow_va;
    if (strcmp(page, "PARENT_COW_DATA") != 0) {
        printf("[FAIL] procchild COW initial data mismatch\n");
        return 14;
    }
    /* Trigger hardware #PF COW copy in Ring 3. */
    memcpy(page, "CHILD_COW_WRITE", 16);
    if (strcmp(page, "CHILD_COW_WRITE") != 0) {
        printf("[FAIL] procchild COW write mismatch\n");
        return 15;
    }
    printf("[OK] procchild COW read and private write succeeded\n");
    _exit(42);
}
