/**
 * @file     vfs.c
 * @brief    POSIX.1-2024 VFS, stat, dirent, lseek, ftruncate, and cwd wrappers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/vm.h>
#include <time.h>
#include <unistd.h>
#include "fd_internal.h"
#include "tls_internal.h"

/* need:: COMP_IPC_0009 COMP_IPC_0017 COMP_LIBC_0006 */

struct __dirstream {
    int fd;
    uint32_t index;
    struct dirent ent;
};

static int g_vfs_conn = -1;
static long g_vfs_conn_pid = -1;
static uint32_t g_vfs_srv_pid;
static uint32_t g_vfs_srv_chan;

_Static_assert(PROC_CWD_MAX == VFS_PATH_MAX,
               "PROC_CWD_MAX must match VFS_PATH_MAX");

static mode_t current_umask(void) {
    long r = __sys5(SYS_FD_OP, FD_OP_UMASK, 0, 1, 0, 0);
    if (r >= 0) {
        return (mode_t)(r & 0777L);
    }
    return 0022u;
}

static int ensure_vfsserv(void) {
    long cur_pid = __sys0(SYS_PROC_GETPID);
    if (cur_pid <= 0) {
        return -EPERM;
    }
    if (g_vfs_conn >= 0 && g_vfs_conn_pid == cur_pid) {
        return g_vfs_conn;
    }
    if (g_vfs_srv_pid == 0u || g_vfs_srv_chan == 0u) {
        for (int tries = 0; tries < 200; tries++) {
            long ns = OpenConnection(1, 1);
            if (ns >= 0) {
                name_msg_t q = {0};
                q.type = NAME_MSG_QUERY;
                (void)strlcpy(q.path, "/dev/vfs", sizeof(q.path));
                name_reply_t rep = {0};
                long r = SendMsg((int)ns, &q, sizeof(q), &rep, sizeof(rep));
                CloseConnection((int)ns);
                if (r >= 0 && rep.err == 0 && rep.pid > 0 && rep.chanid > 0) {
                    g_vfs_srv_pid = (uint32_t)rep.pid;
                    g_vfs_srv_chan = (uint32_t)rep.chanid;
                    break;
                }
            }
            struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
            __sys4(SYS_CLOCK_NANOSLEEP, CLOCK_MONOTONIC, 0, (long)(uintptr_t)&d,
                   0);
        }
    }
    if (g_vfs_srv_pid == 0u || g_vfs_srv_chan == 0u) {
        return -ENXIO;
    }
    long conn = OpenConnection(g_vfs_srv_pid, g_vfs_srv_chan);
    if (conn < 0) {
        return (int)conn;
    }
    g_vfs_conn = (int)conn;
    g_vfs_conn_pid = cur_pid;
    return g_vfs_conn;
}

static long vfs_rpc(const vfs_req_t *req, vfs_rep_t *rep) {
    int conn = ensure_vfsserv();
    if (conn < 0) {
        return conn;
    }
    long r = SendMsg(conn, req, sizeof(*req), rep, sizeof(*rep));
    if (r < 0) {
        return r;
    }
    return rep->err;
}

