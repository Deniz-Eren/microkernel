/**
 * @file     main.c
 * @brief    Ring 3 VFS server (/dev/vfs) with read-only bootfs and tmpfs
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
#include <sys/bootinfo.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include "ext2.h"

/* need:: COMP_IPC_0008 COMP_IPC_0016 COMP_IPC_0017 COMP_IPC_0020 */
/* need:: COMP_LIBC_0011 */

#define VFS_NAME_MAX 128u
#define VFS_MAX_HANDLES 128u
#define VFS_FIFO_CAP 4096u
#define VFS_FIFO_WAIT_MAX 16u
#define VFS_SYMLOOP_MAX 8
#define VFS_DEV_TMPFS 1ULL
#define VFS_DEV_BOOTFS 2ULL
#define VFS_DEV_EXT2 3ULL

#define VFS_CHR_NONE 0u
#define VFS_CHR_NULL 1u
#define VFS_CHR_ZERO 2u
#define VFS_CHR_URANDOM 3u

typedef struct vfs_node vfs_node_t;

typedef struct {
    char name[VFS_NAME_MAX];
    vfs_node_t *node;
} vfs_child_t;

struct vfs_node {
    uint64_t dev;
    uint64_t ino;
    uint32_t ext2_ino;
    uint32_t mode;
    uint32_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint8_t readonly;
    uint8_t dev_kind;
    uint32_t open_refs;
    size_t size;
    size_t cap;
    uint8_t *data;
    const uint8_t *ro_data;
    uint8_t *fifo_buf;
    size_t fifo_head;
    size_t fifo_len;
    uint32_t fifo_readers;
    uint32_t fifo_writers;
    int64_t atime_sec;
    int64_t atime_nsec;
    int64_t mtime_sec;
    int64_t mtime_nsec;
    int64_t ctime_sec;
    int64_t ctime_nsec;
    vfs_node_t *parent;
    vfs_child_t *children;
    size_t child_count;
    size_t child_cap;
};

typedef struct {
    uint8_t used;
    uint32_t refs;
    uint32_t flags;
    size_t offset;
    uint32_t dir_idx;
    vfs_node_t *node;
} vfs_file_t;

typedef struct {
    uint8_t used;
    uint8_t kind; /* 1 = open, 2 = read */
    uint32_t msgid;
    uint32_t handle;
    uint32_t flags;
    uint32_t want;
    vfs_node_t *node;
} vfs_fifo_wait_t;

static vfs_node_t *g_root;
static vfs_node_t *g_mnt;
static uint32_t g_ext2_blocks;
static uint64_t g_next_ino = 1ULL;
static vfs_file_t g_files[VFS_MAX_HANDLES];
static vfs_fifo_wait_t g_fifo_wait[VFS_FIFO_WAIT_MAX];

static uint32_t g_rng_state[16];
static uint32_t g_rng_seeded;
static uint32_t g_rng_bytes_out;

static uint32_t rotl32(uint32_t v, unsigned int n) {
    return (v << n) | (v >> (32u - n));
}

static void chacha20_qr(uint32_t *s, int a, int b, int c, int d) {
    s[a] += s[b];
    s[d] = rotl32(s[d] ^ s[a], 16u);
    s[c] += s[d];
    s[b] = rotl32(s[b] ^ s[c], 12u);
    s[a] += s[b];
    s[d] = rotl32(s[d] ^ s[a], 8u);
    s[c] += s[d];
    s[b] = rotl32(s[b] ^ s[c], 7u);
}

static void urandom_reseed(void) {
    uint8_t seed[44] = {0};
    (void)getentropy(seed, sizeof(seed));
    g_rng_state[0] = 0x61707865u;
    g_rng_state[1] = 0x3320646Eu;
    g_rng_state[2] = 0x79622D32u;
    g_rng_state[3] = 0x6B206574u;
    for (int i = 0; i < 8; i++) {
        uint32_t w = 0u;
        memcpy(&w, seed + (size_t)i * 4u, 4u);
        g_rng_state[4 + i] = w;
    }
    g_rng_state[12] = 1u;
    for (int i = 0; i < 3; i++) {
        uint32_t w = 0u;
        memcpy(&w, seed + 32u + (size_t)i * 4u, 4u);
        g_rng_state[13 + i] = w;
    }
    g_rng_seeded = 1u;
    g_rng_bytes_out = 0u;
}

static void urandom_fill(uint8_t *dst, size_t n) {
    if (!g_rng_seeded || g_rng_bytes_out >= 65536u) {
        urandom_reseed();
    }
    size_t pos = 0u;
    while (pos < n) {
        uint32_t x[16];
        memcpy(x, g_rng_state, sizeof(x));
        for (int r = 0; r < 10; r++) {
            chacha20_qr(x, 0, 4, 8, 12);
            chacha20_qr(x, 1, 5, 9, 13);
            chacha20_qr(x, 2, 6, 10, 14);
            chacha20_qr(x, 3, 7, 11, 15);
            chacha20_qr(x, 0, 5, 10, 15);
            chacha20_qr(x, 1, 6, 11, 12);
            chacha20_qr(x, 2, 7, 8, 13);
            chacha20_qr(x, 3, 4, 9, 14);
        }
        uint8_t block[64];
        for (int i = 0; i < 16; i++) {
            uint32_t w = x[i] + g_rng_state[i];
            memcpy(block + (size_t)i * 4u, &w, 4u);
        }
        g_rng_state[12]++;
        size_t take = n - pos;
        if (take > sizeof(block)) {
            take = sizeof(block);
        }
        memcpy(dst + pos, block, take);
        pos += take;
        g_rng_bytes_out += (uint32_t)take;
    }
}

static void get_now(int64_t *sec, int64_t *nsec) {
    struct timespec ts = {0};
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        *sec = ts.tv_sec;
        *nsec = ts.tv_nsec;
    } else {
        *sec = 0;
        *nsec = 0;
    }
}

static vfs_node_t *node_new(uint64_t dev, uint32_t mode, uint8_t ro,
                            vfs_node_t *parent) {
    vfs_node_t *n = (vfs_node_t *)calloc(1u, sizeof(*n));
    if (!n) {
        return NULL;
    }
    n->dev = dev;
    n->ino = g_next_ino++;
    n->mode = mode;
    n->nlink = S_ISDIR(mode) ? 2u : 1u;
    n->readonly = ro;
    n->parent = parent ? parent : n;
    get_now(&n->mtime_sec, &n->mtime_nsec);
    n->atime_sec = n->mtime_sec;
    n->atime_nsec = n->mtime_nsec;
    n->ctime_sec = n->mtime_sec;
    n->ctime_nsec = n->mtime_nsec;
    return n;
}

static void node_destroy(vfs_node_t *n) {
    if (!n) {
        return;
    }
    free(n->data);
    free(n->fifo_buf);
    free(n->children);
    free(n);
}

