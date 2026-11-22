/**
 * @file     main.c
 * @brief    Ring 3 VFS server (/dev/vfs) with read-only bootfs and tmpfs
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
#include <sys/bootinfo.h>
#include <sys/fd.h>
#include <sys/ipc.h>
#include <sys/microkernel.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include "ext2.h"

/* need:: COMP_IPC_0008 COMP_IPC_0016 */

#define VFS_NAME_MAX 128u
#define VFS_MAX_HANDLES 128u
#define VFS_DEV_TMPFS 1ULL
#define VFS_DEV_BOOTFS 2ULL
#define VFS_DEV_EXT2 3ULL

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
    uint8_t readonly;
    uint32_t open_refs;
    size_t size;
    size_t cap;
    uint8_t *data;
    const uint8_t *ro_data;
    int64_t mtime_sec;
    int64_t mtime_nsec;
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
    vfs_node_t *node;
} vfs_file_t;

static vfs_node_t *g_root;
static vfs_node_t *g_mnt;
static uint64_t g_next_ino = 1ULL;
static vfs_file_t g_files[VFS_MAX_HANDLES];

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
    return n;
}

static void node_destroy(vfs_node_t *n) {
    if (!n) {
        return;
    }
    free(n->data);
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
            g_files[i].node = node;
            node->open_refs++;
            *out_handle = i;
            return 0;
        }
    }
    return -ENFILE;
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
    memset(f, 0, sizeof(*f));
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