static int build_req_path(int dirfd, const char *path, uint32_t *out_handle,
                          char out_path[VFS_PATH_MAX]) {
    if (!path) {
        return -EFAULT;
    }
    if (path[0] == '\0') {
        return -ENOENT;
    }
    if (path[0] == '/') {
        if (strlcpy(out_path, path, VFS_PATH_MAX) >= VFS_PATH_MAX) {
            return -ENAMETOOLONG;
        }
        *out_handle = 0u;
        return 0;
    }
    if (dirfd != AT_FDCWD) {
        if (dirfd < 0) {
            return -EBADF;
        }
        struct __k_fd_entry e = {0};
        long gr =
            __sys5(SYS_FD_OP, FD_OP_GET, dirfd, (long)(uintptr_t)&e, 0, 0);
        if (gr < 0) {
            return (int)gr;
        }
        if (e.kind != FD_KIND_VFS) {
            return -ENOTDIR;
        }
        if (strlcpy(out_path, path, VFS_PATH_MAX) >= VFS_PATH_MAX) {
            return -ENAMETOOLONG;
        }
        *out_handle = e.srv_handle;
        return 0;
    }
    /* Relative to process current working directory. */
    char cwd[VFS_PATH_MAX];
    long cr = __sys5(SYS_FD_OP, FD_OP_GETCWD, (long)(uintptr_t)cwd,
                     (long)sizeof(cwd), 0, 0);
    if (cr < 0) {
        return (int)cr;
    }
    size_t clen = (size_t)cr;
    size_t plen = strlen(path);
    int need_slash = (clen > 0u && cwd[clen - 1u] != '/');
    if (clen + (size_t)need_slash + plen + 1u > VFS_PATH_MAX) {
        return -ENAMETOOLONG;
    }
    memcpy(out_path, cwd, clen);
    size_t pos = clen;
    if (need_slash) {
        out_path[pos++] = '/';
    }
    memcpy(out_path + pos, path, plen + 1u);
    *out_handle = 0u;
    return 0;
}

static void convert_stat(const vfs_stat_t *src, struct stat *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->st_dev = (dev_t)src->st_dev;
    dst->st_ino = (ino_t)src->st_ino;
    dst->st_mode = (mode_t)src->st_mode;
    dst->st_nlink = (nlink_t)src->st_nlink;
    dst->st_uid = (uid_t)src->st_uid;
    dst->st_gid = (gid_t)src->st_gid;
    dst->st_size = (off_t)src->st_size;
    dst->st_atim.tv_sec = (time_t)src->st_mtime_sec;
    dst->st_atim.tv_nsec = (long)src->st_mtime_nsec;
    dst->st_mtim.tv_sec = (time_t)src->st_mtime_sec;
    dst->st_mtim.tv_nsec = (long)src->st_mtime_nsec;
    dst->st_ctim.tv_sec = (time_t)src->st_mtime_sec;
    dst->st_ctim.tv_nsec = (long)src->st_mtime_nsec;
    dst->st_blksize = 4096;
    dst->st_blocks = (blkcnt_t)((src->st_size + 511ULL) / 512ULL);
}

void __libc_vfs_close_handle(uint32_t handle) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CLOSE;
    req.handle = handle;
    (void)vfs_rpc(&req, &rep);
}

long __libc_vfs_dup_handle(uint32_t handle) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_DUP;
    req.handle = handle;
    return vfs_rpc(&req, &rep);
}

ssize_t __libc_vfs_read(uint32_t handle, void *buf, size_t n) {
    if (n == 0u) {
        return 0;
    }
    uint8_t *dst = (uint8_t *)buf;
    size_t done = 0u;
    while (done < n) {
        size_t chunk = n - done;
        if (chunk > VFS_DATA_MAX) {
            chunk = VFS_DATA_MAX;
        }
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_READ;
        req.handle = handle;
        req.len = (uint32_t)chunk;
        long r = vfs_rpc(&req, &rep);
        if (r < 0) {
            if (r == -EPIPE) {
                (void)raise(SIGPIPE);
            }
            if (done > 0u) {
                return (ssize_t)done;
            }
            errno = (int)(-r);
            return -1;
        }
        if (rep.len > 0u) {
            memcpy(dst + done, rep.u.data, rep.len);
            done += rep.len;
        }
        if (rep.len < chunk) {
            break;
        }
    }
    return (ssize_t)done;
}