static int dir_find_idx(const vfs_node_t *dir, const char *name) {
    for (size_t i = 0; i < dir->child_count; i++) {
        if (strcmp(dir->children[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int dir_add_child(vfs_node_t *dir, const char *name, vfs_node_t *child) {
    if (strlen(name) >= VFS_NAME_MAX) {
        return -ENAMETOOLONG;
    }
    if (dir_find_idx(dir, name) >= 0) {
        return -EEXIST;
    }
    if (dir->child_count == dir->child_cap) {
        size_t ncap = dir->child_cap ? dir->child_cap * 2u : 8u;
        vfs_child_t *nc =
            (vfs_child_t *)reallocarray(dir->children, ncap, sizeof(*nc));
        if (!nc) {
            return -ENOMEM;
        }
        dir->children = nc;
        dir->child_cap = ncap;
    }
    vfs_child_t *slot = &dir->children[dir->child_count++];
    (void)strlcpy(slot->name, name, sizeof(slot->name));
    slot->node = child;
    if (S_ISDIR(child->mode)) {
        child->parent = dir;
        dir->nlink++;
    }
    get_now(&dir->mtime_sec, &dir->mtime_nsec);
    return 0;
}

static uint64_t parse_octal(const char *s, size_t n) {
    uint64_t v = 0;
    for (size_t i = 0; i < n && s[i]; i++) {
        if (s[i] >= '0' && s[i] <= '7') {
            v = (v << 3) | (uint64_t)(s[i] - '0');
        }
    }
    return v;
}

static void populate_bootfs(vfs_node_t *boot_dir) {
    const unsigned long *auxv = (const unsigned long *)__auxv;
    if (!auxv) {
        return;
    }
    unsigned long bi_va = 0;
    for (size_t i = 0; auxv[i] != 0UL; i += 2) {
        if (auxv[i] == AT_MICROKERNEL_BOOTINFO) {
            bi_va = auxv[i + 1];
            break;
        }
    }
    if (!bi_va) {
        return;
    }
    const boot_info_t *bi = (const boot_info_t *)(uintptr_t)bi_va;
    if (bi->magic != BOOTINFO_MAGIC || bi->boot_tar_size < 512u) {
        return;
    }

    const uint8_t *base =
        (const uint8_t *)(uintptr_t)(BOOTFS_USER_VA +
                                     (bi->boot_tar_phys & 0xFFFULL));
    size_t off = 0;
    while (off + 512u <= bi->boot_tar_size) {
        const char *hdr = (const char *)(base + off);
        if (hdr[0] == '\0') {
            break;
        }
        if (memcmp(hdr + 257, "ustar", 5u) != 0) {
            break;
        }
        uint64_t sz = parse_octal(hdr + 124, 12u);
        size_t data_off = off + 512u;
        size_t next_off = data_off + (size_t)((sz + 511ULL) & ~511ULL);
        if (next_off > bi->boot_tar_size) {
            break;
        }
        char name[VFS_NAME_MAX];
        size_t nl = 0;
        while (nl + 1u < sizeof(name) && nl < 100u && hdr[nl] != '\0') {
            name[nl] = hdr[nl];
            nl++;
        }
        name[nl] = '\0';
        const char *leaf = name;
        if (leaf[0] == '.' && leaf[1] == '/') {
            leaf += 2;
        }
        if (leaf[0] != '\0' && strchr(leaf, '/') == NULL) {
            vfs_node_t *fn =
                node_new(VFS_DEV_BOOTFS, S_IFREG | 0555u, 1u, boot_dir);
            if (fn) {
                fn->size = (size_t)sz;
                fn->ro_data = base + data_off;
                if (dir_add_child(boot_dir, leaf, fn) != 0) {
                    node_destroy(fn);
                }
            }
        }
        off = next_off;
    }
}

static vfs_file_t *get_file(uint32_t handle) {
    if (handle == 0u || handle >= VFS_MAX_HANDLES || !g_files[handle].used) {
        return NULL;
    }
    return &g_files[handle];
}

static int alloc_file(vfs_node_t *node, uint32_t flags, uint32_t *out_handle) {
    for (uint32_t i = 1u; i < VFS_MAX_HANDLES; i++) {
        if (!g_files[i].used) {
            g_files[i].used = 1u;
            g_files[i].refs = 1u;
            g_files[i].flags = flags;
            g_files[i].offset = 0u;
            g_files[i].dir_idx = 0u;
            g_files[i].node = node;
            node->open_refs++;
            *out_handle = i;
            return 0;
        }
    }
    return -ENFILE;
}

static void fill_stat(const vfs_node_t *n, vfs_stat_t *st);

static size_t fifo_pop(vfs_node_t *n, uint8_t *dst, size_t max_n) {
    size_t take = n->fifo_len < max_n ? n->fifo_len : max_n;
    for (size_t i = 0; i < take; i++) {
        dst[i] = n->fifo_buf[(n->fifo_head + i) % VFS_FIFO_CAP];
    }
    n->fifo_head = (n->fifo_head + take) % VFS_FIFO_CAP;
    n->fifo_len -= take;
    return take;
}

static void fifo_wake_waiters(vfs_node_t *n) {
    for (uint32_t i = 0; i < VFS_FIFO_WAIT_MAX; i++) {
        if (!g_fifo_wait[i].used || g_fifo_wait[i].node != n) {
            continue;
        }
        if (g_fifo_wait[i].kind == 1u) {
            uint32_t acc = g_fifo_wait[i].flags & (uint32_t)O_ACCMODE;
            int ready = (acc == (uint32_t)O_RDONLY && n->fifo_writers > 0u) ||
                        (acc == (uint32_t)O_WRONLY && n->fifo_readers > 0u);
            if (!ready) {
                continue;
            }
            vfs_rep_t rep = {0};
            uint32_t h = 0u;
            int hr = alloc_file(n, g_fifo_wait[i].flags, &h);
            if (hr < 0) {
                rep.err = hr;
            } else {
                rep.err = 0;
                rep.handle = h;
                fill_stat(n, &rep.u.st);
            }
            uint32_t msgid = g_fifo_wait[i].msgid;
            g_fifo_wait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        } else if (g_fifo_wait[i].kind == 2u) {
            if (n->fifo_len == 0u && n->fifo_writers > 0u) {
                continue;
            }
            vfs_rep_t rep = {0};
            if (n->fifo_len > 0u) {
                size_t want = g_fifo_wait[i].want > VFS_DATA_MAX
                                  ? VFS_DATA_MAX
                                  : g_fifo_wait[i].want;
                rep.len = (uint32_t)fifo_pop(n, rep.u.data, want);
            } else {
                rep.len = 0u;
            }
            rep.err = 0;
            uint32_t msgid = g_fifo_wait[i].msgid;
            g_fifo_wait[i].used = 0u;
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        }
    }
}

static void release_handle(uint32_t handle) {
    vfs_file_t *f = get_file(handle);
    if (!f) {
        return;
    }
    if (f->refs > 1u) {
        f->refs--;
        return;
    }
    vfs_node_t *n = f->node;
    uint32_t fflags = f->flags;
    memset(f, 0, sizeof(*f));
    if (n && S_ISFIFO(n->mode)) {
        uint32_t acc = fflags & (uint32_t)O_ACCMODE;
        if ((acc == (uint32_t)O_RDONLY || acc == (uint32_t)O_RDWR) &&
            n->fifo_readers > 0u) {
            n->fifo_readers--;
        }
        if ((acc == (uint32_t)O_WRONLY || acc == (uint32_t)O_RDWR) &&
            n->fifo_writers > 0u) {
            n->fifo_writers--;
        }
        fifo_wake_waiters(n);
    }
    if (n && n->open_refs > 0u) {
        n->open_refs--;
        if (n->nlink == 0u && n->open_refs == 0u && !n->readonly) {
            if (n->dev == VFS_DEV_EXT2 && n->ext2_ino != 0u) {
                (void)ext2_free_inode_and_blocks(n->ext2_ino);
            }
            node_destroy(n);
        }
    }
}

/* Resolve `path` starting from `start` into `(parent, leaf, target)` with
 * optional symlink traversal. */
static int resolve_path_ex(vfs_node_t *start, const char *path, int follow_last,
                           int depth, vfs_node_t **out_parent, char *out_leaf,
                           size_t leaf_cap, vfs_node_t **out_node) {
    if (!path || path[0] == '\0') {
        return -ENOENT;
    }
    if (depth > VFS_SYMLOOP_MAX) {
        return -ELOOP;
    }
    vfs_node_t *cur = (path[0] == '/') ? g_root : start;
    if (!cur || !S_ISDIR(cur->mode)) {
        return -ENOTDIR;
    }

    const char *p = path;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        if (out_parent) {
            *out_parent = cur->parent;
        }
        if (out_leaf && leaf_cap > 0u) {
            out_leaf[0] = '\0';
        }
        if (out_node) {
            *out_node = cur;
        }
        return 0;
    }

    char comp[VFS_NAME_MAX];
    for (;;) {
        size_t len = 0;
        while (p[len] != '\0' && p[len] != '/') {
            if (len + 1u >= sizeof(comp)) {
                return -ENAMETOOLONG;
            }
            comp[len] = p[len];
            len++;
        }
        comp[len] = '\0';
        p += len;
        int had_slash = (*p == '/');
        while (*p == '/') {
            p++;
        }
        int last = (*p == '\0');

        if (!S_ISDIR(cur->mode)) {
            return -ENOTDIR;
        }

        vfs_node_t *next = NULL;
        if (strcmp(comp, ".") == 0) {
            next = cur;
        } else if (strcmp(comp, "..") == 0) {
            next = cur->parent;
        } else {
            int idx = dir_find_idx(cur, comp);
            if (idx >= 0) {
                next = cur->children[idx].node;
            }
        }

        if (next && S_ISLNK(next->mode) &&
            (!last || follow_last || had_slash)) {
            char target[VFS_PATH_MAX];
            size_t tlen = next->size;
            if (tlen >= sizeof(target)) {
                return -ENAMETOOLONG;
            }
            if (tlen > 0u && next->data) {
                memcpy(target, next->data, tlen);
            }
            target[tlen] = '\0';
            if (last) {
                return resolve_path_ex(cur, target, follow_last, depth + 1,
                                       out_parent, out_leaf, leaf_cap,
                                       out_node);
            }
            vfs_node_t *resolved = NULL;
            int sr = resolve_path_ex(cur, target, 1, depth + 1, NULL, NULL, 0u,
                                     &resolved);
            if (sr < 0 || !resolved) {
                return sr < 0 ? sr : -ENOENT;
            }
            next = resolved;
        }

        if (last) {
            if (out_parent) {
                *out_parent = cur;
            }
            if (out_leaf && leaf_cap > 0u) {
                (void)strlcpy(out_leaf, comp, leaf_cap);
            }
            if (out_node) {
                *out_node = next;
            }
            return next ? 0 : -ENOENT;
        }
        if (!next) {
            return -ENOENT;
        }
        cur = next;
    }
}

static int resolve_path(vfs_node_t *start, const char *path,
                        vfs_node_t **out_parent, char *out_leaf,
                        size_t leaf_cap, vfs_node_t **out_node) {
    return resolve_path_ex(start, path, 1, 0, out_parent, out_leaf, leaf_cap,
                           out_node);
}

static void fill_stat(const vfs_node_t *n, vfs_stat_t *st) {
    memset(st, 0, sizeof(*st));
    st->st_dev = n->dev;
    st->st_ino = n->ino;
    st->st_mode = n->mode;
    st->st_nlink = n->nlink;
    st->st_uid = n->uid;
    st->st_gid = n->gid;
    st->st_size = (uint64_t)n->size;
    st->st_atime_sec = n->atime_sec;
    st->st_atime_nsec = n->atime_nsec;
    st->st_mtime_sec = n->mtime_sec;
    st->st_mtime_nsec = n->mtime_nsec;
    st->st_ctime_sec = n->ctime_sec;
    st->st_ctime_nsec = n->ctime_nsec;
}

static int build_canonical_path(const vfs_node_t *node, char *out, size_t cap) {
    if (node == g_root) {
        if (cap < 2u) {
            return -ENAMETOOLONG;
        }
        out[0] = '/';
        out[1] = '\0';
        return 0;
    }
    const char *parts[32];
    int depth = 0;
    const vfs_node_t *cur = node;
    while (cur && cur != g_root && depth < 32) {
        const vfs_node_t *par = cur->parent;
        const char *found = NULL;
        for (size_t i = 0; i < par->child_count; i++) {
            if (par->children[i].node == cur) {
                found = par->children[i].name;
                break;
            }
        }
        if (!found) {
            return -ENOENT;
        }
        parts[depth++] = found;
        cur = par;
    }
    if (cur != g_root) {
        return -ENAMETOOLONG;
    }
    size_t pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        size_t plen = strlen(parts[i]);
        if (pos + 1u + plen + 1u > cap) {
            return -ENAMETOOLONG;
        }
        out[pos++] = '/';
        memcpy(out + pos, parts[i], plen);
        pos += plen;
    }
    out[pos] = '\0';
    return 0;
}

static vfs_node_t *resolve_start_dir(uint32_t dir_handle, int *err_out) {
    if (dir_handle == 0u) {
        return g_root;
    }
    vfs_file_t *df = get_file(dir_handle);
    if (!df) {
        *err_out = -EBADF;
        return NULL;
    }
    if (!S_ISDIR(df->node->mode)) {
        *err_out = -ENOTDIR;
        return NULL;
    }
    return df->node;
}

static int handle_open(uint32_t msgid, const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return 0;
    }
    vfs_node_t *parent = NULL;
    vfs_node_t *node = NULL;
    char leaf[VFS_NAME_MAX] = {0};
    int follow = (req->flags & (uint32_t)O_NOFOLLOW) == 0u;
    int r = resolve_path_ex(start, req->u.path, follow, 0, &parent, leaf,
                            sizeof(leaf), &node);
    uint32_t acc = req->flags & (uint32_t)O_ACCMODE;

    if (r == 0 && node) {
        if ((req->flags & (uint32_t)(O_CREAT | O_EXCL)) ==
            (uint32_t)(O_CREAT | O_EXCL)) {
            rep->err = -EEXIST;
            return 0;
        }
        if (S_ISLNK(node->mode)) {
            rep->err = -ELOOP;
            return 0;
        }
        if ((req->flags & (uint32_t)O_DIRECTORY) && !S_ISDIR(node->mode)) {
            rep->err = -ENOTDIR;
            return 0;
        }
        if (S_ISDIR(node->mode) && acc != (uint32_t)O_RDONLY) {
            rep->err = -EISDIR;
            return 0;
        }
        if (node->readonly &&
            (acc != (uint32_t)O_RDONLY || (req->flags & (uint32_t)O_TRUNC))) {
            rep->err = -EROFS;
            return 0;
        }
        if (S_ISFIFO(node->mode)) {
            int nonblock = (req->flags & (uint32_t)O_NONBLOCK) != 0u;
            if (acc == (uint32_t)O_WRONLY && nonblock &&
                node->fifo_readers == 0u) {
                rep->err = -ENXIO;
                return 0;
            }
            if (acc == (uint32_t)O_RDONLY || acc == (uint32_t)O_RDWR) {
                node->fifo_readers++;
            }
            if (acc == (uint32_t)O_WRONLY || acc == (uint32_t)O_RDWR) {
                node->fifo_writers++;
            }
            fifo_wake_waiters(node);
            if (!nonblock &&
                ((acc == (uint32_t)O_RDONLY && node->fifo_writers == 0u) ||
                 (acc == (uint32_t)O_WRONLY && node->fifo_readers == 0u))) {
                for (uint32_t i = 0; i < VFS_FIFO_WAIT_MAX; i++) {
                    if (!g_fifo_wait[i].used) {
                        g_fifo_wait[i].used = 1u;
                        g_fifo_wait[i].kind = 1u;
                        g_fifo_wait[i].msgid = msgid;
                        g_fifo_wait[i].flags = req->flags;
                        g_fifo_wait[i].node = node;
                        return 1; /* deferred reply */
                    }
                }
                if (acc == (uint32_t)O_RDONLY && node->fifo_readers > 0u) {
                    node->fifo_readers--;
                }
                if (acc == (uint32_t)O_WRONLY && node->fifo_writers > 0u) {
                    node->fifo_writers--;
                }
                rep->err = -EAGAIN;
                return 0;
            }
        }
        if ((req->flags & (uint32_t)O_TRUNC) && S_ISREG(node->mode)) {
            int64_t sec = 0;
            int64_t nsec = 0;
            get_now(&sec, &nsec);
            if (node->dev == VFS_DEV_EXT2) {
                int tr = ext2_truncate_inode(node->ext2_ino, 0u, (uint32_t)sec);
                if (tr < 0) {
                    rep->err = tr;
                    return 0;
                }
                node->size = 0u;
            } else {
                free(node->data);
                node->data = NULL;
                node->size = 0u;
                node->cap = 0u;
            }
            node->mtime_sec = sec;
            node->mtime_nsec = nsec;
        }
    } else if (r == -ENOENT && (req->flags & (uint32_t)O_CREAT) && parent &&
               leaf[0] != '\0') {
        if (parent->readonly) {
            rep->err = -EROFS;
            return 0;
        }
        if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
            rep->err = -EEXIST;
            return 0;
        }
        uint32_t fmode = S_IFREG | (req->mode & 0777u);
        uint32_t new_eino = 0u;
        if (parent->dev == VFS_DEV_EXT2) {
            int64_t sec = 0;
            int64_t nsec = 0;
            get_now(&sec, &nsec);
            int cr = ext2_create_file(parent->ext2_ino, leaf,
                                      (uint16_t)(req->mode & 0777u),
                                      (uint32_t)sec, &new_eino);
            if (cr < 0) {
                rep->err = cr;
                return 0;
            }
        }
        node = node_new(parent->dev, fmode, 0u, parent);
        if (!node) {
            if (new_eino != 0u) {
                (void)ext2_unlink_entry(parent->ext2_ino, leaf, 0, 0u, NULL);
                (void)ext2_free_inode_and_blocks(new_eino);
            }
            rep->err = -ENOMEM;
            return 0;
        }
        node->ext2_ino = new_eino;
        int ar = dir_add_child(parent, leaf, node);
        if (ar < 0) {
            if (new_eino != 0u) {
                (void)ext2_unlink_entry(parent->ext2_ino, leaf, 0, 0u, NULL);
                (void)ext2_free_inode_and_blocks(new_eino);
            }
            node_destroy(node);
            rep->err = ar;
            return 0;
        }
    } else {
        rep->err = r < 0 ? r : -ENOENT;
        return 0;
    }

    uint32_t h = 0u;
    int hr = alloc_file(node, req->flags, &h);
    if (hr < 0) {
        rep->err = hr;
        return 0;
    }
    rep->err = 0;
    rep->handle = h;
    fill_stat(node, &rep->u.st);
    return 0;
}

static int handle_read(uint32_t msgid, const vfs_req_t *req, vfs_rep_t *rep,
                       int positional) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return 0;
    }
    if ((f->flags & (uint32_t)O_ACCMODE) == (uint32_t)O_WRONLY) {
        rep->err = -EBADF;
        return 0;
    }
    if (S_ISDIR(f->node->mode)) {
        rep->err = -EISDIR;
        return 0;
    }
    if (f->node->dev_kind != VFS_CHR_NONE) {
        uint32_t want = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
        rep->err = 0;
        if (f->node->dev_kind == VFS_CHR_NULL || want == 0u) {
            rep->len = 0u;
        } else if (f->node->dev_kind == VFS_CHR_ZERO) {
            memset(rep->u.data, 0, want);
            rep->len = want;
        } else if (f->node->dev_kind == VFS_CHR_URANDOM) {
            urandom_fill(rep->u.data, want);
            rep->len = want;
        }
        return 0;
    }
    if (S_ISFIFO(f->node->mode)) {
        if (positional) {
            rep->err = -ESPIPE;
            return 0;
        }
        uint32_t want = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
        if (want == 0u) {
            rep->err = 0;
            rep->len = 0u;
            return 0;
        }
        if (f->node->fifo_len > 0u) {
            rep->err = 0;
            rep->len = (uint32_t)fifo_pop(f->node, rep->u.data, want);
            return 0;
        }
        if (f->node->fifo_writers == 0u) {
            rep->err = 0;
            rep->len = 0u;
            return 0;
        }
        if ((f->flags & (uint32_t)O_NONBLOCK) != 0u) {
            rep->err = -EAGAIN;
            return 0;
        }
        for (uint32_t i = 0; i < VFS_FIFO_WAIT_MAX; i++) {
            if (!g_fifo_wait[i].used) {
                g_fifo_wait[i].used = 1u;
                g_fifo_wait[i].kind = 2u;
                g_fifo_wait[i].msgid = msgid;
                g_fifo_wait[i].want = want;
                g_fifo_wait[i].node = f->node;
                return 1;
            }
        }
        rep->err = -EAGAIN;
        return 0;
    }
    if (positional && req->offset < 0) {
        rep->err = -EINVAL;
        return 0;
    }
    size_t off = positional ? (size_t)req->offset : f->offset;
    uint32_t want = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
    if (off >= f->node->size || want == 0u) {
        rep->err = 0;
        rep->len = 0u;
        return 0;
    }
    if (f->node->dev == VFS_DEV_EXT2) {
        long got = ext2_read_data(f->node->ext2_ino, off, rep->u.data, want);
        if (got < 0) {
            rep->err = (int32_t)got;
            return 0;
        }
        if (!positional) {
            f->offset += (size_t)got;
        }
        rep->err = 0;
        rep->len = (uint32_t)got;
        return 0;
    }
    size_t avail = f->node->size - off;
    size_t take = want < avail ? want : avail;
    const uint8_t *src = f->node->ro_data ? f->node->ro_data : f->node->data;
    if (src && take > 0u) {
        memcpy(rep->u.data, src + off, take);
    }
    if (!positional) {
        f->offset += take;
    }
    rep->err = 0;
    rep->len = (uint32_t)take;
    return 0;
}