/* Resolve `path` starting from `start` into `(parent, leaf, target)`. */
static int resolve_path(vfs_node_t *start, const char *path,
                        vfs_node_t **out_parent, char *out_leaf,
                        size_t leaf_cap, vfs_node_t **out_node) {
    if (!path || path[0] == '\0') {
        return -ENOENT;
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

static void fill_stat(const vfs_node_t *n, vfs_stat_t *st) {
    memset(st, 0, sizeof(*st));
    st->st_dev = n->dev;
    st->st_ino = n->ino;
    st->st_mode = n->mode;
    st->st_nlink = n->nlink;
    st->st_uid = 0u;
    st->st_gid = 0u;
    st->st_size = (uint64_t)n->size;
    st->st_mtime_sec = n->mtime_sec;
    st->st_mtime_nsec = n->mtime_nsec;
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

static void handle_open(const vfs_req_t *req, vfs_rep_t *rep) {
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
    uint32_t acc = req->flags & (uint32_t)O_ACCMODE;

    if (r == 0 && node) {
        if ((req->flags & (uint32_t)(O_CREAT | O_EXCL)) ==
            (uint32_t)(O_CREAT | O_EXCL)) {
            rep->err = -EEXIST;
            return;
        }
        if ((req->flags & (uint32_t)O_DIRECTORY) && !S_ISDIR(node->mode)) {
            rep->err = -ENOTDIR;
            return;
        }
        if (S_ISDIR(node->mode) && acc != (uint32_t)O_RDONLY) {
            rep->err = -EISDIR;
            return;
        }
        if (node->readonly &&
            (acc != (uint32_t)O_RDONLY || (req->flags & (uint32_t)O_TRUNC))) {
            rep->err = -EROFS;
            return;
        }
        if ((req->flags & (uint32_t)O_TRUNC) && S_ISREG(node->mode)) {
            int64_t sec = 0;
            int64_t nsec = 0;
            get_now(&sec, &nsec);
            if (node->dev == VFS_DEV_EXT2) {
                int tr = ext2_truncate_inode(node->ext2_ino, 0u, (uint32_t)sec);
                if (tr < 0) {
                    rep->err = tr;
                    return;
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
            return;
        }
        if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
            rep->err = -EEXIST;
            return;
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
                return;
            }
        }
        node = node_new(parent->dev, fmode, 0u, parent);
        if (!node) {
            if (new_eino != 0u) {
                (void)ext2_unlink_entry(parent->ext2_ino, leaf, 0, 0u, NULL);
                (void)ext2_free_inode_and_blocks(new_eino);
            }
            rep->err = -ENOMEM;
            return;
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
            return;
        }
    } else {
        rep->err = r < 0 ? r : -ENOENT;
        return;
    }

    uint32_t h = 0u;
    int hr = alloc_file(node, req->flags, &h);
    if (hr < 0) {
        rep->err = hr;
        return;
    }
    rep->err = 0;
    rep->handle = h;
    fill_stat(node, &rep->u.st);
}

static void handle_read(const vfs_req_t *req, vfs_rep_t *rep) {
    vfs_file_t *f = get_file(req->handle);
    if (!f) {
        rep->err = -EBADF;
        return;
    }
    if ((f->flags & (uint32_t)O_ACCMODE) == (uint32_t)O_WRONLY) {
        rep->err = -EBADF;
        return;
    }
    if (S_ISDIR(f->node->mode)) {
        rep->err = -EISDIR;
        return;
    }
    uint32_t want = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
    if (f->offset >= f->node->size || want == 0u) {
        rep->err = 0;
        rep->len = 0u;
        return;
    }
    if (f->node->dev == VFS_DEV_EXT2) {
        long got =
            ext2_read_data(f->node->ext2_ino, f->offset, rep->u.data, want);
        if (got < 0) {
            rep->err = (int32_t)got;
            return;
        }
        f->offset += (size_t)got;
        rep->err = 0;
        rep->len = (uint32_t)got;
        return;
    }
    size_t avail = f->node->size - f->offset;
    size_t take = want < avail ? want : avail;
    const uint8_t *src = f->node->readonly ? f->node->ro_data : f->node->data;
    if (src && take > 0u) {
        memcpy(rep->u.data, src + f->offset, take);
    }
    f->offset += take;
    rep->err = 0;
    rep->len = (uint32_t)take;
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

static void handle_write(const vfs_req_t *req, vfs_rep_t *rep) {
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
    if (f->node->readonly) {
        rep->err = -EROFS;
        return;
    }
    uint32_t take = req->len > VFS_DATA_MAX ? VFS_DATA_MAX : req->len;
    if ((f->flags & (uint32_t)O_APPEND) || (req->flags & (uint32_t)O_APPEND)) {
        f->offset = f->node->size;
    }
    if (take > 0u) {
        if (f->node->dev == VFS_DEV_EXT2) {
            int64_t sec = 0;
            int64_t nsec = 0;
            get_now(&sec, &nsec);
            long put = ext2_write_data(f->node->ext2_ino, f->offset,
                                       req->u.data, take, (uint32_t)sec);
            if (put < 0) {
                rep->err = (int32_t)put;
                return;
            }
            f->offset += (size_t)put;
            if (f->offset > f->node->size) {
                f->node->size = f->offset;
            }
            f->node->mtime_sec = sec;
            f->node->mtime_nsec = nsec;
            rep->err = 0;
            rep->len = (uint32_t)put;
            return;
        }
        if (f->offset > ((size_t)-1) - take) {
            rep->err = -EFBIG;
            return;
        }
        size_t end = f->offset + take;
        int er = ensure_capacity(f->node, end);
        if (er < 0) {
            rep->err = er;
            return;
        }
        if (f->offset > f->node->size) {
            memset(f->node->data + f->node->size, 0, f->offset - f->node->size);
        }
        memcpy(f->node->data + f->offset, req->u.data, take);
        f->offset = end;
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
    int r = resolve_path(start, req->u.path, NULL, NULL, 0u, &node);
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
    int r =
        resolve_path(start, req->u.path, &parent, leaf, sizeof(leaf), &node);
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
    node->nlink = 0u;
    if (node->open_refs == 0u) {
        if (node->dev == VFS_DEV_EXT2 && node->ext2_ino != 0u) {
            (void)ext2_free_inode_and_blocks(node->ext2_ino);
        }
        node_destroy(node);
    }
    rep->err = 0;
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
    uint32_t idx = req->len;
    if (idx == 0u) {
        rep->err = 0;
        rep->flags = 0u;
        rep->u.dent.d_ino = f->node->ino;
        (void)strlcpy(rep->u.dent.d_name, ".", sizeof(rep->u.dent.d_name));
        return;
    }
    if (idx == 1u) {
        rep->err = 0;
        rep->flags = 0u;
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
    rep->err = 0;
    rep->flags = 0u;
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
    int r1 = resolve_path(old_start, req->u.ren.old_path, &old_parent, old_leaf,
                          sizeof(old_leaf), &old_node);
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
    int r2 = resolve_path(new_start, req->u.ren.new_path, &new_parent, new_leaf,
                          sizeof(new_leaf), &new_node);
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
        replaced->nlink = 0u;
        if (replaced->open_refs == 0u) {
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
    child->mtime_sec = (int64_t)info.mtime;
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
    populate_bootfs(boot_dir);
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

        switch (req.op) {
        case VFS_OP_OPEN:
            handle_open(&req, &rep);
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
            handle_read(&req, &rep);
            break;
        case VFS_OP_WRITE:
            handle_write(&req, &rep);
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
        (void)ReplyMsg((int)msgid, &rep, sizeof(rep));
    }
}
