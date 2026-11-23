/**
 * @file     test_vfs.c
 * @brief    QEMU test for Ring 3 VFS bootfs, tmpfs, stat, dirent, and cwd
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* need:: VER_IPC_0005 VER_LIBC_0012 */

static int test_bootfs_readonly(void) {
    DIR *d = opendir("/boot");
    if (!d) {
        printf("[FAIL] opendir(/boot) errno=%d\n", errno);
        return 1;
    }
    int saw_sysproc = 0;
    int saw_vfsserv = 0;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        count++;
        if (strcmp(de->d_name, "sysproc") == 0) {
            saw_sysproc = 1;
        } else if (strcmp(de->d_name, "vfsserv") == 0) {
            saw_vfsserv = 1;
        }
    }
    rewinddir(d);
    if (readdir(d) == NULL || closedir(d) != 0) {
        printf("[FAIL] rewinddir/closedir(/boot) failed\n");
        return 1;
    }
    if (!saw_sysproc || !saw_vfsserv || count < 4) {
        printf("[FAIL] /boot listing incomplete (count=%d)\n", count);
        return 1;
    }

    int fd = open("/boot/sysproc", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        printf("[FAIL] open(/boot/sysproc) errno=%d\n", errno);
        return 1;
    }
    struct stat st = {0};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 64) {
        printf("[FAIL] fstat(/boot/sysproc) invalid\n");
        close(fd);
        return 1;
    }
    uint8_t magic[4] = {0};
    if (read(fd, magic, sizeof(magic)) != 4 || magic[0] != 0x7Fu ||
        magic[1] != 'E' || magic[2] != 'L' || magic[3] != 'F') {
        printf("[FAIL] /boot/sysproc ELF magic mismatch\n");
        close(fd);
        return 1;
    }
    close(fd);

    errno = 0;
    if (open("/boot/sysproc", O_WRONLY) != -1 || errno != EROFS) {
        printf("[FAIL] write open on /boot expected EROFS, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (unlink("/boot/sysproc") != -1 || errno != EROFS) {
        printf("[FAIL] unlink on /boot expected EROFS, got %d\n", errno);
        return 1;
    }
    printf("[OK] bootfs /boot readdir, ELF read, fstat, and EROFS verified\n");
    return 0;
}