static int ensure_capacity(vfs_node_t *n, size_t need) {
    if (need <= n->cap) {
        return 0;
    }
    size_t ncap = n->cap ? n->cap : 256u;
    while (ncap < need) {
        if (ncap > ((size_t)-1) / 2u) {
            ncap = need;
            break;
        }
        ncap *= 2u;
    }
    uint8_t *nd = (uint8_t *)realloc(n->data, ncap);
    if (!nd) {
        return -ENOMEM;
    }
    memset(nd + n->cap, 0, ncap - n->cap);
    n->data = nd;
    n->cap = ncap;
    return 0;
}

static void handle_write(const vfs_req_t *req, vfs_rep_t *rep, int positional) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return;
    }
    uint32_t acc = f->flags & (uint32_t)O_ACCMODE;
    if (acc != (uint32_t)O_WRONLY && acc != (uint32_t)O_RDWR) {
        rep->err = -EBADF;
        return;
    }
    if (S_ISDIR(f->node->mode)) {
        rep->err = -EISDIR;
        return;
    }
    if (f->node->dev_kind != VFS_CHR_NONE) {
        uint32_t take = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
        rep->err = 0;
        rep->len = take;
        return;
    }
    if (f->node->readonly) {
        rep->err = -EROFS;
        return;
    }
    if (S_ISFIFO(f->node->mode)) {
        if (positional) {
            rep->err = -ESPIPE;
            return;
        }
        if (f->node->fifo_readers == 0u) {
            rep->err = -EPIPE;
            return;
        }
        uint32_t take = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
        size_t space = VFS_FIFO_CAP - f->node->fifo_len;
        if (space == 0u && take > 0u) {
            rep->err = -EAGAIN;
            return;
        }
        if (take > space) {
            take = (uint32_t)space;
        }
        for (uint32_t i = 0; i < take; i++) {
            size_t tail =
                (f->node->fifo_head + f->node->fifo_len) % VFS_FIFO_CAP;
            f->node->fifo_buf[tail] = req->u.data[i];
            f->node->fifo_len++;
        }
        fifo_wake_waiters(f->node);
        rep->err = 0;
        rep->len = take;
        return;
    }
    if (positional && req->offset < 0) {
        rep->err = -EINVAL;
        return;
    }
    uint32_t take = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
    size_t off = positional ? (size_t)req->offset : f->offset;
    if (!positional && ((f->flags & (uint32_t)O_APPEND) ||
                        (req->flags & (uint32_t)O_APPEND))) {
        off = f->node->size;
        f->offset = off;
    }
    if (take > 0u) {
        if (f->node->dev == VFS_DEV_EXT2) {
            int64_t sec = 0;
            int64_t nsec = 0;
            get_now(&sec, &nsec);
            long put = ext2_write_data(f->node->ext2_ino, off, req->u.data,
                                       take, (uint32_t)sec);
            if (put < 0) {
                rep->err = (int32_t)put;
                return;
            }
            off += (size_t)put;
            if (!positional) {
                f->offset = off;
            }
            if (off > f->node->size) {
                f->node->size = off;
            }
            f->node->mtime_sec = sec;
            f->node->mtime_nsec = nsec;
            rep->err = 0;
            rep->len = (uint32_t)put;
            return;
        }
        if (off > ((size_t)-1) - take) {
            rep->err = -EFBIG;
            return;
        }
        size_t end = off + take;
        int er = ensure_capacity(f->node, end);
        if (er < 0) {
            rep->err = er;
            return;
        }
        if (off > f->node->size) {
            memset(f->node->data + f->node->size, 0, off - f->node->size);
        }
        memcpy(f->node->data + off, req->u.data, take);
        if (!positional) {
            f->offset = end;
        }
        if (end > f->node->size) {
            f->node->size = end;
        }
        get_now(&f->node->mtime_sec, &f->node->mtime_nsec);
    }
    rep->err = 0;
    rep->len = take;
}

