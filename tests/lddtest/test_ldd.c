/**
 * @file     test_ldd.c
 * @brief    QEMU test for /boot/ld.so ldd trace mode and /usr/sbin/ldconfig
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

/* need:: VER_LIBC_0036 VER_LIBC_0037 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int run_capture_pipe(const char *path, char *const args[],
                            char *const envp[], char *out, size_t out_sz) {
    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) != 0) {
        printf("[FAIL] lddtest pipe2 failed errno=%d\n", errno);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] lddtest fork failed errno=%d\n", errno);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        (void)dup2(pfd[1], STDOUT_FILENO);
        (void)close(pfd[0]);
        (void)close(pfd[1]);
        (void)execve(path, args, envp);
        _exit(200);
    }
    (void)close(pfd[1]);
    size_t got = 0u;
    while (got + 1u < out_sz) {
        ssize_t n = read(pfd[0], out + got, out_sz - 1u - got);
        if (n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    out[got] = '\0';
    (void)close(pfd[0]);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        printf("[FAIL] lddtest child st=0x%x\n", st);
        return -1;
    }
    return WEXITSTATUS(st);
}

static int run_capture_file(const char *path, char *const args[],
                            char *const envp[], const char *tmp_path, char *out,
                            size_t out_sz) {
    int fd = open(tmp_path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        printf("[FAIL] lddtest open %s errno=%d\n", tmp_path, errno);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        (void)close(fd);
        (void)unlink(tmp_path);
        return -1;
    }
    if (pid == 0) {
        (void)dup2(fd, STDOUT_FILENO);
        (void)close(fd);
        (void)execve(path, args, envp);
        _exit(200);
    }
    (void)close(fd);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        (void)unlink(tmp_path);
        return -1;
    }
    int rfd = open(tmp_path, O_RDONLY | O_CLOEXEC);
    size_t got = 0u;
    if (rfd >= 0) {
        while (got + 1u < out_sz) {
            ssize_t n = read(rfd, out + got, out_sz - 1u - got);
            if (n <= 0) {
                break;
            }
            got += (size_t)n;
        }
        (void)close(rfd);
    }
    out[got] = '\0';
    (void)unlink(tmp_path);
    return WEXITSTATUS(st);
}

static int run_capture_pty(const char *path, char *const args[],
                           char *const envp[], char *out, size_t out_sz) {
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        printf("[FAIL] lddtest PTY open/unlock failed errno=%d\n", errno);
        if (master >= 0) {
            (void)close(master);
        }
        return -1;
    }
    char pts_path[32] = {0};
    if (ptsname_r(master, pts_path, sizeof(pts_path)) != 0) {
        (void)close(master);
        return -1;
    }
    int slave = open(pts_path, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slave < 0) {
        (void)close(master);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        (void)close(slave);
        (void)close(master);
        return -1;
    }
    if (pid == 0) {
        (void)dup2(slave, STDOUT_FILENO);
        (void)close(slave);
        (void)close(master);
        (void)execve(path, args, envp);
        _exit(200);
    }
    (void)close(slave);
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st)) {
        (void)close(master);
        return -1;
    }
    size_t got = 0u;
    while (got + 1u < out_sz) {
        struct pollfd pfd = {.fd = master, .events = POLLIN, .revents = 0};
        if (poll(&pfd, 1u, 100) <= 0 || !(pfd.revents & POLLIN)) {
            break;
        }
        ssize_t n = read(master, out + got, out_sz - 1u - got);
        if (n <= 0) {
            break;
        }
        got += (size_t)n;
    }
    out[got] = '\0';
    (void)close(master);
    return WEXITSTATUS(st);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        return -1;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0u) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int check_link(const char *path, const char *expected) {
    char target[128];
    ssize_t n = readlink(path, target, sizeof(target) - 1u);
    if (n < 0) {
        printf("[FAIL] lddtest readlink %s errno=%d\n", path, errno);
        return -1;
    }
    target[n] = '\0';
    if (strcmp(target, expected) != 0) {
        printf("[FAIL] lddtest %s -> '%s' (expected '%s')\n", path, target,
               expected);
        return -1;
    }
    return 0;
}

static int expect_substr(const char *label, const char *buf,
                         const char *needle) {
    if (!strstr(buf, needle)) {
        printf("[FAIL] lddtest %s missing '%s' (got '%s')\n", label, needle,
               buf);
        return -1;
    }
    return 0;
}

int main(void) {
    char *const no_env[] = {0};
    char *const zlib_args[] = {"ldd", "/boot/zlibtest", 0};
    char *const openssl_args[] = {"ldd", "/boot/openssltest", 0};
    char *const sshd_args[] = {"ldd", "/boot/sshd", 0};
    char *const trace_args[] = {"zlibtest", 0};
    char *const trace_env[] = {"LD_TRACE_LOADED_OBJECTS=1", 0};
    char *const ldconfig_args[] = {"ldconfig", 0};
    char *const ldconfig_tmp_args[] = {"ldconfig", "-n", "/tmp/ldcfgtest", 0};
    char out[1024];

    if (check_link("/usr/bin/ldd", "/boot/ld.so") != 0 ||
        check_link("/usr/sbin/ldconfig", "/boot/ldconfig") != 0) {
        return 1;
    }

    /* Verify /etc/ld.so.conf reads its actual text (not NULs). */
    int cfd = open("/etc/ld.so.conf", O_RDONLY);
    if (cfd < 0) {
        printf("[FAIL] lddtest open /etc/ld.so.conf failed\n");
        return 1;
    }
    ssize_t cn = read(cfd, out, sizeof(out) - 1u);
    (void)close(cfd);
    if (cn <= 0) {
        printf("[FAIL] lddtest read /etc/ld.so.conf failed\n");
        return 1;
    }
    out[(size_t)cn] = '\0';
    if (expect_substr("ld.so.conf /boot", out, "/boot\n") != 0 ||
        expect_substr("ld.so.conf /lib", out, "/lib\n") != 0 ||
        expect_substr("ld.so.conf /usr/lib", out, "/usr/lib\n") != 0) {
        return 1;
    }

    /* Pipe capture proves ldd writes to fd 1 (not hardcoded serial). */
    if (run_capture_pipe("/boot/ldd", zlib_args, no_env, out, sizeof(out)) !=
            0 ||
        expect_substr("pipe zlib libz", out,
                      "libz.so.1 => /boot/libz.so.1 (0x") != 0 ||
        expect_substr("pipe zlib libc", out, "libc.so => /boot/libc.so (0x") !=
            0) {
        return 1;
    }
    if (run_capture_pipe("/boot/ldd", openssl_args, no_env, out, sizeof(out)) !=
            0 ||
        expect_substr("pipe openssl libssl", out,
                      "libssl.so.3 => /boot/libssl.so.3 (0x") != 0 ||
        expect_substr("pipe openssl libcrypto", out,
                      "libcrypto.so.3 => /boot/libcrypto.so.3 (0x") != 0 ||
        expect_substr("pipe openssl libc", out,
                      "libc.so => /boot/libc.so (0x") != 0) {
        return 1;
    }
    if (run_capture_pipe("/boot/ldd", sshd_args, no_env, out, sizeof(out)) !=
            0 ||
        expect_substr("pipe sshd libcrypto", out,
                      "libcrypto.so.3 => /boot/libcrypto.so.3 (0x") != 0 ||
        expect_substr("pipe sshd libz", out,
                      "libz.so.1 => /boot/libz.so.1 (0x") != 0 ||
        expect_substr("pipe sshd libc", out, "libc.so => /boot/libc.so (0x") !=
            0) {
        return 1;
    }
    if (run_capture_pipe("/boot/zlibtest", trace_args, trace_env, out,
                         sizeof(out)) != 0 ||
        expect_substr("LD_TRACE libz", out,
                      "libz.so.1 => /boot/libz.so.1 (0x") != 0 ||
        strstr(out, "[OK] zlib") != NULL) {
        printf("[FAIL] lddtest LD_TRACE_LOADED_OBJECTS=1 failed\n");
        return 1;
    }

    /* VFS file redirection proves fd 1 -> FD_KIND_VFS works in ld.so. */
    if (run_capture_file("/boot/ldd", zlib_args, no_env, "/tmp/ldd_out.txt",
                         out, sizeof(out)) != 0 ||
        expect_substr("file zlib libz", out,
                      "libz.so.1 => /boot/libz.so.1 (0x") != 0) {
        return 1;
    }

    /* PTY capture proves fd 1 -> /dev/pts/N (interactive SSH) works. */
    if (run_capture_pty("/boot/ldd", sshd_args, no_env, out, sizeof(out)) !=
            0 ||
        expect_substr("pty sshd libcrypto", out,
                      "libcrypto.so.3 => /boot/libcrypto.so.3 (0x") != 0 ||
        expect_substr("pty sshd libc", out, "libc.so => /boot/libc.so (0x") !=
            0) {
        return 1;
    }

    /* Verify ldconfig default scan and DT_SONAME symlink creation. */
    if (run_capture_pipe("/boot/ldconfig", ldconfig_args, no_env, out,
                         sizeof(out)) != 0 ||
        expect_substr("ldconfig default", out,
                      "ldconfig: scanned 3 directories, created 0 symlinks") !=
            0) {
        return 1;
    }

    if (mkdir("/tmp/ldcfgtest", 0755) != 0 && errno != EEXIST) {
        printf("[FAIL] lddtest mkdir /tmp/ldcfgtest errno=%d\n", errno);
        return 1;
    }
    if (copy_file("/boot/libz.so.1", "/tmp/ldcfgtest/libz.so.1.3.1") != 0) {
        printf("[FAIL] lddtest copy libz.so.1 failed\n");
        return 1;
    }
    if (run_capture_pipe("/boot/ldconfig", ldconfig_tmp_args, no_env, out,
                         sizeof(out)) != 0 ||
        expect_substr("ldconfig tmp", out, "created 1 symlinks") != 0) {
        (void)unlink("/tmp/ldcfgtest/libz.so.1");
        (void)unlink("/tmp/ldcfgtest/libz.so.1.3.1");
        (void)rmdir("/tmp/ldcfgtest");
        return 1;
    }
    int link_ok = check_link("/tmp/ldcfgtest/libz.so.1", "libz.so.1.3.1");
    (void)unlink("/tmp/ldcfgtest/libz.so.1");
    (void)unlink("/tmp/ldcfgtest/libz.so.1.3.1");
    (void)rmdir("/tmp/ldcfgtest");
    if (link_ok != 0) {
        return 1;
    }

    printf("[OK] lddtest ldd over /etc/ld.so.conf, LD_TRACE_LOADED_OBJECTS, "
           "pipe/file/PTY stdout, and ldconfig symlinks verified\n");
    return 0;
}