ssize_t __libc_vfs_write(uint32_t handle, const void *buf, size_t n,
                         uint32_t status_flags) {
    if (n == 0u) {
        return 0;
    }
    const uint8_t *src = (const uint8_t *)buf;
    size_t done = 0u;
    while (done < n) {
        size_t chunk = n - done;
        if (chunk > VFS_DATA_MAX) {
            chunk = VFS_DATA_MAX;
        }
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_WRITE;
        req.handle = handle;
        req.flags = status_flags;
        req.len = (uint32_t)chunk;
        memcpy(req.u.data, src + done, chunk);
        long r = vfs_rpc(&req, &rep);
        if (r < 0) {
            if (r == -EPIPE) {
                (void)raise(SIGPIPE);
            }
            if (done > 0u) {
                return (ssize_t)done;
            }
            errno = (int)(-r);
            return -1;
        }
        done += rep.len;
        if (rep.len < chunk) {
            break;
        }
    }
    return (ssize_t)done;
}

int openat(int fd, const char *path, int oflag, ...) {
    __libc_cancel_point();
    mode_t mode = 0u;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = (mode_t)va_arg(ap, unsigned int);
        va_end(ap);
    }

    if (path && strcmp(path, "/dev/tty") == 0) {
        struct __k_fd_entry te = {
            .kind = FD_KIND_TTY,
            .fd_flags = (oflag & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
            .status_flags = (uint16_t)(oflag & (O_ACCMODE | O_NONBLOCK)),
            .srv_pid = 0u,
            .srv_chanid = 0u,
            .srv_handle = 0u,
        };
        long tfd =
            __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&te, 0, 0);
        if (tfd < 0) {
            errno = (int)(-tfd);
            return -1;
        }
        return (int)tfd;
    }

    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_OPEN;
    req.flags = (uint32_t)oflag;
    mode_t cur_umask = current_umask();
    req.mode = (uint32_t)(mode & ~cur_umask & 0777u);
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }

    struct __k_fd_entry e = {
        .kind = FD_KIND_VFS,
        .fd_flags = (oflag & O_CLOEXEC) ? FD_FLAG_CLOEXEC : 0u,
        .status_flags =
            (uint16_t)(oflag & (O_ACCMODE | O_NONBLOCK | O_APPEND | O_DSYNC)),
        .srv_pid = g_vfs_srv_pid,
        .srv_chanid = g_vfs_srv_chan,
        .srv_handle = rep.handle,
    };
    long nfd = __sys5(SYS_FD_OP, FD_OP_ALLOC, 0, (long)(uintptr_t)&e, 0, 0);
    if (nfd < 0) {
        __libc_vfs_close_handle(rep.handle);
        errno = (int)(-nfd);
        return -1;
    }
    return (int)nfd;
}

int open(const char *path, int oflag, ...) {
    mode_t mode = 0u;
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = (mode_t)va_arg(ap, unsigned int);
        va_end(ap);
    }
    return openat(AT_FDCWD, path, oflag, mode);
}