static void handle_lseek(const vfs_req_t *req, vfs_rep_t *rep) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return;
    }
    if (S_ISFIFO(f->node->mode)) {
        rep->err = -ESPIPE;
        return;
    }
    if (S_ISCHR(f->node->mode)) {
        rep->err = 0;
        rep->offset = 0;
        return;
    }
    int64_t base = 0;
    if (req->whence == SEEK_SET) {
        base = 0;
    } else if (req->whence == SEEK_CUR) {
        base = (int64_t)f->offset;
    } else if (req->whence == SEEK_END) {
        base = (int64_t)f->node->size;
    } else {
        rep->err = -EINVAL;
        return;
    }
    int64_t target = base + req->offset;
    if (target < 0) {
        rep->err = -EINVAL;
        return;
    }
    f->offset = (size_t)target;
    if (S_ISDIR(f->node->mode) && target == 0) {
        f->dir_idx = 0u;
    }
    rep->err = 0;
    rep->offset = target;
}

static void handle_ftruncate(const vfs_req_t *req, vfs_rep_t *rep) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return;
    }
    uint32_t acc = f->flags & (uint32_t)O_ACCMODE;
    if (acc != (uint32_t)O_WRONLY && acc != (uint32_t)O_RDWR) {
        rep->err = -EINVAL;
        return;
    }
    if (S_ISDIR(f->node->mode)) {
        rep->err = -EISDIR;
        return;
    }
    if (S_ISCHR(f->node->mode)) {
        rep->err = 0;
        return;
    }
    if (f->node->readonly) {
        rep->err = -EROFS;
        return;
    }
    if (req->offset < 0) {
        rep->err = -EINVAL;
        return;
    }
    size_t nlen = (size_t)req->offset;
    if (f->node->dev == VFS_DEV_EXT2) {
        int64_t sec = 0;
        int64_t nsec = 0;
        get_now(&sec, &nsec);
        int tr = ext2_truncate_inode(f->node->ext2_ino, nlen, (uint32_t)sec);
        if (tr < 0) {
            rep->err = tr;
            return;
        }
        f->node->size = nlen;
        f->node->mtime_sec = sec;
        f->node->mtime_nsec = nsec;
        rep->err = 0;
        return;
    }
    if (nlen == 0u) {
        free(f->node->data);
        f->node->data = NULL;
        f->node->size = 0u;
        f->node->cap = 0u;
    } else if (nlen > f->node->size) {
        int er = ensure_capacity(f->node, nlen);
        if (er < 0) {
            rep->err = er;
            return;
        }
        memset(f->node->data + f->node->size, 0, nlen - f->node->size);
        f->node->size = nlen;
    } else {
        f->node->size = nlen;
    }
    get_now(&f->node->mtime_sec, &f->node->mtime_nsec);
    rep->err = 0;
}

static void handle_stat(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    vfs_node_t *node = NULL;
    int follow = (req->flags & (uint32_t)AT_SYMLINK_NOFOLLOW) == 0u;
    int r =
        resolve_path_ex(start, req->u.path, follow, 0, NULL, NULL, 0u, &node);
    if (r < 0 || !node) {
        rep->err = r < 0 ? r : -ENOENT;
        return;
    }
    rep->err = 0;
    fill_stat(node, &rep->u.st);
}

static void handle_mkdir(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    vfs_node_t *parent = NULL;
    vfs_node_t *node = NULL;
    char leaf[VFS_NAME_MAX] = {0};
    int r =
        resolve_path(start, req->u.path, &parent, leaf, sizeof(leaf), &node);
    if (r == 0 && node) {
        rep->err = -EEXIST;
        return;
    }
    if (r != -ENOENT || !parent || leaf[0] == '\0') {
        rep->err = r < 0 ? r : -EINVAL;
        return;
    }
    if (parent->readonly) {
        rep->err = -EROFS;
        return;
    }
    uint32_t new_eino = 0u;
    if (parent->dev == VFS_DEV_EXT2) {
        int64_t sec = 0;
        int64_t nsec = 0;
        get_now(&sec, &nsec);
        int cr = ext2_create_dir(parent->ext2_ino, leaf,
                                 (uint16_t)(req->mode & 0777u), (uint32_t)sec,
                                 &new_eino);
        if (cr < 0) {
            rep->err = cr;
            return;
        }
    }
    vfs_node_t *nd =
        node_new(parent->dev, S_IFDIR | (req->mode & 0777u), 0u, parent);
    if (!nd) {
        if (new_eino != 0u) {
            (void)ext2_unlink_entry(parent->ext2_ino, leaf, 1, 0u, NULL);
            (void)ext2_free_inode_and_blocks(new_eino);
        }
        rep->err = -ENOMEM;
        return;
    }
    nd->ext2_ino = new_eino;
    int ar = dir_add_child(parent, leaf, nd);
    if (ar < 0) {
        if (new_eino != 0u) {
            (void)ext2_unlink_entry(parent->ext2_ino, leaf, 1, 0u, NULL);
            (void)ext2_free_inode_and_blocks(new_eino);
        }
        node_destroy(nd);
        rep->err = ar;
        return;
    }
    rep->err = 0;
}

