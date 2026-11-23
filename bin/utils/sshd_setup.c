/**
 * @file     sshd_setup.c
 * @brief    Initialize /mnt/etc/ssh, Ed25519 host key, and launch /boot/sshd
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* need:: COMP_IPC_0021 */

static int ensure_dir(const char *path, mode_t mode) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            return -1;
        }
        (void)chmod(path, mode);
        return 0;
    }
    if (mkdir(path, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    (void)chmod(path, mode);
    return 0;
}

static int write_file_mode(const char *path, const char *data, mode_t mode) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        return -1;
    }
    size_t len = strlen(data);
    size_t off = 0u;
    while (off < len) {
        ssize_t n = write(fd, data + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(fd);
            return -1;
        }
        off += (size_t)n;
    }
    (void)chmod(path, mode);
    (void)close(fd);
    return 0;
}

static int run_cmd(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        (void)execv(path, argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return -1;
    }
    return 0;
}

static int sshd_pid_alive(void) {
    int fd = open("/tmp/sshd.pid", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1u);
    (void)close(fd);
    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';
    long pid = strtol(buf, NULL, 10);
    if (pid <= 0) {
        return 0;
    }
    return kill((pid_t)pid, 0) == 0 ? 1 : 0;
}

int main(void) {
    if (ensure_dir("/mnt", 0755u) != 0 || ensure_dir("/mnt/etc", 0755u) != 0 ||
        ensure_dir("/mnt/etc/ssh", 0755u) != 0 ||
        ensure_dir("/mnt/etc/ssh/empty", 0755u) != 0 ||
        ensure_dir("/mnt/root", 0755u) != 0 ||
        ensure_dir("/mnt/root/.ssh", 0700u) != 0) {
        fprintf(stderr, "sshd-setup: failed to prepare /mnt directories\n");
        return 1;
    }

    if (access("/mnt/etc/passwd", F_OK) != 0) {
        const char *hash = crypt("microkernel", "$5$mkdev$");
        if (!hash) {
            hash = "*";
        }
        char pw[512];
        int n = snprintf(
            pw, sizeof(pw),
            "root:%s:0:0:root:/mnt/root:/boot/sh\n"
            "sshd:*:22:22:sshd privilege separation:/mnt/etc/ssh/empty:"
            "/boot/false\n"
            "nobody:*:65534:65534:nobody:/tmp:/boot/sh\n",
            hash);
        if (n <= 0 || (size_t)n >= sizeof(pw) ||
            write_file_mode("/mnt/etc/passwd", pw, 0644u) != 0) {
            fprintf(stderr, "sshd-setup: failed to write /mnt/etc/passwd\n");
            return 1;
        }
    }

    if (access("/mnt/etc/ssh/ssh_host_ed25519_key", F_OK) != 0) {
        char *const keygen_argv[] = {
            (char *)"/boot/ssh-keygen",
            (char *)"-t",
            (char *)"ed25519",
            (char *)"-N",
            (char *)"",
            (char *)"-f",
            (char *)"/mnt/etc/ssh/ssh_host_ed25519_key",
            (char *)"-q",
            NULL,
        };
        if (run_cmd("/boot/ssh-keygen", keygen_argv) != 0) {
            fprintf(stderr, "sshd-setup: ssh-keygen failed\n");
            return 1;
        }
    }

    if (access("/mnt/etc/ssh/sshd_config", F_OK) != 0) {
        static const char k_cfg[] =
            "Port 22\n"
            "ListenAddress 0.0.0.0\n"
            "HostKey /mnt/etc/ssh/ssh_host_ed25519_key\n"
            "AuthorizedKeysFile .ssh/authorized_keys "
            "/mnt/etc/ssh/authorized_keys\n"
            "PermitRootLogin yes\n"
            "PasswordAuthentication yes\n"
            "PubkeyAuthentication yes\n"
            "UseDNS no\n"
            "PidFile /tmp/sshd.pid\n";
        if (write_file_mode("/mnt/etc/ssh/sshd_config", k_cfg, 0644u) != 0) {
            fprintf(stderr, "sshd-setup: failed to write sshd_config\n");
            return 1;
        }
    }

    if (sshd_pid_alive()) {
        printf("[OK] sshd already listening on port 22\n");
        return 0;
    }

    char *const sshd_argv[] = {
        (char *)"/boot/sshd",
        (char *)"-f",
        (char *)"/mnt/etc/ssh/sshd_config",
        NULL,
    };
    if (run_cmd("/boot/sshd", sshd_argv) != 0) {
        fprintf(stderr, "sshd-setup: /boot/sshd exited with error\n");
        return 1;
    }

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};
    for (int i = 0; i < 40; i++) {
        if (sshd_pid_alive()) {
            break;
        }
        (void)nanosleep(&ts, NULL);
    }

    printf("[OK] sshd listening on port 22 "
           "(user=root, dev password=microkernel)\n");
    return 0;
}