int creat(const char *path, mode_t mode) {
    return openat(AT_FDCWD, path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

off_t lseek(int fildes, off_t offset, int whence) {
    if (fildes < 0) {
        errno = EBADF;
        return (off_t)-1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return (off_t)-1;
    }
    if (e.kind == FD_KIND_SERVER || e.kind == FD_KIND_CONSOLE ||
        e.kind == FD_KIND_TTY) {
        errno = ESPIPE;
        return (off_t)-1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EBADF;
        return (off_t)-1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_LSEEK;
    req.handle = e.srv_handle;
    req.offset = (int64_t)offset;
    req.whence = (uint32_t)whence;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return (off_t)-1;
    }
    return (off_t)rep.offset;
}

int ftruncate(int fildes, off_t length) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_SHM) {
        long r =
            __sys4(SYS_SHM_OP, SHM_OP_TRUNCATE, (long)fildes, (long)length, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        return 0;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_FTRUNCATE;
    req.handle = e.srv_handle;
    req.offset = (int64_t)length;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int fstatat(int fd, const char *restrict path, struct stat *restrict buf,
            int flag) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    if (flag & ~AT_SYMLINK_NOFOLLOW) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_STAT;
    req.flags = (uint32_t)flag;
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    convert_stat(&rep.u.st, buf);
    return 0;
}

int stat(const char *restrict path, struct stat *restrict buf) {
    return fstatat(AT_FDCWD, path, buf, 0);
}

int lstat(const char *restrict path, struct stat *restrict buf) {
    return fstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

int fstat(int fildes, struct stat *buf) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_CONSOLE || e.kind == FD_KIND_TTY) {
        memset(buf, 0, sizeof(*buf));
        buf->st_mode = S_IFCHR | 0620u;
        buf->st_nlink = 1u;
        return 0;
    }
    if (e.kind == FD_KIND_SERVER) {
        memset(buf, 0, sizeof(*buf));
        buf->st_mode = S_IFIFO | 0600u;
        buf->st_nlink = 1u;
        return 0;
    }
    if (e.kind == FD_KIND_SHM) {
        struct __k_shm_stat ks = {0};
        long r = __sys4(SYS_SHM_OP, SHM_OP_STAT, (long)fildes,
                        (long)(uintptr_t)&ks, 0);
        if (r < 0) {
            errno = (int)(-r);
            return -1;
        }
        memset(buf, 0, sizeof(*buf));
        buf->st_ino = (ino_t)ks.id;
        buf->st_mode = (mode_t)(S_IFREG | (ks.mode & 0777u));
        buf->st_nlink = 1u;
        buf->st_size = (off_t)ks.size;
        buf->st_blksize = 4096;
        return 0;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EBADF;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_FSTAT;
    req.handle = e.srv_handle;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    convert_stat(&rep.u.st, buf);
    return 0;
}

int mkdirat(int fd, const char *path, mode_t mode) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_MKDIR;
    mode_t cur_umask = current_umask();
    req.mode = (uint32_t)(mode & ~cur_umask & 0777u);
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    return mkdirat(AT_FDCWD, path, mode);
}

int unlinkat(int fd, const char *path, int flag) {
    if (flag & ~AT_REMOVEDIR) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_UNLINK;
    req.flags = (uint32_t)flag;
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int unlink(const char *path) {
    return unlinkat(AT_FDCWD, path, 0);
}

int rmdir(const char *path) {
    return unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
}

int chdir(const char *path) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_RESOLVE;
    int pr = build_req_path(AT_FDCWD, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (!S_ISDIR(rep.flags)) {
        errno = ENOTDIR;
        return -1;
    }
    long sr =
        __sys5(SYS_FD_OP, FD_OP_SETCWD, (long)(uintptr_t)rep.u.path, 0, 0, 0);
    if (sr < 0) {
        errno = (int)(-sr);
        return -1;
    }
    return 0;
}

int fchdir(int fildes) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = ENOTDIR;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_RESOLVE;
    req.handle = e.srv_handle;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    if (!S_ISDIR(rep.flags)) {
        errno = ENOTDIR;
        return -1;
    }
    long sr =
        __sys5(SYS_FD_OP, FD_OP_SETCWD, (long)(uintptr_t)rep.u.path, 0, 0, 0);
    if (sr < 0) {
        errno = (int)(-sr);
        return -1;
    }
    return 0;
}

char *getcwd(char *buf, size_t size) {
    if (!buf || size == 0u) {
        errno = EINVAL;
        return NULL;
    }
    long r =
        __sys5(SYS_FD_OP, FD_OP_GETCWD, (long)(uintptr_t)buf, (long)size, 0, 0);
    if (r < 0) {
        errno = (int)(-r);
        return NULL;
    }
    return buf;
}

mode_t umask(mode_t cmask) {
    long r = __sys5(SYS_FD_OP, FD_OP_UMASK, (long)(cmask & 0777u), 0, 0, 0);
    if (r >= 0) {
        return (mode_t)(r & 0777L);
    }
    return 0022u;
}

int access(const char *path, int amode) {
    if (amode & ~(F_OK | R_OK | W_OK | X_OK)) {
        errno = EINVAL;
        return -1;
    }
    struct stat st = {0};
    if (stat(path, &st) != 0) {
        return -1;
    }
    if ((amode & R_OK) && !(st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH))) {
        errno = EACCES;
        return -1;
    }
    if ((amode & W_OK) && !(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
        errno = EACCES;
        return -1;
    }
    if ((amode & X_OK) && !(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

DIR *fdopendir(int fd) {
    struct stat st = {0};
    if (fstat(fd, &st) != 0) {
        return NULL;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    DIR *d = (DIR *)calloc(1u, sizeof(*d));
    if (!d) {
        return NULL;
    }
    d->fd = fd;
    d->index = 0u;
    return d;
}

DIR *opendir(const char *dirname) {
    int fd = open(dirname, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }
    DIR *d = fdopendir(fd);
    if (!d) {
        int saved = errno;
        (void)close(fd);
        errno = saved;
    }
    return d;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return NULL;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, dirp->fd, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0 || e.kind != FD_KIND_VFS) {
        errno = EBADF;
        return NULL;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_READDIR;
    req.handle = e.srv_handle;
    req.len = dirp->index;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return NULL;
    }
    if (rep.flags != 0u) {
        return NULL; /* EOF: do not modify errno */
    }
    dirp->index++;
    dirp->ent.d_ino = (ino_t)rep.u.dent.d_ino;
    (void)strlcpy(dirp->ent.d_name, rep.u.dent.d_name,
                  sizeof(dirp->ent.d_name));
    return &dirp->ent;
}

void rewinddir(DIR *dirp) {
    if (dirp) {
        dirp->index = 0u;
    }
}

int dirfd(DIR *dirp) {
    if (!dirp) {
        errno = EINVAL;
        return -1;
    }
    return dirp->fd;
}

int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }
    int fd = dirp->fd;
    free(dirp);
    return close(fd);
}

int renameat(int oldfd, const char *oldpath, int newfd, const char *newpath) {
    char old_buf[VFS_PATH_MAX];
    char new_buf[VFS_PATH_MAX];
    uint32_t old_h = 0u;
    uint32_t new_h = 0u;

    int r1 = build_req_path(oldfd, oldpath, &old_h, old_buf);
    if (r1 < 0) {
        errno = -r1;
        return -1;
    }
    int r2 = build_req_path(newfd, newpath, &new_h, new_buf);
    if (r2 < 0) {
        errno = -r2;
        return -1;
    }
    if (strlen(old_buf) >= VFS_RENAME_PATH_MAX ||
        strlen(new_buf) >= VFS_RENAME_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_RENAME;
    req.handle = old_h;
    req.u.ren.new_handle = new_h;
    (void)strlcpy(req.u.ren.old_path, old_buf, sizeof(req.u.ren.old_path));
    (void)strlcpy(req.u.ren.new_path, new_buf, sizeof(req.u.ren.new_path));
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int rename(const char *oldpath, const char *newpath) {
    return renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

char *realpath(const char *restrict file_name, char *restrict resolved_name) {
    if (!file_name) {
        errno = EINVAL;
        return NULL;
    }
    if (file_name[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_RESOLVE;
    int pr = build_req_path(AT_FDCWD, file_name, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return NULL;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return NULL;
    }
    size_t len = strlen(rep.u.path);
    if (len >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    char *dst = resolved_name;
    if (!dst) {
        dst = (char *)malloc(len + 1u);
        if (!dst) {
            errno = ENOMEM;
            return NULL;
        }
    }
    memcpy(dst, rep.u.path, len + 1u);
    return dst;
}

int fchmodat(int fd, const char *path, mode_t mode, int flag) {
    if (flag & ~AT_SYMLINK_NOFOLLOW) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CHMOD;
    req.mode = (uint32_t)(mode & 07777u);
    req.flags = (uint32_t)flag;
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int chmod(const char *path, mode_t mode) {
    return fchmodat(AT_FDCWD, path, mode, 0);
}

int fchmod(int fildes, mode_t mode) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CHMOD;
    req.handle = e.srv_handle;
    req.mode = (uint32_t)(mode & 07777u);
    req.flags = VFS_FLAG_BY_FD;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int fchownat(int fd, const char *path, uid_t owner, gid_t group, int flag) {
    if (flag & ~AT_SYMLINK_NOFOLLOW) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CHOWN;
    req.mode = (uint32_t)owner;
    req.whence = (uint32_t)group;
    req.flags = (uint32_t)flag;
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int chown(const char *path, uid_t owner, gid_t group) {
    return fchownat(AT_FDCWD, path, owner, group, 0);
}

int lchown(const char *path, uid_t owner, gid_t group) {
    return fchownat(AT_FDCWD, path, owner, group, AT_SYMLINK_NOFOLLOW);
}

int fchown(int fildes, uid_t owner, gid_t group) {
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_CHOWN;
    req.handle = e.srv_handle;
    req.mode = (uint32_t)owner;
    req.whence = (uint32_t)group;
    req.flags = VFS_FLAG_BY_FD;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int faccessat(int fd, const char *path, int amode, int flag) {
    if (amode & ~(F_OK | R_OK | W_OK | X_OK)) {
        errno = EINVAL;
        return -1;
    }
    if (flag & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW)) {
        errno = EINVAL;
        return -1;
    }
    struct stat st = {0};
    int sf = (flag & AT_SYMLINK_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0;
    if (fstatat(fd, path, &st, sf) != 0) {
        return -1;
    }
    if ((amode & R_OK) && !(st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH))) {
        errno = EACCES;
        return -1;
    }
    if ((amode & W_OK) && !(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) {
        errno = EACCES;
        return -1;
    }
    if ((amode & X_OK) && !(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int linkat(int fd1, const char *path1, int fd2, const char *path2, int flag) {
    if (flag & ~AT_SYMLINK_FOLLOW) {
        errno = EINVAL;
        return -1;
    }
    char old_buf[VFS_PATH_MAX];
    char new_buf[VFS_PATH_MAX];
    uint32_t old_h = 0u;
    uint32_t new_h = 0u;
    int r1 = build_req_path(fd1, path1, &old_h, old_buf);
    if (r1 < 0) {
        errno = -r1;
        return -1;
    }
    int r2 = build_req_path(fd2, path2, &new_h, new_buf);
    if (r2 < 0) {
        errno = -r2;
        return -1;
    }
    if (strlen(old_buf) >= VFS_RENAME_PATH_MAX ||
        strlen(new_buf) >= VFS_RENAME_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_LINK;
    req.handle = old_h;
    req.flags = (uint32_t)flag;
    req.u.ren.new_handle = new_h;
    (void)strlcpy(req.u.ren.old_path, old_buf, sizeof(req.u.ren.old_path));
    (void)strlcpy(req.u.ren.new_path, new_buf, sizeof(req.u.ren.new_path));
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int link(const char *path1, const char *path2) {
    return linkat(AT_FDCWD, path1, AT_FDCWD, path2, 0);
}

int symlinkat(const char *path1, int fd, const char *path2) {
    if (!path1 || !path2) {
        errno = EFAULT;
        return -1;
    }
    if (path1[0] == '\0' || path2[0] == '\0') {
        errno = ENOENT;
        return -1;
    }
    char new_buf[VFS_PATH_MAX];
    uint32_t new_h = 0u;
    int r2 = build_req_path(fd, path2, &new_h, new_buf);
    if (r2 < 0) {
        errno = -r2;
        return -1;
    }
    if (strlen(path1) >= VFS_RENAME_PATH_MAX ||
        strlen(new_buf) >= VFS_RENAME_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_SYMLINK;
    req.handle = new_h;
    (void)strlcpy(req.u.ren.old_path, path1, sizeof(req.u.ren.old_path));
    (void)strlcpy(req.u.ren.new_path, new_buf, sizeof(req.u.ren.new_path));
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int symlink(const char *path1, const char *path2) {
    return symlinkat(path1, AT_FDCWD, path2);
}

ssize_t readlinkat(int fd, const char *restrict path, char *restrict buf,
                   size_t bufsize) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_READLINK;
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    size_t copy_n = rep.len < bufsize ? (size_t)rep.len : bufsize;
    if (copy_n > 0u) {
        memcpy(buf, rep.u.path, copy_n);
    }
    return (ssize_t)copy_n;
}

ssize_t readlink(const char *restrict path, char *restrict buf,
                 size_t bufsize) {
    return readlinkat(AT_FDCWD, path, buf, bufsize);
}

int mkfifoat(int fd, const char *path, mode_t mode) {
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_MKFIFO;
    mode_t cur_umask = current_umask();
    req.mode = (uint32_t)(mode & ~cur_umask & 0777u);
    int pr = build_req_path(fd, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    return 0;
}

int mkfifo(const char *path, mode_t mode) {
    return mkfifoat(AT_FDCWD, path, mode);
}

ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset) {
    __libc_cancel_point();
    if (!buf && nbyte > 0u) {
        errno = EFAULT;
        return -1;
    }
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_SERVER || e.kind == FD_KIND_CONSOLE ||
        e.kind == FD_KIND_TTY || e.kind == FD_KIND_SOCK) {
        errno = ESPIPE;
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EBADF;
        return -1;
    }
    if (nbyte == 0u) {
        return 0;
    }
    uint8_t *dst = (uint8_t *)buf;
    size_t done = 0u;
    while (done < nbyte) {
        size_t chunk = nbyte - done;
        if (chunk > VFS_DATA_MAX) {
            chunk = VFS_DATA_MAX;
        }
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_PREAD;
        req.handle = e.srv_handle;
        req.offset = (int64_t)offset + (int64_t)done;
        req.len = (uint32_t)chunk;
        long r = vfs_rpc(&req, &rep);
        if (r < 0) {
            if (done > 0u) {
                return (ssize_t)done;
            }
            errno = (int)(-r);
            return -1;
        }
        if (rep.len > 0u) {
            memcpy(dst + done, rep.u.data, rep.len);
            done += rep.len;
        }
        if (rep.len < chunk) {
            break;
        }
    }
    return (ssize_t)done;
}

ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset) {
    __libc_cancel_point();
    if (!buf && nbyte > 0u) {
        errno = EFAULT;
        return -1;
    }
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind == FD_KIND_SERVER || e.kind == FD_KIND_CONSOLE ||
        e.kind == FD_KIND_TTY || e.kind == FD_KIND_SOCK) {
        errno = ESPIPE;
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EBADF;
        return -1;
    }
    if (nbyte == 0u) {
        return 0;
    }
    const uint8_t *src = (const uint8_t *)buf;
    size_t done = 0u;
    while (done < nbyte) {
        size_t chunk = nbyte - done;
        if (chunk > VFS_DATA_MAX) {
            chunk = VFS_DATA_MAX;
        }
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_PWRITE;
        req.handle = e.srv_handle;
        req.offset = (int64_t)offset + (int64_t)done;
        req.len = (uint32_t)chunk;
        memcpy(req.u.data, src + done, chunk);
        long r = vfs_rpc(&req, &rep);
        if (r < 0) {
            if (done > 0u) {
                return (ssize_t)done;
            }
            errno = (int)(-r);
            return -1;
        }
        done += rep.len;
        if (rep.len < chunk) {
            break;
        }
    }
    return (ssize_t)done;
}

int truncate(const char *path, off_t length) {
    if (length < 0) {
        errno = EINVAL;
        return -1;
    }
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    int r = ftruncate(fd, length);
    int saved = errno;
    (void)close(fd);
    if (r < 0) {
        errno = saved;
    }
    return r;
}

ssize_t posix_getdents(int fildes, void *buf, size_t nbyte, int flags) {
    if (flags != 0) {
        errno = EINVAL;
        return -1;
    }
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = ENOTDIR;
        return -1;
    }

    size_t hdr_sz = offsetof(struct posix_dent, d_name);
    size_t max_rec = (hdr_sz + 256u + 7u) & ~(size_t)7u;
    if (nbyte < hdr_sz + 8u) {
        errno = EINVAL;
        return -1;
    }

    uint8_t *out = (uint8_t *)buf;
    size_t used = 0u;
    for (;;) {
        if (used > 0u && used + max_rec > nbyte) {
            break;
        }
        vfs_req_t req = {0};
        vfs_rep_t rep = {0};
        req.op = VFS_OP_READDIR;
        req.handle = e.srv_handle;
        req.flags = VFS_READDIR_NEXT;
        long r = vfs_rpc(&req, &rep);
        if (r < 0) {
            if (used > 0u) {
                return (ssize_t)used;
            }
            errno = (int)(-r);
            return -1;
        }
        if (rep.flags != 0u) {
            break; /* EOF */
        }
        size_t nlen = strlen(rep.u.dent.d_name);
        size_t rec = (hdr_sz + nlen + 1u + 7u) & ~(size_t)7u;
        if (used + rec > nbyte) {
            if (used > 0u) {
                break;
            }
            errno = EINVAL;
            return -1;
        }
        struct posix_dent *pd = (struct posix_dent *)(out + used);
        memset(pd, 0, rec);
        pd->d_ino = (ino_t)rep.u.dent.d_ino;
        pd->d_reclen = (reclen_t)rec;
        pd->d_type = (unsigned char)rep.len;
        memcpy(pd->d_name, rep.u.dent.d_name, nlen + 1u);
        used += rec;
    }
    return (ssize_t)used;
}

static void convert_statvfs(const vfs_statvfs_t *src, struct statvfs *dst) {
    dst->f_bsize = (unsigned long)src->f_bsize;
    dst->f_frsize = (unsigned long)src->f_frsize;
    dst->f_blocks = (fsblkcnt_t)src->f_blocks;
    dst->f_bfree = (fsblkcnt_t)src->f_bfree;
    dst->f_bavail = (fsblkcnt_t)src->f_bavail;
    dst->f_files = (fsfilcnt_t)src->f_files;
    dst->f_ffree = (fsfilcnt_t)src->f_ffree;
    dst->f_favail = (fsfilcnt_t)src->f_favail;
    dst->f_fsid = (unsigned long)src->f_fsid;
    dst->f_flag = (unsigned long)src->f_flag;
    dst->f_namemax = (unsigned long)src->f_namemax;
}

int statvfs(const char *restrict path, struct statvfs *restrict buf) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_STATVFS;
    int pr = build_req_path(AT_FDCWD, path, &req.handle, req.u.path);
    if (pr < 0) {
        errno = -pr;
        return -1;
    }
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    convert_statvfs(&rep.u.stvfs, buf);
    return 0;
}

int fstatvfs(int fildes, struct statvfs *buf) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    if (fildes < 0) {
        errno = EBADF;
        return -1;
    }
    struct __k_fd_entry e = {0};
    long gr = __sys5(SYS_FD_OP, FD_OP_GET, fildes, (long)(uintptr_t)&e, 0, 0);
    if (gr < 0) {
        errno = (int)(-gr);
        return -1;
    }
    if (e.kind != FD_KIND_VFS) {
        errno = EINVAL;
        return -1;
    }
    vfs_req_t req = {0};
    vfs_rep_t rep = {0};
    req.op = VFS_OP_STATVFS;
    req.handle = e.srv_handle;
    req.flags = VFS_FLAG_BY_FD;
    long r = vfs_rpc(&req, &rep);
    if (r < 0) {
        errno = (int)(-r);
        return -1;
    }
    convert_statvfs(&rep.u.stvfs, buf);
    return 0;
}