static void handle_unlink(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    vfs_node_t *parent = NULL;
    vfs_node_t *node = NULL;
    char leaf[VFS_NAME_MAX] = {0};
    int r = resolve_path_ex(start, req->u.path, 0, 0, &parent, leaf,
                            sizeof(leaf), &node);
    if (r < 0 || !node) {
        rep->err = r < 0 ? r : -ENOENT;
        return;
    }
    if (node == g_root || node == g_mnt || strcmp(leaf, ".") == 0 ||
        strcmp(leaf, "..") == 0) {
        rep->err = -EBUSY;
        return;
    }
    if (!parent || parent->readonly || node->readonly) {
        rep->err = -EROFS;
        return;
    }
    int want_dir = (req->flags & (uint32_t)AT_REMOVEDIR) != 0u;
    if (want_dir) {
        if (!S_ISDIR(node->mode)) {
            rep->err = -ENOTDIR;
            return;
        }
        if (node->child_count > 0u) {
            rep->err = -ENOTEMPTY;
            return;
        }
    } else if (S_ISDIR(node->mode)) {
        rep->err = -EISDIR;
        return;
    }

    int idx = dir_find_idx(parent, leaf);
    if (idx < 0) {
        rep->err = -ENOENT;
        return;
    }
    if (parent->dev == VFS_DEV_EXT2) {
        int64_t sec = 0;
        int64_t nsec = 0;
        get_now(&sec, &nsec);
        int ur =
            ext2_unlink_entry(parent->ext2_ino, leaf,
                              S_ISDIR(node->mode) ? 1 : 0, (uint32_t)sec, NULL);
        if (ur < 0) {
            rep->err = ur;
            return;
        }
    }
    parent->children[idx] = parent->children[parent->child_count - 1u];
    parent->child_count--;
    if (S_ISDIR(node->mode) && parent->nlink > 2u) {
        parent->nlink--;
    }
    get_now(&parent->mtime_sec, &parent->mtime_nsec);
    if (node->nlink > 0u) {
        node->nlink--;
    }
    if (S_ISDIR(node->mode)) {
        node->nlink = 0u;
    }
    if (node->nlink == 0u && node->open_refs == 0u) {
        if (node->dev == VFS_DEV_EXT2 && node->ext2_ino != 0u) {
            (void)ext2_free_inode_and_blocks(node->ext2_ino);
        }
        node_destroy(node);
    }
    rep->err = 0;
}

static uint32_t mode_to_dtype(uint32_t mode) {
    if (S_ISREG(mode)) {
        return DT_REG;
    }
    if (S_ISDIR(mode)) {
        return DT_DIR;
    }
    if (S_ISLNK(mode)) {
        return DT_LNK;
    }
    if (S_ISFIFO(mode)) {
        return DT_FIFO;
    }
    if (S_ISCHR(mode)) {
        return DT_CHR;
    }
    if (S_ISBLK(mode)) {
        return DT_BLK;
    }
    if (S_ISSOCK(mode)) {
        return DT_SOCK;
    }
    return DT_UNKNOWN;
}

static void handle_readdir(const vfs_req_t *req, vfs_rep_t *rep) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return;
    }
    if (!S_ISDIR(f->node->mode)) {
        rep->err = -ENOTDIR;
        return;
    }
    uint32_t idx = (req->flags & VFS_READDIR_NEXT) ? f->dir_idx : req->len;
    if (idx == 0u) {
        if (req->flags & VFS_READDIR_NEXT) {
            f->dir_idx = 1u;
        }
        rep->err = 0;
        rep->flags = 0u;
        rep->len = DT_DIR;
        rep->u.dent.d_ino = f->node->ino;
        (void)strlcpy(rep->u.dent.d_name, ".", sizeof(rep->u.dent.d_name));
        return;
    }
    if (idx == 1u) {
        if (req->flags & VFS_READDIR_NEXT) {
            f->dir_idx = 2u;
        }
        rep->err = 0;
        rep->flags = 0u;
        rep->len = DT_DIR;
        rep->u.dent.d_ino = f->node->parent->ino;
        (void)strlcpy(rep->u.dent.d_name, "..", sizeof(rep->u.dent.d_name));
        return;
    }
    size_t cidx = (size_t)(idx - 2u);
    if (cidx >= f->node->child_count) {
        rep->err = 0;
        rep->flags = 1u; /* EOF */
        return;
    }
    if (req->flags & VFS_READDIR_NEXT) {
        f->dir_idx = idx + 1u;
    }
    rep->err = 0;
    rep->flags = 0u;
    rep->len = mode_to_dtype(f->node->children[cidx].node->mode);
    rep->u.dent.d_ino = f->node->children[cidx].node->ino;
    (void)strlcpy(rep->u.dent.d_name, f->node->children[cidx].name,
                  sizeof(rep->u.dent.d_name));
}

static void handle_resolve(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *node = NULL;
    if (req->u.path[0] == '\0') {
        vfs_file_t *f = get_file(req->handle);
        if (!f) {
            rep->err = -EBADF;
            return;
        }
        node = f->node;
    } else {
        vfs_node_t *start = resolve_start_dir(req->handle, &err);
        if (!start) {
            rep->err = err;
            return;
        }
        int r = resolve_path(start, req->u.path, NULL, NULL, 0u, &node);
        if (r < 0 || !node) {
            rep->err = r < 0 ? r : -ENOENT;
            return;
        }
    }
    if (node->nlink == 0u) {
        rep->err = -ENOENT;
        return;
    }
    int pr = build_canonical_path(node, rep->u.path, sizeof(rep->u.path));
    if (pr < 0) {
        rep->err = pr;
        return;
    }
    rep->err = 0;
    rep->flags = node->mode;
}

static void handle_rename(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *old_start = resolve_start_dir(req->handle, &err);
    if (!old_start) {
        rep->err = err;
        return;
    }
    vfs_node_t *new_start = resolve_start_dir(req->u.ren.new_handle, &err);
    if (!new_start) {
        rep->err = err;
        return;
    }

    vfs_node_t *old_parent = NULL;
    vfs_node_t *old_node = NULL;
    char old_leaf[VFS_NAME_MAX] = {0};
    int r1 = resolve_path_ex(old_start, req->u.ren.old_path, 0, 0, &old_parent,
                             old_leaf, sizeof(old_leaf), &old_node);
    if (r1 < 0 || !old_node) {
        rep->err = r1 < 0 ? r1 : -ENOENT;
        return;
    }
    if (old_node == g_root || old_node == g_mnt || strcmp(old_leaf, ".") == 0 ||
        strcmp(old_leaf, "..") == 0) {
        rep->err = -EBUSY;
        return;
    }
    if (!old_parent || old_parent->readonly || old_node->readonly) {
        rep->err = -EROFS;
        return;
    }

    vfs_node_t *new_parent = NULL;
    vfs_node_t *new_node = NULL;
    char new_leaf[VFS_NAME_MAX] = {0};
    int r2 = resolve_path_ex(new_start, req->u.ren.new_path, 0, 0, &new_parent,
                             new_leaf, sizeof(new_leaf), &new_node);
    if ((r2 < 0 && r2 != -ENOENT) || !new_parent || new_leaf[0] == '\0') {
        rep->err = (r2 < 0 && r2 != -ENOENT) ? r2 : -ENOENT;
        return;
    }
    if (strcmp(new_leaf, ".") == 0 || strcmp(new_leaf, "..") == 0) {
        rep->err = -EINVAL;
        return;
    }
    if (new_parent->readonly) {
        rep->err = -EROFS;
        return;
    }
    if (old_parent->dev != new_parent->dev) {
        rep->err = -EXDEV;
        return;
    }
    if (old_node == new_node) {
        rep->err = 0;
        return;
    }

    /* Prevent moving a directory into its own subtree. */
    if (S_ISDIR(old_node->mode)) {
        for (vfs_node_t *anc = new_parent; anc; anc = anc->parent) {
            if (anc == old_node) {
                rep->err = -EINVAL;
                return;
            }
            if (anc == g_root) {
                break;
            }
        }
    }

    vfs_node_t *replaced = NULL;
    int nidx = -1;
    if (new_node) {
        if (new_node->readonly) {
            rep->err = -EROFS;
            return;
        }
        if (S_ISDIR(old_node->mode) && !S_ISDIR(new_node->mode)) {
            rep->err = -ENOTDIR;
            return;
        }
        if (!S_ISDIR(old_node->mode) && S_ISDIR(new_node->mode)) {
            rep->err = -EISDIR;
            return;
        }
        if (S_ISDIR(new_node->mode) && new_node->child_count > 0u) {
            rep->err = -ENOTEMPTY;
            return;
        }
        nidx = dir_find_idx(new_parent, new_leaf);
        if (nidx < 0) {
            rep->err = -ENOENT;
            return;
        }
        replaced = new_node;
    }

    int oidx = dir_find_idx(old_parent, old_leaf);
    if (oidx < 0) {
        rep->err = -ENOENT;
        return;
    }
    if (!replaced && strlen(new_leaf) >= VFS_NAME_MAX) {
        rep->err = -ENAMETOOLONG;
        return;
    }

    if (old_parent->dev == VFS_DEV_EXT2) {
        int64_t sec = 0;
        int64_t nsec = 0;
        get_now(&sec, &nsec);
        int rr = ext2_rename_entry(
            old_parent->ext2_ino, old_leaf, new_parent->ext2_ino, new_leaf,
            S_ISDIR(old_node->mode) ? 1 : 0, (uint32_t)sec, NULL);
        if (rr < 0) {
            rep->err = rr;
            return;
        }
    }

    if (replaced) {
        new_parent->children[nidx].node = old_node;
        if (S_ISDIR(old_node->mode)) {
            old_node->parent = new_parent;
        }
        old_parent->children[oidx] =
            old_parent->children[old_parent->child_count - 1u];
        old_parent->child_count--;
        if (S_ISDIR(old_node->mode) && old_parent->nlink > 2u) {
            old_parent->nlink--;
        }
        get_now(&new_parent->mtime_sec, &new_parent->mtime_nsec);
        if (old_parent != new_parent) {
            get_now(&old_parent->mtime_sec, &old_parent->mtime_nsec);
        }
        if (replaced->nlink > 0u) {
            replaced->nlink--;
        }
        if (S_ISDIR(replaced->mode)) {
            replaced->nlink = 0u;
        }
        if (replaced->nlink == 0u && replaced->open_refs == 0u) {
            if (replaced->dev == VFS_DEV_EXT2 && replaced->ext2_ino != 0u) {
                (void)ext2_free_inode_and_blocks(replaced->ext2_ino);
            }
            node_destroy(replaced);
        }
        rep->err = 0;
        return;
    }

    if (old_parent == new_parent) {
        (void)strlcpy(old_parent->children[oidx].name, new_leaf,
                      sizeof(old_parent->children[oidx].name));
        get_now(&old_parent->mtime_sec, &old_parent->mtime_nsec);
        rep->err = 0;
        return;
    }

    int ar = dir_add_child(new_parent, new_leaf, old_node);
    if (ar < 0) {
        rep->err = ar;
        return;
    }
    old_parent->children[oidx] =
        old_parent->children[old_parent->child_count - 1u];
    old_parent->child_count--;
    if (S_ISDIR(old_node->mode) && old_parent->nlink > 2u) {
        old_parent->nlink--;
    }
    get_now(&old_parent->mtime_sec, &old_parent->mtime_nsec);
    rep->err = 0;
}