static int test_tmpfs_io_stat_and_openat(void) {
    mode_t prev_mask = umask(0022u);
    (void)prev_mask;
    if (mkdir("/tmp/work", 0755u) != 0) {
        printf("[FAIL] mkdir(/tmp/work) errno=%d\n", errno);
        return 1;
    }
    if (chdir("/tmp/work") != 0) {
        printf("[FAIL] chdir(/tmp/work) errno=%d\n", errno);
        return 1;
    }
    char cwd[128] = {0};
    if (!getcwd(cwd, sizeof(cwd)) || strcmp(cwd, "/tmp/work") != 0) {
        printf("[FAIL] getcwd='%s' expected '/tmp/work'\n", cwd);
        return 1;
    }
    errno = 0;
    if (getcwd(cwd, 4u) != NULL || errno != ERANGE) {
        printf("[FAIL] getcwd small buffer expected ERANGE, got %d\n", errno);
        return 1;
    }

    int fd = open("hello.txt", O_CREAT | O_RDWR | O_EXCL, 0644u);
    if (fd < 0) {
        printf("[FAIL] open(hello.txt) errno=%d\n", errno);
        return 1;
    }
    errno = 0;
    if (open("hello.txt", O_CREAT | O_RDWR | O_EXCL, 0644u) != -1 ||
        errno != EEXIST) {
        printf("[FAIL] O_EXCL duplicate expected EEXIST, got %d\n", errno);
        close(fd);
        return 1;
    }

    /* Multi-chunk write (> 384 bytes) + lseek + read + ftruncate. */
    char wbuf[600];
    for (size_t i = 0; i < sizeof(wbuf); i++) {
        wbuf[i] = (char)('A' + (i % 26u));
    }
    if (write(fd, wbuf, sizeof(wbuf)) != (ssize_t)sizeof(wbuf)) {
        printf("[FAIL] write(600) failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    if (lseek(fd, 0, SEEK_END) != (off_t)sizeof(wbuf) ||
        lseek(fd, 0, SEEK_SET) != 0) {
        printf("[FAIL] lseek failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    char rbuf[600] = {0};
    if (read(fd, rbuf, sizeof(rbuf)) != (ssize_t)sizeof(rbuf) ||
        memcmp(rbuf, wbuf, sizeof(wbuf)) != 0) {
        printf("[FAIL] multi-chunk readback mismatch\n");
        close(fd);
        return 1;
    }

    if (ftruncate(fd, 10) != 0 || ftruncate(fd, 24) != 0) {
        printf("[FAIL] ftruncate shrink/grow failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    struct stat st = {0};
    if (stat("/tmp/work/hello.txt", &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size != 24) {
        printf("[FAIL] stat after ftruncate size=%ld\n", (long)st.st_size);
        close(fd);
        return 1;
    }
    char zbuf[24] = {0};
    if (lseek(fd, 0, SEEK_SET) != 0 || read(fd, zbuf, 24u) != 24 ||
        memcmp(zbuf, wbuf, 10u) != 0 || zbuf[10] != 0 || zbuf[23] != 0) {
        printf("[FAIL] ftruncate zero-extension mismatch\n");
        close(fd);
        return 1;
    }
    close(fd);

    /* openat + fstatat + unlinkat on directory FD. */
    int dfd = open("/tmp/work", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        printf("[FAIL] open(/tmp/work, O_DIRECTORY) errno=%d\n", errno);
        return 1;
    }
    int sfd = openat(dfd, "sub.txt", O_CREAT | O_RDWR, 0600u);
    if (sfd < 0 || write(sfd, "ok", 2u) != 2) {
        printf("[FAIL] openat(sub.txt) failed errno=%d\n", errno);
        close(dfd);
        return 1;
    }
    close(sfd);
    if (fstatat(dfd, "sub.txt", &st, 0) != 0 || st.st_size != 2 ||
        access("/tmp/work/sub.txt", R_OK | W_OK) != 0 ||
        unlinkat(dfd, "sub.txt", 0) != 0) {
        printf("[FAIL] fstatat/access/unlinkat failed errno=%d\n", errno);
        close(dfd);
        return 1;
    }
    close(dfd);

    printf("[OK] tmpfs mkdir, chdir, getcwd, openat, lseek, ftruncate, and "
           "stat verified\n");
    return 0;
}

static int test_dup_fork_unlink_and_rmdir(void) {
    /* Pipe lseek must fail with ESPIPE. */
    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] pipe2 failed\n");
        return 1;
    }
    errno = 0;
    if (lseek(pfd[0], 0, SEEK_CUR) != (off_t)-1 || errno != ESPIPE) {
        printf("[FAIL] pipe lseek expected ESPIPE, got %d\n", errno);
        close(pfd[0]);
        close(pfd[1]);
        return 1;
    }
    close(pfd[0]);
    close(pfd[1]);

    int fd = open("/tmp/work/shared.txt", O_CREAT | O_RDWR | O_TRUNC, 0644u);
    if (fd < 0) {
        printf("[FAIL] open(shared.txt) errno=%d\n", errno);
        return 1;
    }
    int fd2 = dup(fd);
    if (fd2 < 0 || write(fd, "ab", 2u) != 2 || write(fd2, "cd", 2u) != 2) {
        printf("[FAIL] dup shared-offset write failed\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork failed\n");
        return 1;
    }
    if (pid == 0) {
        ssize_t w = write(fd2, "ef", 2u);
        _exit(w == 2 ? 0 : 1);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        printf("[FAIL] fork child VFS write failed\n");
        return 1;
    }
    close(fd2);

    /* Unlink while still open: stat returns ENOENT, read still succeeds. */
    if (unlink("/tmp/work/shared.txt") != 0) {
        printf("[FAIL] unlink(shared.txt) errno=%d\n", errno);
        close(fd);
        return 1;
    }
    struct stat st = {0};
    errno = 0;
    if (stat("/tmp/work/shared.txt", &st) != -1 || errno != ENOENT) {
        printf("[FAIL] stat after unlink expected ENOENT, got %d\n", errno);
        close(fd);
        return 1;
    }
    char buf[8] = {0};
    if (lseek(fd, 0, SEEK_SET) != 0 || read(fd, buf, 6u) != 6 ||
        memcmp(buf, "abcdef", 6u) != 0) {
        printf("[FAIL] unlinked open file readback='%s' expected 'abcdef'\n",
               buf);
        close(fd);
        return 1;
    }
    close(fd);

    /* rmdir non-empty must fail with ENOTEMPTY; after unlink it succeeds. */
    if (chdir("/") != 0) {
        printf("[FAIL] chdir(/) failed\n");
        return 1;
    }
    errno = 0;
    if (rmdir("/tmp/work") != -1 || errno != ENOTEMPTY) {
        printf("[FAIL] rmdir non-empty expected ENOTEMPTY, got %d\n", errno);
        return 1;
    }
    if (unlink("/tmp/work/hello.txt") != 0 || rmdir("/tmp/work") != 0) {
        printf("[FAIL] cleanup unlink/rmdir failed errno=%d\n", errno);
        return 1;
    }
    errno = 0;
    if (stat("/tmp/work", &st) != -1 || errno != ENOENT) {
        printf("[FAIL] stat after rmdir expected ENOENT, got %d\n", errno);
        return 1;
    }
    printf("[OK] VFS shared dup/fork offsets, unlinked open persistence, and "
           "rmdir verified\n");
    return 0;
}

static int test_futimens_and_utimensat(void) {
    int fd = open("/tmp/utime_file.txt", O_CREAT | O_RDWR | O_TRUNC, 0644u);
    if (fd < 0) {
        printf("[FAIL] utimens open errno=%d\n", errno);
        return 1;
    }
    struct timespec ts[2] = {
        {.tv_sec = 1700000100L, .tv_nsec = 123456789L},
        {.tv_sec = 1700000200L, .tv_nsec = 987654321L},
    };
    if (futimens(fd, ts) != 0) {
        printf("[FAIL] futimens explicit failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    struct stat st = {0};
    if (fstat(fd, &st) != 0 || st.st_atim.tv_sec != 1700000100L ||
        st.st_atim.tv_nsec != 123456789L || st.st_mtim.tv_sec != 1700000200L ||
        st.st_mtim.tv_nsec != 987654321L) {
        printf("[FAIL] futimens stat mismatch atim=%ld.%ld mtim=%ld.%ld\n",
               (long)st.st_atim.tv_sec, st.st_atim.tv_nsec,
               (long)st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
        close(fd);
        return 1;
    }

    /* UTIME_OMIT preserves atime while updating mtime; NULL sets both to NOW.
     */
    struct timespec partial[2] = {
        {.tv_sec = 0, .tv_nsec = UTIME_OMIT},
        {.tv_sec = 1700000300L, .tv_nsec = 500L},
    };
    if (utimensat(AT_FDCWD, "/tmp/utime_file.txt", partial, 0) != 0 ||
        fstat(fd, &st) != 0 || st.st_atim.tv_sec != 1700000100L ||
        st.st_atim.tv_nsec != 123456789L || st.st_mtim.tv_sec != 1700000300L ||
        st.st_mtim.tv_nsec != 500L) {
        printf("[FAIL] utimensat UTIME_OMIT failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    if (utimensat(AT_FDCWD, "/tmp/utime_file.txt", NULL, 0) != 0 ||
        fstat(fd, &st) != 0 || st.st_mtim.tv_sec == 1700000300L) {
        printf("[FAIL] utimensat NULL UTIME_NOW failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    close(fd);

    /* Symlink AT_SYMLINK_NOFOLLOW updates the link without changing target. */
    if (symlink("/tmp/utime_file.txt", "/tmp/utime_link") != 0) {
        printf("[FAIL] symlink for utimensat failed errno=%d\n", errno);
        return 1;
    }
    struct timespec link_ts[2] = {
        {.tv_sec = 1600000001L, .tv_nsec = 111L},
        {.tv_sec = 1600000002L, .tv_nsec = 222L},
    };
    if (utimensat(AT_FDCWD, "/tmp/utime_link", link_ts, AT_SYMLINK_NOFOLLOW) !=
            0 ||
        lstat("/tmp/utime_link", &st) != 0 ||
        st.st_atim.tv_sec != 1600000001L || st.st_mtim.tv_sec != 1600000002L ||
        stat("/tmp/utime_file.txt", &st) != 0 ||
        st.st_mtim.tv_sec == 1600000002L) {
        printf("[FAIL] utimensat AT_SYMLINK_NOFOLLOW failed\n");
        return 1;
    }

    /* Error paths: invalid tv_nsec -> EINVAL, bad flag -> EINVAL, EROFS, EBADF.
     */
    struct timespec bad_ts[2] = {
        {.tv_sec = 0, .tv_nsec = 1000000000L},
        {.tv_sec = 0, .tv_nsec = 0},
    };
    errno = 0;
    if (utimensat(AT_FDCWD, "/tmp/utime_file.txt", bad_ts, 0) != -1 ||
        errno != EINVAL) {
        printf("[FAIL] utimensat bad tv_nsec expected EINVAL, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (utimensat(AT_FDCWD, "/tmp/utime_file.txt", NULL, 0x1234) != -1 ||
        errno != EINVAL) {
        printf("[FAIL] utimensat bad flag expected EINVAL, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (utimensat(AT_FDCWD, "/boot/sysproc", NULL, 0) != -1 || errno != EROFS) {
        printf("[FAIL] utimensat /boot expected EROFS, got %d\n", errno);
        return 1;
    }
    errno = 0;
    if (futimens(999, NULL) != -1 || errno != EBADF) {
        printf("[FAIL] futimens bad fd expected EBADF, got %d\n", errno);
        return 1;
    }

    (void)unlink("/tmp/utime_link");
    (void)unlink("/tmp/utime_file.txt");
    printf("[OK] futimens and utimensat (UTIME_NOW, UTIME_OMIT, NOFOLLOW, "
           "EROFS) verified\n");
    return 0;
}

int main(void) {
    if (test_bootfs_readonly() != 0) {
        return 1;
    }
    if (test_tmpfs_io_stat_and_openat() != 0) {
        return 1;
    }
    if (test_dup_fork_unlink_and_rmdir() != 0) {
        return 1;
    }
    if (test_futimens_and_utimensat() != 0) {
        return 1;
    }
    return 0;
}
