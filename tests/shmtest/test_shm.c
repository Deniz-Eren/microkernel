/**
 * @file     test_shm.c
 * @brief    QEMU test for POSIX.1-2024 SHM (shm_open/MAP_SHARED), ML, and MLR
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: VER_MEM_0004 */

static int test_shm_shared_and_fork(void) {
    /* Invalid names must be rejected with EINVAL. */
    errno = 0;
    if (shm_open("noslash", O_CREAT | O_RDWR, 0600u) != -1 || errno != EINVAL) {
        printf("[FAIL] shm_open(noslash) expected EINVAL, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (shm_open("/a/b", O_CREAT | O_RDWR, 0600u) != -1 || errno != EINVAL) {
        printf("[FAIL] shm_open(/a/b) expected EINVAL, got %d\n", errno);
        return 1;
    }

    int fd = shm_open("/mk_shm_test", O_CREAT | O_EXCL | O_RDWR, 0600u);
    if (fd < 0) {
        printf("[FAIL] shm_open create failed errno=%d\n", errno);
        return 1;
    }
    /* POSIX mandates FD_CLOEXEC on shm_open descriptors. */
    if ((fcntl(fd, F_GETFD) & FD_CLOEXEC) == 0) {
        printf("[FAIL] shm_open FD missing FD_CLOEXEC\n");
        close(fd);
        return 1;
    }
    errno = 0;
    if (shm_open("/mk_shm_test", O_CREAT | O_EXCL | O_RDWR, 0600u) != -1 ||
        errno != EEXIST) {
        printf("[FAIL] shm_open duplicate expected EEXIST, got %d\n", errno);
        close(fd);
        return 1;
    }

    if (ftruncate(fd, 8192) != 0) {
        printf("[FAIL] ftruncate(shm, 8192) errno=%d\n", errno);
        close(fd);
        return 1;
    }
    struct stat st = {0};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 8192) {
        printf("[FAIL] fstat(shm) size=%ld\n", (long)st.st_size);
        close(fd);
        return 1;
    }

    /* W^X must reject PROT_WRITE | PROT_EXEC on MAP_SHARED. */
    errno = 0;
    if (mmap(NULL, 4096u, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd,
             0) != MAP_FAILED ||
        errno != EACCES) {
        printf("[FAIL] SHM W^X expected EACCES, got %d\n", errno);
        close(fd);
        return 1;
    }

    /* O_RDONLY SHM descriptor must reject writable MAP_SHARED. */
    int ro_fd = shm_open("/mk_shm_test", O_RDONLY, 0);
    if (ro_fd < 0) {
        printf("[FAIL] shm_open(O_RDONLY) failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    errno = 0;
    if (mmap(NULL, 4096u, PROT_READ | PROT_WRITE, MAP_SHARED, ro_fd, 0) !=
            MAP_FAILED ||
        errno != EACCES) {
        printf("[FAIL] RO SHM write MAP_SHARED expected EACCES, got %d\n",
               errno);
        close(ro_fd);
        close(fd);
        return 1;
    }
    close(ro_fd);

    uint32_t *shared = (uint32_t *)mmap(NULL, 8192u, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, 0);
    if (shared == MAP_FAILED) {
        printf("[FAIL] mmap(MAP_SHARED, shm) errno=%d\n", errno);
        close(fd);
        return 1;
    }
    shared[0] = 0x11223344u;
    shared[1024] = 0x55667788u;

    /* Anonymous MAP_SHARED must also stay shared across fork(). */
    uint32_t *anon_sh = (uint32_t *)mmap(NULL, 4096u, PROT_READ | PROT_WRITE,
                                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (anon_sh == MAP_FAILED) {
        printf("[FAIL] mmap(MAP_SHARED|MAP_ANONYMOUS) errno=%d\n", errno);
        munmap(shared, 8192u);
        close(fd);
        return 1;
    }
    anon_sh[0] = 0x10101010u;

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork failed\n");
        return 1;
    }
    if (pid == 0) {
        /* Re-open independently by name and verify shared frames. */
        int cfd = shm_open("/mk_shm_test", O_RDWR, 0);
        if (cfd < 0) {
            _exit(1);
        }
        uint32_t *cview = (uint32_t *)mmap(NULL, 4096u, PROT_READ | PROT_WRITE,
                                           MAP_SHARED, cfd, 0);
        if (cview == MAP_FAILED || cview[0] != 0x11223344u) {
            _exit(2);
        }
        cview[0] = 0xCAFEBABEu;
        munmap(cview, 4096u);

        /* MAP_PRIVATE on SHM must NOT overwrite the shared object. */
        uint32_t *pview = (uint32_t *)mmap(NULL, 4096u, PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE, cfd, 0);
        if (pview == MAP_FAILED || pview[0] != 0xCAFEBABEu) {
            _exit(3);
        }
        pview[0] = 0xBAD0BAD0u;
        munmap(pview, 4096u);
        close(cfd);

        /* Inherited MAP_SHARED mappings must stay shared (not COW). */
        shared[1024] = 0xDEADBEEFu;
        anon_sh[0] = 0x20202020u;
        _exit(0);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        printf("[FAIL] shm child failed status=%d\n", status);
        return 1;
    }
    if (shared[0] != 0xCAFEBABEu || shared[1024] != 0xDEADBEEFu ||
        anon_sh[0] != 0x20202020u) {
        printf("[FAIL] SHM/MAP_SHARED visibility mismatch (%x %x %x)\n",
               shared[0], shared[1024], anon_sh[0]);
        return 1;
    }

    if (shm_unlink("/mk_shm_test") != 0) {
        printf("[FAIL] shm_unlink failed errno=%d\n", errno);
        return 1;
    }
    errno = 0;
    if (shm_open("/mk_shm_test", O_RDONLY, 0) != -1 || errno != ENOENT) {
        printf("[FAIL] shm_open after unlink expected ENOENT, got %d\n", errno);
        return 1;
    }
    /* Existing mapping remains valid until unmapped. */
    if (shared[0] != 0xCAFEBABEu) {
        printf("[FAIL] unlinked SHM mapping corrupted\n");
        return 1;
    }
    munmap(anon_sh, 4096u);
    munmap(shared, 8192u);
    close(fd);

    printf("[OK] SHM shm_open/shm_unlink, ftruncate, and cross-process "
           "MAP_SHARED verified\n");
    return 0;
}

static int test_mlock_and_mlockall(void) {
    void *pg = mmap(NULL, 8192u, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pg == MAP_FAILED) {
        printf("[FAIL] mlock mmap failed\n");
        return 1;
    }
    /* Unaligned interior range must succeed on mapped memory. */
    if (mlock((char *)pg + 13, 4096u) != 0 ||
        munlock((char *)pg + 13, 4096u) != 0) {
        printf("[FAIL] mlock/munlock mapped range failed errno=%d\n", errno);
        munmap(pg, 8192u);
        return 1;
    }
    /* Unmapped VA must fail with ENOMEM. */
    errno = 0;
    if (mlock((void *)0x48000000ULL, 4096u) != -1 || errno != ENOMEM) {
        printf("[FAIL] mlock unmapped expected ENOMEM, got %d\n", errno);
        munmap(pg, 8192u);
        return 1;
    }
    /* Invalid mlockall flags must fail with EINVAL. */
    errno = 0;
    if (mlockall(0) != -1 || errno != EINVAL) {
        printf("[FAIL] mlockall(0) expected EINVAL, got %d\n", errno);
        munmap(pg, 8192u);
        return 1;
    }
    errno = 0;
    if (mlockall(0x80) != -1 || errno != EINVAL) {
        printf("[FAIL] mlockall(0x80) expected EINVAL, got %d\n", errno);
        munmap(pg, 8192u);
        return 1;
    }
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        printf("[FAIL] mlockall(MCL_CURRENT|MCL_FUTURE) errno=%d\n", errno);
        munmap(pg, 8192u);
        return 1;
    }
    void *fut = mmap(NULL, 4096u, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (fut == MAP_FAILED) {
        printf("[FAIL] mmap under MCL_FUTURE failed\n");
        munlockall();
        munmap(pg, 8192u);
        return 1;
    }
    if (munlockall() != 0) {
        printf("[FAIL] munlockall failed errno=%d\n", errno);
        munmap(fut, 4096u);
        munmap(pg, 8192u);
        return 1;
    }
    munmap(fut, 4096u);
    munmap(pg, 8192u);

    printf("[OK] ML/MLR mlock, munlock, mlockall, and munlockall verified\n");
    return 0;
}

int main(void) {
    if (test_shm_shared_and_fork() != 0) {
        return 1;
    }
    if (test_mlock_and_mlockall() != 0) {
        return 1;
    }
    return 0;
}