static vfs_node_t *resolve_node_or_fd(const vfs_req_t *req, int follow,
                                      int *err_out) {
    if (req->flags & VFS_FLAG_BY_FD) {
        vfs_file_t *f = get_file(req->handle);
        if (!f) {
            *err_out = -EBADF;
            return NULL;
        }
        return f->node;
    }
    vfs_node_t *start = resolve_start_dir(req->handle, err_out);
    if (!start) {
        return NULL;
    }
    vfs_node_t *node = NULL;
    int r =
        resolve_path_ex(start, req->u.path, follow, 0, NULL, NULL, 0u, &node);
    if (r < 0 || !node) {
        *err_out = r < 0 ? r : -ENOENT;
        return NULL;
    }
    return node;
}

static void handle_chmod(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    int follow = (req->flags & (uint32_t)AT_SYMLINK_NOFOLLOW) == 0u;
    vfs_node_t *node = resolve_node_or_fd(req, follow, &err);
    if (!node) {
        rep->err = err;
        return;
    }
    if (node->readonly) {
        rep->err = -EROFS;
        return;
    }
    node->mode = (node->mode & S_IFMT) | (req->mode & 07777u);
    get_now(&node->ctime_sec, &node->ctime_nsec);
    rep->err = 0;
}

static void handle_chown(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    int follow = (req->flags & (uint32_t)AT_SYMLINK_NOFOLLOW) == 0u;
    vfs_node_t *node = resolve_node_or_fd(req, follow, &err);
    if (!node) {
        rep->err = err;
        return;
    }
    if (node->readonly) {
        rep->err = -EROFS;
        return;
    }
    if (req->mode != (uint32_t)-1) {
        node->uid = req->mode;
    }
    if (req->whence != (uint32_t)-1) {
        node->gid = req->whence;
    }
    get_now(&node->ctime_sec, &node->ctime_nsec);
    rep->err = 0;
}

static int valid_utime_nsec(int64_t ns) {
    if (ns == (int64_t)UTIME_NOW || ns == (int64_t)UTIME_OMIT) {
        return 1;
    }
    return ns >= 0 && ns < 1000000000L;
}

static void handle_utimens(const vfs_req_t *req, vfs_rep_t *rep) {
    int64_t an = req->u.utimes.atime_nsec;
    int64_t mn = req->u.utimes.mtime_nsec;
    if (!valid_utime_nsec(an) || !valid_utime_nsec(mn)) {
        rep->err = -EINVAL;
        return;
    }
    int err = 0;
    vfs_node_t *node = NULL;
    if (req->flags & VFS_FLAG_BY_FD) {
        vfs_file_t *f = get_file(req->handle);
        if (!f) {
            rep->err = -EBADF;
            return;
        }
        node = f->node;
    } else {
        vfs_node_t *start = resolve_start_dir(req->handle, &err);
        if (!start) {
            rep->err = err;
            return;
        }
        int follow = (req->flags & (uint32_t)AT_SYMLINK_NOFOLLOW) == 0u;
        int r = resolve_path_ex(start, req->u.utimes.path, follow, 0, NULL,
                                NULL, 0u, &node);
        if (r < 0 || !node) {
            rep->err = r < 0 ? r : -ENOENT;
            return;
        }
    }
    if (node->readonly) {
        rep->err = -EROFS;
        return;
    }
    int64_t now_sec = 0;
    int64_t now_nsec = 0;
    get_now(&now_sec, &now_nsec);
    if (an == (int64_t)UTIME_NOW) {
        node->atime_sec = now_sec;
        node->atime_nsec = now_nsec;
    } else if (an != (int64_t)UTIME_OMIT) {
        node->atime_sec = req->u.utimes.atime_sec;
        node->atime_nsec = an;
    }
    if (mn == (int64_t)UTIME_NOW) {
        node->mtime_sec = now_sec;
        node->mtime_nsec = now_nsec;
    } else if (mn != (int64_t)UTIME_OMIT) {
        node->mtime_sec = req->u.utimes.mtime_sec;
        node->mtime_nsec = mn;
    }
    if (an != (int64_t)UTIME_OMIT || mn != (int64_t)UTIME_OMIT) {
        node->ctime_sec = now_sec;
        node->ctime_nsec = now_nsec;
        if (node->dev == VFS_DEV_EXT2 && node->ext2_ino != 0u) {
            int er = ext2_set_times(node->ext2_ino, (uint32_t)node->atime_sec,
                                    (uint32_t)node->mtime_sec,
                                    (uint32_t)node->ctime_sec);
            if (er < 0) {
                rep->err = er;
                return;
            }
        }
    }
    rep->err = 0;
}

static void handle_link(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *old_start = resolve_start_dir(req->handle, &err);
    if (!old_start) {
        rep->err = err;
        return;
    }
    vfs_node_t *new_start = resolve_start_dir(req->u.ren.new_handle, &err);
    if (!new_start) {
        rep->err = err;
        return;
    }
    vfs_node_t *target = NULL;
    int follow = (req->flags & (uint32_t)AT_SYMLINK_FOLLOW) != 0u;
    int r1 = resolve_path_ex(old_start, req->u.ren.old_path, follow, 0, NULL,
                             NULL, 0u, &target);
    if (r1 < 0 || !target) {
        rep->err = r1 < 0 ? r1 : -ENOENT;
        return;
    }
    if (S_ISDIR(target->mode)) {
        rep->err = -EPERM;
        return;
    }
    vfs_node_t *new_parent = NULL;
    vfs_node_t *existing = NULL;
    char new_leaf[VFS_NAME_MAX] = {0};
    int r2 = resolve_path_ex(new_start, req->u.ren.new_path, 0, 0, &new_parent,
                             new_leaf, sizeof(new_leaf), &existing);
    if (r2 == 0 && existing) {
        rep->err = -EEXIST;
        return;
    }
    if (r2 != -ENOENT || !new_parent || new_leaf[0] == '\0') {
        rep->err = r2 < 0 ? r2 : -EINVAL;
        return;
    }
    if (new_parent->readonly || target->readonly) {
        rep->err = -EROFS;
        return;
    }
    if (new_parent->dev != target->dev) {
        rep->err = -EXDEV;
        return;
    }
    int ar = dir_add_child(new_parent, new_leaf, target);
    if (ar < 0) {
        rep->err = ar;
        return;
    }
    target->nlink++;
    get_now(&target->mtime_sec, &target->mtime_nsec);
    rep->err = 0;
}

static void handle_symlink(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    if (req->u.ren.old_path[0] == '\0') {
        rep->err = -ENOENT;
        return;
    }
    vfs_node_t *parent = NULL;
    vfs_node_t *existing = NULL;
    char leaf[VFS_NAME_MAX] = {0};
    int r = resolve_path_ex(start, req->u.ren.new_path, 0, 0, &parent, leaf,
                            sizeof(leaf), &existing);
    if (r == 0 && existing) {
        rep->err = -EEXIST;
        return;
    }
    if (r != -ENOENT || !parent || leaf[0] == '\0') {
        rep->err = r < 0 ? r : -EINVAL;
        return;
    }
    if (parent->readonly) {
        rep->err = -EROFS;
        return;
    }
    size_t tlen = strlen(req->u.ren.old_path);
    vfs_node_t *ln = node_new(parent->dev, S_IFLNK | 0777u, 0u, parent);
    if (!ln) {
        rep->err = -ENOMEM;
        return;
    }
    ln->data = (uint8_t *)malloc(tlen + 1u);
    if (!ln->data) {
        node_destroy(ln);
        rep->err = -ENOMEM;
        return;
    }
    memcpy(ln->data, req->u.ren.old_path, tlen + 1u);
    ln->size = tlen;
    ln->cap = tlen + 1u;
    int ar = dir_add_child(parent, leaf, ln);
    if (ar < 0) {
        node_destroy(ln);
        rep->err = ar;
        return;
    }
    rep->err = 0;
}

static void handle_readlink(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    vfs_node_t *node = NULL;
    int r = resolve_path_ex(start, req->u.path, 0, 0, NULL, NULL, 0u, &node);
    if (r < 0 || !node) {
        rep->err = r < 0 ? r : -ENOENT;
        return;
    }
    if (!S_ISLNK(node->mode)) {
        rep->err = -EINVAL;
        return;
    }
    size_t tlen = node->size;
    if (tlen >= sizeof(rep->u.path)) {
        tlen = sizeof(rep->u.path) - 1u;
    }
    if (tlen > 0u && node->data) {
        memcpy(rep->u.path, node->data, tlen);
    }
    rep->u.path[tlen] = '\0';
    rep->len = (uint32_t)tlen;
    rep->err = 0;
}

static void handle_mkfifo(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *start = resolve_start_dir(req->handle, &err);
    if (!start) {
        rep->err = err;
        return;
    }
    vfs_node_t *parent = NULL;
    vfs_node_t *existing = NULL;
    char leaf[VFS_NAME_MAX] = {0};
    int r = resolve_path_ex(start, req->u.path, 0, 0, &parent, leaf,
                            sizeof(leaf), &existing);
    if (r == 0 && existing) {
        rep->err = -EEXIST;
        return;
    }
    if (r != -ENOENT || !parent || leaf[0] == '\0') {
        rep->err = r < 0 ? r : -EINVAL;
        return;
    }
    if (parent->readonly) {
        rep->err = -EROFS;
        return;
    }
    vfs_node_t *fn =
        node_new(parent->dev, S_IFIFO | (req->mode & 0777u), 0u, parent);
    if (!fn) {
        rep->err = -ENOMEM;
        return;
    }
    fn->fifo_buf = (uint8_t *)calloc(1u, VFS_FIFO_CAP);
    if (!fn->fifo_buf) {
        node_destroy(fn);
        rep->err = -ENOMEM;
        return;
    }
    int ar = dir_add_child(parent, leaf, fn);
    if (ar < 0) {
        node_destroy(fn);
        rep->err = ar;
        return;
    }
    rep->err = 0;
}

static void count_tree_usage(const vfs_node_t *n, uint64_t dev, uint64_t *files,
                             uint64_t *bytes) {
    if (!n || n->dev != dev) {
        return;
    }
    (*files)++;
    *bytes += (uint64_t)n->size;
    if (S_ISDIR(n->mode)) {
        for (size_t i = 0; i < n->child_count; i++) {
            count_tree_usage(n->children[i].node, dev, files, bytes);
        }
    }
}

static void handle_statvfs(const vfs_req_t *req, vfs_rep_t *rep) {
    int err = 0;
    vfs_node_t *node = resolve_node_or_fd(req, 1, &err);
    if (!node) {
        rep->err = err;
        return;
    }
    vfs_statvfs_t *sv = &rep->u.stvfs;
    memset(sv, 0, sizeof(*sv));
    sv->f_bsize = 4096ULL;
    sv->f_frsize = 4096ULL;
    sv->f_fsid = node->dev;
    sv->f_namemax = 255ULL;
    sv->f_flag = node->readonly ? ST_RDONLY : 0ULL;

    uint64_t used_files = 0ULL;
    uint64_t used_bytes = 0ULL;
    const vfs_node_t *root_for_dev = g_root;
    if (node->dev == VFS_DEV_EXT2 && g_mnt) {
        root_for_dev = g_mnt;
        sv->f_blocks = g_ext2_blocks > 0u ? (uint64_t)g_ext2_blocks : 1024ULL;
        sv->f_files = 512ULL;
    } else if (node->dev == VFS_DEV_BOOTFS) {
        int idx = dir_find_idx(g_root, "boot");
        if (idx >= 0) {
            root_for_dev = g_root->children[idx].node;
        }
        count_tree_usage(root_for_dev, node->dev, &used_files, &used_bytes);
        uint64_t used_blks = (used_bytes + 4095ULL) / 4096ULL;
        sv->f_blocks = used_blks > 0ULL ? used_blks : 1ULL;
        sv->f_bfree = 0ULL;
        sv->f_bavail = 0ULL;
        sv->f_files = used_files;
        sv->f_ffree = 0ULL;
        sv->f_favail = 0ULL;
        rep->err = 0;
        return;
    } else {
        sv->f_blocks = 4096ULL; /* 16 MiB tmpfs quota view */
        sv->f_files = 1024ULL;
    }
    count_tree_usage(root_for_dev, node->dev, &used_files, &used_bytes);
    uint64_t used_blks = (used_bytes + 4095ULL) / 4096ULL;
    sv->f_bfree =
        (sv->f_blocks > used_blks) ? (sv->f_blocks - used_blks) : 0ULL;
    sv->f_bavail = sv->f_bfree;
    sv->f_ffree =
        (sv->f_files > used_files) ? (sv->f_files - used_files) : 0ULL;
    sv->f_favail = sv->f_ffree;
    rep->err = 0;
}

static int register_name(int chanid, const char *path) {
    for (int tries = 0; tries < 200; tries++) {
        long ns = OpenConnection(1, 1);
        if (ns >= 0) {
            name_msg_t req = {0};
            req.type = NAME_MSG_ATTACH;
            req.chanid = (uint32_t)chanid;
            (void)strlcpy(req.path, path, sizeof(req.path));
            name_reply_t rep = {0};
            long r = SendMsg((int)ns, &req, sizeof(req), &rep, sizeof(rep));
            CloseConnection((int)ns);
            if (r >= 0 && rep.err == 0) {
                return 0;
            }
        }
        struct timespec d = {.tv_sec = 0, .tv_nsec = 5000000L};
        clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
    }
    return -1;
}

static int populate_ext2_cb(const char *name, uint32_t ino, uint8_t ftype,
                            void *ctx) {
    (void)ftype;
    vfs_node_t *parent = (vfs_node_t *)ctx;
    ext2_inode_info_t info = {0};
    if (ext2_stat_inode(ino, &info) < 0) {
        return 0;
    }
    vfs_node_t *child = node_new(VFS_DEV_EXT2, info.mode, 0u, parent);
    if (!child) {
        return -ENOMEM;
    }
    child->ext2_ino = ino;
    child->size = info.size;
    child->atime_sec = (int64_t)info.atime;
    child->atime_nsec = 0;
    child->mtime_sec = (int64_t)info.mtime;
    child->mtime_nsec = 0;
    child->ctime_sec = (int64_t)info.ctime;
    child->ctime_nsec = 0;
    if (dir_add_child(parent, name, child) != 0) {
        node_destroy(child);
        return 0;
    }
    if (S_ISDIR(info.mode)) {
        (void)ext2_iterate_dir(ino, populate_ext2_cb, child);
    }
    return 0;
}

static void mount_ext2_on_mnt(void) {
    int formatted = 0;
    uint32_t total_blocks = 0u;
    if (ext2_init(&formatted, &total_blocks) != 0) {
        return;
    }
    g_ext2_blocks = total_blocks;
    vfs_node_t *mnt_dir = node_new(VFS_DEV_EXT2, S_IFDIR | 0755u, 0u, g_root);
    if (!mnt_dir || dir_add_child(g_root, "mnt", mnt_dir) != 0) {
        node_destroy(mnt_dir);
        return;
    }
    mnt_dir->ext2_ino = EXT2_ROOT_INO;
    g_mnt = mnt_dir;
    if (!formatted) {
        (void)ext2_iterate_dir(EXT2_ROOT_INO, populate_ext2_cb, mnt_dir);
        printf("[OK] vfsserv mounted existing ext2 on /mnt (%u blocks,"
               " %zu entries)\n",
               total_blocks, mnt_dir->child_count);
    } else {
        printf("[OK] vfsserv formatted and mounted ext2 on /mnt (%u blocks)\n",
               total_blocks);
    }
}

int main(void) {
    g_root = node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, NULL);
    if (!g_root) {
        printf("[FAIL] vfsserv root allocation failed\n");
        return 1;
    }
    vfs_node_t *boot_dir =
        node_new(VFS_DEV_BOOTFS, S_IFDIR | 0555u, 1u, g_root);
    if (!boot_dir || dir_add_child(g_root, "boot", boot_dir) != 0) {
        node_destroy(boot_dir);
        printf("[FAIL] vfsserv /boot setup failed\n");
        return 1;
    }
    vfs_node_t *tmp_dir = node_new(VFS_DEV_TMPFS, S_IFDIR | 0777u, 0u, g_root);
    if (!tmp_dir || dir_add_child(g_root, "tmp", tmp_dir) != 0) {
        node_destroy(tmp_dir);
        printf("[FAIL] vfsserv /tmp setup failed\n");
        return 1;
    }
    vfs_node_t *dev_dir = node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, g_root);
    if (dev_dir && dir_add_child(g_root, "dev", dev_dir) == 0) {
        struct {
            const char *name;
            uint8_t kind;
        } chr_devs[] = {
            {"null", VFS_CHR_NULL},       {"zero", VFS_CHR_ZERO},
            {"urandom", VFS_CHR_URANDOM}, {"random", VFS_CHR_URANDOM},
            {"tty", VFS_CHR_NONE},        {"ptmx", VFS_CHR_NONE},
        };
        for (size_t i = 0; i < sizeof(chr_devs) / sizeof(chr_devs[0]); i++) {
            vfs_node_t *dn =
                node_new(VFS_DEV_TMPFS, S_IFCHR | 0666u, 0u, dev_dir);
            if (dn) {
                dn->dev_kind = chr_devs[i].kind;
                if (dir_add_child(dev_dir, chr_devs[i].name, dn) != 0) {
                    node_destroy(dn);
                }
            }
        }
        vfs_node_t *pts_dir =
            node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, dev_dir);
        if (pts_dir && dir_add_child(dev_dir, "pts", pts_dir) != 0) {
            node_destroy(pts_dir);
        }
    } else {
        node_destroy(dev_dir);
    }
    populate_bootfs(boot_dir);
    vfs_node_t *bin_link = node_new(VFS_DEV_TMPFS, S_IFLNK | 0777u, 1u, g_root);
    if (bin_link) {
        bin_link->data = (uint8_t *)malloc(6u);
        if (bin_link->data) {
            memcpy(bin_link->data, "/boot", 6u);
            bin_link->size = 5u;
            bin_link->cap = 6u;
            if (dir_add_child(g_root, "bin", bin_link) != 0) {
                node_destroy(bin_link);
            }
        } else {
            node_destroy(bin_link);
        }
    }
    /* /etc/ld.so.conf gives /boot/ld.so and /usr/sbin/ldconfig their
     * library search directories. */
    vfs_node_t *etc_dir = node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, g_root);
    if (etc_dir && dir_add_child(g_root, "etc", etc_dir) != 0) {
        node_destroy(etc_dir);
        etc_dir = NULL;
    }
    static const char k_so_conf[] = "/boot\n/lib\n/usr/lib\n";
    vfs_node_t *so_conf = node_new(VFS_DEV_TMPFS, S_IFREG | 0644u, 1u, etc_dir);
    if (so_conf) {
        so_conf->ro_data = (const uint8_t *)k_so_conf;
        so_conf->size = sizeof(k_so_conf) - 1u;
        if (dir_add_child(etc_dir, "ld.so.conf", so_conf) != 0) {
            node_destroy(so_conf);
        }
    }
    /* /etc/nsswitch.conf gives the five NSS databases small files defaults. */
    static const char k_nsswitch_conf[] =
        "passwd: files\ngroup: files\nhosts: files\n"
        "services: files\nprotocols: files\n";
    vfs_node_t *nss_conf =
        node_new(VFS_DEV_TMPFS, S_IFREG | 0644u, 1u, etc_dir);
    if (nss_conf) {
        nss_conf->ro_data = (const uint8_t *)k_nsswitch_conf;
        nss_conf->size = sizeof(k_nsswitch_conf) - 1u;
        if (dir_add_child(etc_dir, "nsswitch.conf", nss_conf) != 0) {
            node_destroy(nss_conf);
        }
    }
    /* /usr/bin/ldd, /usr/bin/getent, and /usr/sbin/ldconfig point at bootfs. */
    vfs_node_t *usr_dir = node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, g_root);
    if (usr_dir && dir_add_child(g_root, "usr", usr_dir) != 0) {
        node_destroy(usr_dir);
        usr_dir = NULL;
    }
    vfs_node_t *usr_bin = node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, usr_dir);
    if (usr_bin) {
        if (dir_add_child(usr_dir, "bin", usr_bin) != 0) {
            node_destroy(usr_bin);
        } else {
            vfs_node_t *ldd_link =
                node_new(VFS_DEV_TMPFS, S_IFLNK | 0777u, 1u, usr_bin);
            if (ldd_link) {
                ldd_link->data = (uint8_t *)malloc(12u);
                if (ldd_link->data) {
                    memcpy(ldd_link->data, "/boot/ld.so", 12u);
                    ldd_link->size = 11u;
                    ldd_link->cap = 12u;
                    if (dir_add_child(usr_bin, "ldd", ldd_link) != 0) {
                        node_destroy(ldd_link);
                    }
                } else {
                    node_destroy(ldd_link);
                }
            }
            vfs_node_t *getent_link =
                node_new(VFS_DEV_TMPFS, S_IFLNK | 0777u, 1u, usr_bin);
            if (getent_link) {
                getent_link->data = (uint8_t *)malloc(13u);
                if (getent_link->data) {
                    memcpy(getent_link->data, "/boot/getent", 13u);
                    getent_link->size = 12u;
                    getent_link->cap = 13u;
                    if (dir_add_child(usr_bin, "getent", getent_link) != 0) {
                        node_destroy(getent_link);
                    }
                } else {
                    node_destroy(getent_link);
                }
            }
        }
    }
    vfs_node_t *usr_sbin =
        node_new(VFS_DEV_TMPFS, S_IFDIR | 0755u, 0u, usr_dir);
    if (usr_sbin) {
        if (dir_add_child(usr_dir, "sbin", usr_sbin) != 0) {
            node_destroy(usr_sbin);
        } else {
            vfs_node_t *ldconfig_link =
                node_new(VFS_DEV_TMPFS, S_IFLNK | 0777u, 1u, usr_sbin);
            if (ldconfig_link) {
                ldconfig_link->data = (uint8_t *)malloc(15u);
                if (ldconfig_link->data) {
                    memcpy(ldconfig_link->data, "/boot/ldconfig", 15u);
                    ldconfig_link->size = 14u;
                    ldconfig_link->cap = 15u;
                    if (dir_add_child(usr_sbin, "ldconfig", ldconfig_link) !=
                        0) {
                        node_destroy(ldconfig_link);
                    }
                } else {
                    node_destroy(ldconfig_link);
                }
            }
        }
    }
    mount_ext2_on_mnt();

    int chanid = (int)CreateChannel();
    if (chanid < 0) {
        printf("[FAIL] vfsserv CreateChannel -> %d\n", chanid);
        return 1;
    }
    if (register_name(chanid, "/dev/vfs") != 0) {
        printf("[FAIL] vfsserv failed to register /dev/vfs\n");
        return 1;
    }
    printf("[OK] vfsserv registered /dev/vfs (bootfs=%zu entries)\n",
           boot_dir->child_count);

    vfs_req_t req;
    vfs_rep_t rep;
    ipc_recv_info_t info;
    for (;;) {
        memset(&req, 0, sizeof(req));
        memset(&rep, 0, sizeof(rep));
        memset(&info, 0, sizeof(info));
        long msgid = ReceiveMsg(chanid, &req, sizeof(req), &info);
        if (msgid < 0) {
            continue;
        }
        if (msgid == 0) {
            const ipc_pulse_t *pulse = (const ipc_pulse_t *)(const void *)&req;
            if (pulse->code == PULSE_CODE_FD_CLOSE) {
                release_handle(pulse->value);
            }
            continue;
        }

        int deferred = 0;
        switch (req.op) {
        case VFS_OP_OPEN:
            deferred = handle_open((uint32_t)msgid, &req, &rep);
            break;
        case VFS_OP_CLOSE:
            release_handle(req.handle);
            rep.err = 0;
            break;
        case VFS_OP_DUP: {
            vfs_file_t *f = get_file(req.handle);
            if (!f) {
                rep.err = -EBADF;
            } else {
                f->refs++;
                rep.err = 0;
            }
            break;
        }
        case VFS_OP_READ:
            deferred = handle_read((uint32_t)msgid, &req, &rep, 0);
            break;
        case VFS_OP_WRITE:
            handle_write(&req, &rep, 0);
            break;
        case VFS_OP_PREAD:
            deferred = handle_read((uint32_t)msgid, &req, &rep, 1);
            break;
        case VFS_OP_PWRITE:
            handle_write(&req, &rep, 1);
            break;
        case VFS_OP_LSEEK:
            handle_lseek(&req, &rep);
            break;
        case VFS_OP_FTRUNCATE:
            handle_ftruncate(&req, &rep);
            break;
        case VFS_OP_STAT:
            handle_stat(&req, &rep);
            break;
        case VFS_OP_FSTAT: {
            vfs_file_t *f = get_file(req.handle);
            if (!f) {
                rep.err = -EBADF;
            } else {
                rep.err = 0;
                fill_stat(f->node, &rep.u.st);
            }
            break;
        }
        case VFS_OP_MKDIR:
            handle_mkdir(&req, &rep);
            break;
        case VFS_OP_UNLINK:
            handle_unlink(&req, &rep);
            break;
        case VFS_OP_READDIR:
            handle_readdir(&req, &rep);
            break;
        case VFS_OP_RESOLVE:
            handle_resolve(&req, &rep);
            break;
        case VFS_OP_RENAME:
            handle_rename(&req, &rep);
            break;
        case VFS_OP_CHMOD:
            handle_chmod(&req, &rep);
            break;
        case VFS_OP_CHOWN:
            handle_chown(&req, &rep);
            break;
        case VFS_OP_LINK:
            handle_link(&req, &rep);
            break;
        case VFS_OP_SYMLINK:
            handle_symlink(&req, &rep);
            break;
        case VFS_OP_READLINK:
            handle_readlink(&req, &rep);
            break;
        case VFS_OP_MKFIFO:
            handle_mkfifo(&req, &rep);
            break;
        case VFS_OP_STATVFS:
            handle_statvfs(&req, &rep);
            break;
        case VFS_OP_UTIMENS:
            handle_utimens(&req, &rep);
            break;
        case VFS_OP_STATS: {
            uint32_t open_handles = 0;
            for (uint32_t i = 1; i < VFS_MAX_HANDLES; i++) {
                if (g_files[i].used) {
                    open_handles++;
                }
            }
            rep.err = 0;
            rep.len = open_handles;
            break;
        }
        default:
            rep.err = -ENOSYS;
            break;
        }
        if (!deferred) {
            (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
        }
    }
}
