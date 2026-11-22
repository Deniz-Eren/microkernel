/**
 * @file     ext2.c
 * @brief    Standard rev-1 ext2 filesystem over Ring 3 /dev/blk IPC
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#include "ext2.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/blk.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <time.h>

/* need:: COMP_IPC_0016 */

#define EXT2_MAGIC 0xEF53u
#define EXT2_BLK_SIZE 1024u
#define EXT2_SECTORS_PER_BLK (EXT2_BLK_SIZE / BLK_SECTOR_SIZE)
#define EXT2_INODE_SIZE 128u
#define EXT2_INODES_PER_BLK (EXT2_BLK_SIZE / EXT2_INODE_SIZE)
#define EXT2_INODES_COUNT 256u
#define EXT2_INODE_TBL_BLKS (EXT2_INODES_COUNT / EXT2_INODES_PER_BLK)
#define EXT2_FIRST_DATA_BLK 1u
#define EXT2_SB_BLK 1u
#define EXT2_BGD_BLK 2u
#define EXT2_BBMP_BLK 3u
#define EXT2_IBMP_BLK 4u
#define EXT2_ITBL_BLK 5u
#define EXT2_ROOT_DIR_BLK (EXT2_ITBL_BLK + EXT2_INODE_TBL_BLKS) /* 37 */
#define EXT2_DIRECT_BLKS 12u
#define EXT2_PTRS_PER_BLK (EXT2_BLK_SIZE / 4u) /* 256 */
#define EXT2_MAX_FILE_BLKS (EXT2_DIRECT_BLKS + EXT2_PTRS_PER_BLK)
#define EXT2_TAIL_RESERVE_SECTORS 32u

#define EXT2_FT_REG_FILE 1u
#define EXT2_FT_DIR 2u

typedef struct __attribute__((packed)) {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t s_uuid[16];
    char s_volume_name[16];
} ext2_super_t;

typedef struct __attribute__((packed)) {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t bg_reserved[12];
} ext2_bgd_t;

typedef struct __attribute__((packed)) {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t i_osd2[12];
} ext2_inode_t;

typedef struct __attribute__((packed)) {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
} ext2_dirent_hdr_t;

static int g_blk_conn = -1;
static ext2_super_t g_sb;
static ext2_bgd_t g_bgd;
static uint8_t g_bbmp[EXT2_BLK_SIZE];
static uint8_t g_ibmp[EXT2_BLK_SIZE];

static int blk_read_block(uint32_t blk, void *buf) {
    static blk_req_t req;
    static blk_rep_t rep;
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_READ;
    req.lba = (uint64_t)blk * EXT2_SECTORS_PER_BLK;
    req.count = EXT2_SECTORS_PER_BLK;
    long r =
        SendMsg(g_blk_conn, &req, offsetof(blk_req_t, data), &rep, sizeof(rep));
    if (r < 0 || rep.err != 0 || rep.bytes != EXT2_BLK_SIZE) {
        return -EIO;
    }
    memcpy(buf, rep.data, EXT2_BLK_SIZE);
    return 0;
}

static int blk_write_block(uint32_t blk, const void *buf) {
    static blk_req_t req;
    static blk_rep_t rep;
    memset(&req, 0, sizeof(req));
    memset(&rep, 0, sizeof(rep));
    req.op = BLK_OP_WRITE;
    req.lba = (uint64_t)blk * EXT2_SECTORS_PER_BLK;
    req.count = EXT2_SECTORS_PER_BLK;
    memcpy(req.data, buf, EXT2_BLK_SIZE);
    long r =
        SendMsg(g_blk_conn, &req, offsetof(blk_req_t, data) + EXT2_BLK_SIZE,
                &rep, sizeof(rep));
    if (r < 0 || rep.err != 0 || rep.bytes != EXT2_BLK_SIZE) {
        return -EIO;
    }
    return 0;
}

static int flush_meta(void) {
    uint8_t buf[EXT2_BLK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &g_sb, sizeof(g_sb));
    if (blk_write_block(EXT2_SB_BLK, buf) < 0) {
        return -EIO;
    }
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &g_bgd, sizeof(g_bgd));
    if (blk_write_block(EXT2_BGD_BLK, buf) < 0) {
        return -EIO;
    }
    if (blk_write_block(g_bgd.bg_block_bitmap, g_bbmp) < 0 ||
        blk_write_block(g_bgd.bg_inode_bitmap, g_ibmp) < 0) {
        return -EIO;
    }
    return 0;
}

static int read_inode(uint32_t ino, ext2_inode_t *out) {
    if (ino == 0u || ino > g_sb.s_inodes_count) {
        return -EINVAL;
    }
    uint32_t idx = ino - 1u;
    uint32_t blk = g_bgd.bg_inode_table + (idx / EXT2_INODES_PER_BLK);
    uint32_t off = (idx % EXT2_INODES_PER_BLK) * EXT2_INODE_SIZE;
    uint8_t buf[EXT2_BLK_SIZE];
    if (blk_read_block(blk, buf) < 0) {
        return -EIO;
    }
    memcpy(out, buf + off, sizeof(*out));
    return 0;
}

static int write_inode(uint32_t ino, const ext2_inode_t *in) {
    if (ino == 0u || ino > g_sb.s_inodes_count) {
        return -EINVAL;
    }
    uint32_t idx = ino - 1u;
    uint32_t blk = g_bgd.bg_inode_table + (idx / EXT2_INODES_PER_BLK);
    uint32_t off = (idx % EXT2_INODES_PER_BLK) * EXT2_INODE_SIZE;
    uint8_t buf[EXT2_BLK_SIZE];
    if (blk_read_block(blk, buf) < 0) {
        return -EIO;
    }
    memcpy(buf + off, in, sizeof(*in));
    return blk_write_block(blk, buf);
}

static void bmp_set(uint8_t *bmp, uint32_t bit) {
    bmp[bit >> 3] |= (uint8_t)(1u << (bit & 7u));
}

static void bmp_clear(uint8_t *bmp, uint32_t bit) {
    bmp[bit >> 3] &= (uint8_t) ~(1u << (bit & 7u));
}

static int bmp_test(const uint8_t *bmp, uint32_t bit) {
    return (bmp[bit >> 3] & (uint8_t)(1u << (bit & 7u))) != 0u;
}

static uint32_t alloc_block(void) {
    uint32_t max_bits = g_sb.s_blocks_count - g_sb.s_first_data_block;
    for (uint32_t b = 0u; b < max_bits; b++) {
        if (!bmp_test(g_bbmp, b)) {
            bmp_set(g_bbmp, b);
            if (g_sb.s_free_blocks_count > 0u) {
                g_sb.s_free_blocks_count--;
            }
            if (g_bgd.bg_free_blocks_count > 0u) {
                g_bgd.bg_free_blocks_count--;
            }
            uint32_t blk = b + g_sb.s_first_data_block;
            uint8_t z[EXT2_BLK_SIZE];
            memset(z, 0, sizeof(z));
            if (blk_write_block(blk, z) < 0) {
                return 0u;
            }
            return blk;
        }
    }
    return 0u;
}

static void free_block(uint32_t blk) {
    if (blk < g_sb.s_first_data_block || blk >= g_sb.s_blocks_count) {
        return;
    }
    uint32_t b = blk - g_sb.s_first_data_block;
    if (bmp_test(g_bbmp, b)) {
        bmp_clear(g_bbmp, b);
        g_sb.s_free_blocks_count++;
        g_bgd.bg_free_blocks_count++;
    }
}

static uint32_t alloc_inode(void) {
    for (uint32_t i = g_sb.s_first_ino; i <= g_sb.s_inodes_count; i++) {
        if (!bmp_test(g_ibmp, i - 1u)) {
            bmp_set(g_ibmp, i - 1u);
            if (g_sb.s_free_inodes_count > 0u) {
                g_sb.s_free_inodes_count--;
            }
            if (g_bgd.bg_free_inodes_count > 0u) {
                g_bgd.bg_free_inodes_count--;
            }
            return i;
        }
    }
    return 0u;
}

static void free_inode_bit(uint32_t ino) {
    if (ino == 0u || ino > g_sb.s_inodes_count) {
        return;
    }
    if (bmp_test(g_ibmp, ino - 1u)) {
        bmp_clear(g_ibmp, ino - 1u);
        g_sb.s_free_inodes_count++;
        g_bgd.bg_free_inodes_count++;
    }
}

/* Resolve logical block index `lblk` of `ino`, allocating if `create` != 0. */
static int inode_get_block(ext2_inode_t *in, uint32_t lblk, int create,
                           uint32_t *out_blk, int *dirty_in) {
    if (lblk >= EXT2_MAX_FILE_BLKS) {
        return -EFBIG;
    }
    if (lblk < EXT2_DIRECT_BLKS) {
        if (in->i_block[lblk] == 0u && create) {
            uint32_t nb = alloc_block();
            if (nb == 0u) {
                return -ENOSPC;
            }
            in->i_block[lblk] = nb;
            in->i_blocks += EXT2_SECTORS_PER_BLK;
            *dirty_in = 1;
        }
        *out_blk = in->i_block[lblk];
        return 0;
    }
    uint32_t ind_idx = lblk - EXT2_DIRECT_BLKS;
    if (in->i_block[12] == 0u) {
        if (!create) {
            *out_blk = 0u;
            return 0;
        }
        uint32_t ib = alloc_block();
        if (ib == 0u) {
            return -ENOSPC;
        }
        in->i_block[12] = ib;
        in->i_blocks += EXT2_SECTORS_PER_BLK;
        *dirty_in = 1;
    }
    uint32_t ptrs[EXT2_PTRS_PER_BLK];
    if (blk_read_block(in->i_block[12], ptrs) < 0) {
        return -EIO;
    }
    if (ptrs[ind_idx] == 0u && create) {
        uint32_t nb = alloc_block();
        if (nb == 0u) {
            return -ENOSPC;
        }
        ptrs[ind_idx] = nb;
        if (blk_write_block(in->i_block[12], ptrs) < 0) {
            return -EIO;
        }
        in->i_blocks += EXT2_SECTORS_PER_BLK;
        *dirty_in = 1;
    }
    *out_blk = ptrs[ind_idx];
    return 0;
}

static uint16_t dirent_min_len(size_t name_len) {
    return (uint16_t)((sizeof(ext2_dirent_hdr_t) + name_len + 3u) & ~3u);
}

static int dir_add_entry_disk(uint32_t dir_ino, const char *name,
                              uint32_t child_ino, uint8_t ftype,
                              uint32_t mtime) {
    size_t nlen = strlen(name);
    if (nlen == 0u || nlen > 255u) {
        return -ENAMETOOLONG;
    }
    uint16_t need = dirent_min_len(nlen);

    ext2_inode_t din = {0};
    if (read_inode(dir_ino, &din) < 0) {
        return -EIO;
    }
    uint32_t nblks = (din.i_size + EXT2_BLK_SIZE - 1u) / EXT2_BLK_SIZE;
    uint8_t buf[EXT2_BLK_SIZE];

    for (uint32_t lb = 0u; lb < nblks; lb++) {
        uint32_t pblk = 0u;
        int dummy = 0;
        if (inode_get_block(&din, lb, 0, &pblk, &dummy) < 0 || pblk == 0u) {
            continue;
        }
        if (blk_read_block(pblk, buf) < 0) {
            return -EIO;
        }
        uint32_t off = 0u;
        while (off + sizeof(ext2_dirent_hdr_t) <= EXT2_BLK_SIZE) {
            ext2_dirent_hdr_t *de = (ext2_dirent_hdr_t *)(buf + off);
            if (de->rec_len < sizeof(ext2_dirent_hdr_t) ||
                off + de->rec_len > EXT2_BLK_SIZE) {
                break;
            }
            if (de->inode == 0u && de->rec_len >= need) {
                de->inode = child_ino;
                de->name_len = (uint8_t)nlen;
                de->file_type = ftype;
                memcpy(buf + off + sizeof(ext2_dirent_hdr_t), name, nlen);
                if (blk_write_block(pblk, buf) < 0) {
                    return -EIO;
                }
                din.i_mtime = mtime;
                return write_inode(dir_ino, &din);
            }
            uint16_t actual = dirent_min_len(de->name_len);
            if (de->inode != 0u && de->rec_len >= actual + need) {
                uint16_t rem = (uint16_t)(de->rec_len - actual);
                de->rec_len = actual;
                ext2_dirent_hdr_t *ne =
                    (ext2_dirent_hdr_t *)(buf + off + actual);
                ne->inode = child_ino;
                ne->rec_len = rem;
                ne->name_len = (uint8_t)nlen;
                ne->file_type = ftype;
                memcpy(buf + off + actual + sizeof(ext2_dirent_hdr_t), name,
                       nlen);
                if (blk_write_block(pblk, buf) < 0) {
                    return -EIO;
                }
                din.i_mtime = mtime;
                return write_inode(dir_ino, &din);
            }
            off += de->rec_len;
        }
    }

    /* Allocate a new directory block. */
    uint32_t new_blk = 0u;
    int dirty = 0;
    int rc = inode_get_block(&din, nblks, 1, &new_blk, &dirty);
    if (rc < 0) {
        return rc;
    }
    memset(buf, 0, sizeof(buf));
    ext2_dirent_hdr_t *ne = (ext2_dirent_hdr_t *)buf;
    ne->inode = child_ino;
    ne->rec_len = EXT2_BLK_SIZE;
    ne->name_len = (uint8_t)nlen;
    ne->file_type = ftype;
    memcpy(buf + sizeof(ext2_dirent_hdr_t), name, nlen);
    if (blk_write_block(new_blk, buf) < 0) {
        return -EIO;
    }
    din.i_size += EXT2_BLK_SIZE;
    din.i_mtime = mtime;
    if (write_inode(dir_ino, &din) < 0) {
        return -EIO;
    }
    return flush_meta();
}

static int dir_remove_entry_disk(uint32_t dir_ino, const char *name,
                                 uint32_t *ino_out, uint32_t mtime) {
    size_t nlen = strlen(name);
    ext2_inode_t din = {0};
    if (read_inode(dir_ino, &din) < 0) {
        return -EIO;
    }
    uint32_t nblks = (din.i_size + EXT2_BLK_SIZE - 1u) / EXT2_BLK_SIZE;
    uint8_t buf[EXT2_BLK_SIZE];

    for (uint32_t lb = 0u; lb < nblks; lb++) {
        uint32_t pblk = 0u;
        int dummy = 0;
        if (inode_get_block(&din, lb, 0, &pblk, &dummy) < 0 || pblk == 0u) {
            continue;
        }
        if (blk_read_block(pblk, buf) < 0) {
            return -EIO;
        }
        uint32_t off = 0u;
        ext2_dirent_hdr_t *prev = NULL;
        while (off + sizeof(ext2_dirent_hdr_t) <= EXT2_BLK_SIZE) {
            ext2_dirent_hdr_t *de = (ext2_dirent_hdr_t *)(buf + off);
            if (de->rec_len < sizeof(ext2_dirent_hdr_t) ||
                off + de->rec_len > EXT2_BLK_SIZE) {
                break;
            }
            if (de->inode != 0u && de->name_len == nlen &&
                memcmp(buf + off + sizeof(ext2_dirent_hdr_t), name, nlen) ==
                    0) {
                if (ino_out) {
                    *ino_out = de->inode;
                }
                if (prev) {
                    prev->rec_len = (uint16_t)(prev->rec_len + de->rec_len);
                } else {
                    de->inode = 0u;
                }
                if (blk_write_block(pblk, buf) < 0) {
                    return -EIO;
                }
                din.i_mtime = mtime;
                return write_inode(dir_ino, &din);
            }
            prev = de;
            off += de->rec_len;
        }
    }
    return -ENOENT;
}

static int ext2_mkfs(uint32_t total_blocks) {
    memset(&g_sb, 0, sizeof(g_sb));
    memset(&g_bgd, 0, sizeof(g_bgd));
    memset(g_bbmp, 0, sizeof(g_bbmp));
    memset(g_ibmp, 0, sizeof(g_ibmp));

    uint32_t used_blks = EXT2_ROOT_DIR_BLK; /* blocks 1..37 inclusive */
    uint32_t free_blks = total_blocks - 1u - used_blks;
    uint32_t free_inos = EXT2_INODES_COUNT - 10u; /* inodes 1..10 reserved */

    g_sb.s_inodes_count = EXT2_INODES_COUNT;
    g_sb.s_blocks_count = total_blocks;
    g_sb.s_free_blocks_count = free_blks;
    g_sb.s_free_inodes_count = free_inos;
    g_sb.s_first_data_block = EXT2_FIRST_DATA_BLK;
    g_sb.s_log_block_size = 0u;
    g_sb.s_log_frag_size = 0u;
    g_sb.s_blocks_per_group = 8192u;
    g_sb.s_frags_per_group = 8192u;
    g_sb.s_inodes_per_group = EXT2_INODES_COUNT;
    g_sb.s_max_mnt_count = 0xFFFFu;
    g_sb.s_magic = EXT2_MAGIC;
    g_sb.s_state = 1u;
    g_sb.s_errors = 1u;
    g_sb.s_rev_level = 1u;
    g_sb.s_first_ino = 11u;
    g_sb.s_inode_size = EXT2_INODE_SIZE;
    (void)strlcpy(g_sb.s_volume_name, "microkernel",
                  sizeof(g_sb.s_volume_name));

    g_bgd.bg_block_bitmap = EXT2_BBMP_BLK;
    g_bgd.bg_inode_bitmap = EXT2_IBMP_BLK;
    g_bgd.bg_inode_table = EXT2_ITBL_BLK;
    g_bgd.bg_free_blocks_count = (uint16_t)free_blks;
    g_bgd.bg_free_inodes_count = (uint16_t)free_inos;
    g_bgd.bg_used_dirs_count = 1u;

    /* Mark metadata blocks 1..37 as allocated in block bitmap. */
    for (uint32_t blk = EXT2_FIRST_DATA_BLK; blk <= EXT2_ROOT_DIR_BLK; blk++) {
        bmp_set(g_bbmp, blk - EXT2_FIRST_DATA_BLK);
    }
    /* Mark reserved inodes 1..10 as allocated in inode bitmap. */
    for (uint32_t i = 0u; i < 10u; i++) {
        bmp_set(g_ibmp, i);
    }

    /* Zero inode table blocks. */
    uint8_t zero[EXT2_BLK_SIZE];
    memset(zero, 0, sizeof(zero));
    for (uint32_t b = EXT2_ITBL_BLK; b < EXT2_ITBL_BLK + EXT2_INODE_TBL_BLKS;
         b++) {
        if (blk_write_block(b, zero) < 0) {
            return -EIO;
        }
    }

    /* Initialize root directory block (block 37) with "." and "..". */
    uint8_t rdir[EXT2_BLK_SIZE];
    memset(rdir, 0, sizeof(rdir));
    ext2_dirent_hdr_t *d1 = (ext2_dirent_hdr_t *)rdir;
    d1->inode = EXT2_ROOT_INO;
    d1->rec_len = 12u;
    d1->name_len = 1u;
    d1->file_type = EXT2_FT_DIR;
    rdir[sizeof(ext2_dirent_hdr_t)] = '.';

    ext2_dirent_hdr_t *d2 = (ext2_dirent_hdr_t *)(rdir + 12u);
    d2->inode = EXT2_ROOT_INO;
    d2->rec_len = (uint16_t)(EXT2_BLK_SIZE - 12u);
    d2->name_len = 2u;
    d2->file_type = EXT2_FT_DIR;
    rdir[12u + sizeof(ext2_dirent_hdr_t)] = '.';
    rdir[12u + sizeof(ext2_dirent_hdr_t) + 1u] = '.';
    if (blk_write_block(EXT2_ROOT_DIR_BLK, rdir) < 0) {
        return -EIO;
    }

    /* Write root inode (inode 2). */
    ext2_inode_t root_in = {0};
    root_in.i_mode = (uint16_t)(S_IFDIR | 0755u);
    root_in.i_size = EXT2_BLK_SIZE;
    root_in.i_links_count = 2u;
    root_in.i_blocks = EXT2_SECTORS_PER_BLK;
    root_in.i_block[0] = EXT2_ROOT_DIR_BLK;
    if (write_inode(EXT2_ROOT_INO, &root_in) < 0) {
        return -EIO;
    }
    return flush_meta();
}

static int connect_blk(uint64_t *num_sectors_out) {
    int ns = (int)OpenConnection(1u, 1u);
    if (ns < 0) {
        return -1;
    }
    name_msg_t q = {0};
    name_reply_t r = {0};
    q.type = NAME_MSG_QUERY;
    (void)strlcpy(q.path, "/dev/blk", sizeof(q.path));
    uint32_t bpid = 0u;
    uint32_t bchan = 0u;
    for (int tries = 0; tries < 40; tries++) {
        memset(&r, 0, sizeof(r));
        if (SendMsg(ns, &q, sizeof(q), &r, sizeof(r)) >= 0 && r.err == 0 &&
            r.pid > 0) {
            bpid = (uint32_t)r.pid;
            bchan = (uint32_t)r.chanid;
            break;
        }
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 5000000L};
        (void)clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }
    (void)CloseConnection(ns);
    if (bpid == 0u) {
        return -1;
    }
    int conn = (int)OpenConnection(bpid, bchan);
    if (conn < 0) {
        return -1;
    }
    static blk_req_t breq;
    static blk_rep_t brep;
    memset(&breq, 0, sizeof(breq));
    memset(&brep, 0, sizeof(brep));
    breq.op = BLK_OP_INFO;
    if (SendMsg(conn, &breq, offsetof(blk_req_t, data), &brep, sizeof(brep)) <
            0 ||
        brep.err != 0 || brep.sector_size != BLK_SECTOR_SIZE ||
        brep.num_sectors < 256u) {
        (void)CloseConnection(conn);
        return -1;
    }
    *num_sectors_out = brep.num_sectors;
    return conn;
}

int ext2_init(int *formatted_out, uint32_t *total_blocks_out) {
    uint64_t num_sectors = 0ULL;
    g_blk_conn = connect_blk(&num_sectors);
    if (g_blk_conn < 0) {
        return -1;
    }
    uint64_t usable_sectors = num_sectors - EXT2_TAIL_RESERVE_SECTORS;
    uint32_t total_blocks = (uint32_t)(usable_sectors / EXT2_SECTORS_PER_BLK);
    if (total_blocks > 8192u) {
        total_blocks = 8192u;
    }

    uint8_t buf[EXT2_BLK_SIZE];
    if (blk_read_block(EXT2_SB_BLK, buf) < 0) {
        return -1;
    }
    ext2_super_t disk_sb = {0};
    memcpy(&disk_sb, buf, sizeof(disk_sb));
    if (disk_sb.s_magic == EXT2_MAGIC && disk_sb.s_log_block_size == 0u &&
        disk_sb.s_inodes_count == EXT2_INODES_COUNT &&
        disk_sb.s_blocks_count <= total_blocks &&
        disk_sb.s_blocks_count > EXT2_ROOT_DIR_BLK) {
        g_sb = disk_sb;
        if (blk_read_block(EXT2_BGD_BLK, buf) < 0) {
            return -1;
        }
        memcpy(&g_bgd, buf, sizeof(g_bgd));
        if (blk_read_block(g_bgd.bg_block_bitmap, g_bbmp) < 0 ||
            blk_read_block(g_bgd.bg_inode_bitmap, g_ibmp) < 0) {
            return -1;
        }
        g_sb.s_mnt_count++;
        (void)flush_meta();
        if (formatted_out) {
            *formatted_out = 0;
        }
        if (total_blocks_out) {
            *total_blocks_out = g_sb.s_blocks_count;
        }
        return 0;
    }

    if (ext2_mkfs(total_blocks) < 0) {
        return -1;
    }
    if (formatted_out) {
        *formatted_out = 1;
    }
    if (total_blocks_out) {
        *total_blocks_out = g_sb.s_blocks_count;
    }
    return 0;
}

int ext2_stat_inode(uint32_t ino, ext2_inode_info_t *out) {
    ext2_inode_t in = {0};
    int rc = read_inode(ino, &in);
    if (rc < 0) {
        return rc;
    }
    out->ino = ino;
    out->mode = in.i_mode;
    out->nlink = in.i_links_count;
    out->size = in.i_size;
    out->mtime = in.i_mtime;
    return 0;
}

int ext2_iterate_dir(uint32_t dir_ino, ext2_dir_cb_t cb, void *ctx) {
    ext2_inode_t din = {0};
    if (read_inode(dir_ino, &din) < 0) {
        return -EIO;
    }
    uint32_t nblks = (din.i_size + EXT2_BLK_SIZE - 1u) / EXT2_BLK_SIZE;
    uint8_t buf[EXT2_BLK_SIZE];
    char name[256];

    for (uint32_t lb = 0u; lb < nblks; lb++) {
        uint32_t pblk = 0u;
        int dummy = 0;
        if (inode_get_block(&din, lb, 0, &pblk, &dummy) < 0 || pblk == 0u) {
            continue;
        }
        if (blk_read_block(pblk, buf) < 0) {
            return -EIO;
        }
        uint32_t off = 0u;
        while (off + sizeof(ext2_dirent_hdr_t) <= EXT2_BLK_SIZE) {
            ext2_dirent_hdr_t *de = (ext2_dirent_hdr_t *)(buf + off);
            if (de->rec_len < sizeof(ext2_dirent_hdr_t) ||
                off + de->rec_len > EXT2_BLK_SIZE) {
                break;
            }
            if (de->inode != 0u && de->name_len > 0u &&
                off + sizeof(ext2_dirent_hdr_t) + de->name_len <=
                    EXT2_BLK_SIZE) {
                memcpy(name, buf + off + sizeof(ext2_dirent_hdr_t),
                       de->name_len);
                name[de->name_len] = '\0';
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    int r = cb(name, de->inode, de->file_type, ctx);
                    if (r != 0) {
                        return r;
                    }
                }
            }
            off += de->rec_len;
        }
    }
    return 0;
}

int ext2_create_file(uint32_t parent_ino, const char *name, uint16_t mode,
                     uint32_t mtime, uint32_t *ino_out) {
    uint32_t ino = alloc_inode();
    if (ino == 0u) {
        return -ENOSPC;
    }
    ext2_inode_t in = {0};
    in.i_mode = (uint16_t)(S_IFREG | (mode & 0777u));
    in.i_links_count = 1u;
    in.i_ctime = mtime;
    in.i_mtime = mtime;
    in.i_atime = mtime;
    if (write_inode(ino, &in) < 0) {
        free_inode_bit(ino);
        return -EIO;
    }
    int rc = dir_add_entry_disk(parent_ino, name, ino, EXT2_FT_REG_FILE, mtime);
    if (rc < 0) {
        free_inode_bit(ino);
        (void)flush_meta();
        return rc;
    }
    if (flush_meta() < 0) {
        return -EIO;
    }
    *ino_out = ino;
    return 0;
}

int ext2_create_dir(uint32_t parent_ino, const char *name, uint16_t mode,
                    uint32_t mtime, uint32_t *ino_out) {
    uint32_t ino = alloc_inode();
    if (ino == 0u) {
        return -ENOSPC;
    }
    uint32_t dblk = alloc_block();
    if (dblk == 0u) {
        free_inode_bit(ino);
        return -ENOSPC;
    }
    uint8_t buf[EXT2_BLK_SIZE];
    memset(buf, 0, sizeof(buf));
    ext2_dirent_hdr_t *d1 = (ext2_dirent_hdr_t *)buf;
    d1->inode = ino;
    d1->rec_len = 12u;
    d1->name_len = 1u;
    d1->file_type = EXT2_FT_DIR;
    buf[sizeof(ext2_dirent_hdr_t)] = '.';

    ext2_dirent_hdr_t *d2 = (ext2_dirent_hdr_t *)(buf + 12u);
    d2->inode = parent_ino;
    d2->rec_len = (uint16_t)(EXT2_BLK_SIZE - 12u);
    d2->name_len = 2u;
    d2->file_type = EXT2_FT_DIR;
    buf[12u + sizeof(ext2_dirent_hdr_t)] = '.';
    buf[12u + sizeof(ext2_dirent_hdr_t) + 1u] = '.';
    if (blk_write_block(dblk, buf) < 0) {
        free_block(dblk);
        free_inode_bit(ino);
        return -EIO;
    }

    ext2_inode_t in = {0};
    in.i_mode = (uint16_t)(S_IFDIR | (mode & 0777u));
    in.i_size = EXT2_BLK_SIZE;
    in.i_links_count = 2u;
    in.i_blocks = EXT2_SECTORS_PER_BLK;
    in.i_block[0] = dblk;
    in.i_ctime = mtime;
    in.i_mtime = mtime;
    in.i_atime = mtime;
    if (write_inode(ino, &in) < 0) {
        free_block(dblk);
        free_inode_bit(ino);
        return -EIO;
    }

    int rc = dir_add_entry_disk(parent_ino, name, ino, EXT2_FT_DIR, mtime);
    if (rc < 0) {
        free_block(dblk);
        free_inode_bit(ino);
        (void)flush_meta();
        return rc;
    }

    ext2_inode_t pin = {0};
    if (read_inode(parent_ino, &pin) == 0) {
        pin.i_links_count++;
        pin.i_mtime = mtime;
        (void)write_inode(parent_ino, &pin);
    }
    g_bgd.bg_used_dirs_count++;
    if (flush_meta() < 0) {
        return -EIO;
    }
    *ino_out = ino;
    return 0;
}

long ext2_read_data(uint32_t ino, size_t offset, void *buf, size_t len) {
    ext2_inode_t in = {0};
    if (read_inode(ino, &in) < 0) {
        return -EIO;
    }
    if (offset >= in.i_size || len == 0u) {
        return 0;
    }
    size_t avail = (size_t)in.i_size - offset;
    size_t todo = len < avail ? len : avail;
    size_t done = 0u;
    uint8_t blkbuf[EXT2_BLK_SIZE];
    uint8_t *dst = (uint8_t *)buf;

    while (done < todo) {
        size_t pos = offset + done;
        uint32_t lblk = (uint32_t)(pos / EXT2_BLK_SIZE);
        size_t boff = pos % EXT2_BLK_SIZE;
        size_t take = EXT2_BLK_SIZE - boff;
        if (take > todo - done) {
            take = todo - done;
        }
        uint32_t pblk = 0u;
        int dummy = 0;
        int rc = inode_get_block(&in, lblk, 0, &pblk, &dummy);
        if (rc < 0) {
            return rc;
        }
        if (pblk == 0u) {
            memset(dst + done, 0, take);
        } else {
            if (blk_read_block(pblk, blkbuf) < 0) {
                return -EIO;
            }
            memcpy(dst + done, blkbuf + boff, take);
        }
        done += take;
    }
    return (long)done;
}

long ext2_write_data(uint32_t ino, size_t offset, const void *buf, size_t len,
                     uint32_t mtime) {
    if (len == 0u) {
        return 0;
    }
    if (offset > (size_t)EXT2_MAX_FILE_BLKS * EXT2_BLK_SIZE - len) {
        return -EFBIG;
    }
    ext2_inode_t in = {0};
    if (read_inode(ino, &in) < 0) {
        return -EIO;
    }
    size_t done = 0u;
    int dirty_meta = 0;
    uint8_t blkbuf[EXT2_BLK_SIZE];
    const uint8_t *src = (const uint8_t *)buf;

    while (done < len) {
        size_t pos = offset + done;
        uint32_t lblk = (uint32_t)(pos / EXT2_BLK_SIZE);
        size_t boff = pos % EXT2_BLK_SIZE;
        size_t take = EXT2_BLK_SIZE - boff;
        if (take > len - done) {
            take = len - done;
        }
        uint32_t pblk = 0u;
        int rc = inode_get_block(&in, lblk, 1, &pblk, &dirty_meta);
        if (rc < 0) {
            return rc;
        }
        if (take < EXT2_BLK_SIZE) {
            if (blk_read_block(pblk, blkbuf) < 0) {
                return -EIO;
            }
        }
        memcpy(blkbuf + boff, src + done, take);
        if (blk_write_block(pblk, blkbuf) < 0) {
            return -EIO;
        }
        done += take;
    }

    if (offset + done > in.i_size) {
        in.i_size = (uint32_t)(offset + done);
    }
    in.i_mtime = mtime;
    if (write_inode(ino, &in) < 0) {
        return -EIO;
    }
    if (dirty_meta && flush_meta() < 0) {
        return -EIO;
    }
    return (long)done;
}

int ext2_truncate_inode(uint32_t ino, size_t new_size, uint32_t mtime) {
    if (new_size > (size_t)EXT2_MAX_FILE_BLKS * EXT2_BLK_SIZE) {
        return -EFBIG;
    }
    ext2_inode_t in = {0};
    if (read_inode(ino, &in) < 0) {
        return -EIO;
    }
    uint32_t keep_blks =
        (uint32_t)((new_size + EXT2_BLK_SIZE - 1u) / EXT2_BLK_SIZE);
    int dirty_meta = 0;

    for (uint32_t i = keep_blks; i < EXT2_DIRECT_BLKS; i++) {
        if (in.i_block[i] != 0u) {
            free_block(in.i_block[i]);
            in.i_block[i] = 0u;
            if (in.i_blocks >= EXT2_SECTORS_PER_BLK) {
                in.i_blocks -= EXT2_SECTORS_PER_BLK;
            }
            dirty_meta = 1;
        }
    }
    if (in.i_block[12] != 0u) {
        uint32_t ptrs[EXT2_PTRS_PER_BLK];
        if (blk_read_block(in.i_block[12], ptrs) < 0) {
            return -EIO;
        }
        uint32_t ind_keep =
            keep_blks > EXT2_DIRECT_BLKS ? (keep_blks - EXT2_DIRECT_BLKS) : 0u;
        int ind_changed = 0;
        for (uint32_t i = ind_keep; i < EXT2_PTRS_PER_BLK; i++) {
            if (ptrs[i] != 0u) {
                free_block(ptrs[i]);
                ptrs[i] = 0u;
                if (in.i_blocks >= EXT2_SECTORS_PER_BLK) {
                    in.i_blocks -= EXT2_SECTORS_PER_BLK;
                }
                ind_changed = 1;
                dirty_meta = 1;
            }
        }
        if (ind_keep == 0u) {
            free_block(in.i_block[12]);
            in.i_block[12] = 0u;
            if (in.i_blocks >= EXT2_SECTORS_PER_BLK) {
                in.i_blocks -= EXT2_SECTORS_PER_BLK;
            }
            dirty_meta = 1;
        } else if (ind_changed) {
            if (blk_write_block(in.i_block[12], ptrs) < 0) {
                return -EIO;
            }
        }
    }

    /* Zero tail bytes inside the last kept block when shrinking mid-block. */
    if (new_size < in.i_size && (new_size % EXT2_BLK_SIZE) != 0u) {
        uint32_t last_lb = (uint32_t)(new_size / EXT2_BLK_SIZE);
        uint32_t pblk = 0u;
        int dummy = 0;
        if (inode_get_block(&in, last_lb, 0, &pblk, &dummy) == 0 &&
            pblk != 0u) {
            uint8_t blkbuf[EXT2_BLK_SIZE];
            if (blk_read_block(pblk, blkbuf) == 0) {
                size_t boff = new_size % EXT2_BLK_SIZE;
                memset(blkbuf + boff, 0, EXT2_BLK_SIZE - boff);
                (void)blk_write_block(pblk, blkbuf);
            }
        }
    }

    in.i_size = (uint32_t)new_size;
    in.i_mtime = mtime;
    if (write_inode(ino, &in) < 0) {
        return -EIO;
    }
    if (dirty_meta && flush_meta() < 0) {
        return -EIO;
    }
    return 0;
}

int ext2_free_inode_and_blocks(uint32_t ino) {
    if (ino < g_sb.s_first_ino || ino > g_sb.s_inodes_count) {
        return 0;
    }
    ext2_inode_t in = {0};
    if (read_inode(ino, &in) < 0) {
        return -EIO;
    }
    int was_dir = S_ISDIR(in.i_mode);
    (void)ext2_truncate_inode(ino, 0u, 0u);
    memset(&in, 0, sizeof(in));
    (void)write_inode(ino, &in);
    free_inode_bit(ino);
    if (was_dir && g_bgd.bg_used_dirs_count > 1u) {
        g_bgd.bg_used_dirs_count--;
    }
    return flush_meta();
}

int ext2_unlink_entry(uint32_t parent_ino, const char *name, int is_dir,
                      uint32_t mtime, uint32_t *unlinked_ino_out) {
    uint32_t target_ino = 0u;
    int rc = dir_remove_entry_disk(parent_ino, name, &target_ino, mtime);
    if (rc < 0) {
        return rc;
    }
    if (is_dir) {
        ext2_inode_t pin = {0};
        if (read_inode(parent_ino, &pin) == 0 && pin.i_links_count > 2u) {
            pin.i_links_count--;
            pin.i_mtime = mtime;
            (void)write_inode(parent_ino, &pin);
        }
    }
    ext2_inode_t tin = {0};
    if (read_inode(target_ino, &tin) == 0) {
        tin.i_links_count = 0u;
        tin.i_dtime = mtime;
        (void)write_inode(target_ino, &tin);
    }
    if (unlinked_ino_out) {
        *unlinked_ino_out = target_ino;
    }
    return 0;
}

static void dir_update_dotdot(uint32_t dir_ino, uint32_t new_parent_ino) {
    ext2_inode_t din = {0};
    if (read_inode(dir_ino, &din) < 0 || din.i_block[0] == 0u) {
        return;
    }
    uint8_t buf[EXT2_BLK_SIZE];
    if (blk_read_block(din.i_block[0], buf) < 0) {
        return;
    }
    uint32_t off = 0u;
    while (off + sizeof(ext2_dirent_hdr_t) <= EXT2_BLK_SIZE) {
        ext2_dirent_hdr_t *de = (ext2_dirent_hdr_t *)(buf + off);
        if (de->rec_len < sizeof(ext2_dirent_hdr_t) ||
            off + de->rec_len > EXT2_BLK_SIZE) {
            break;
        }
        if (de->inode != 0u && de->name_len == 2u &&
            memcmp(buf + off + sizeof(ext2_dirent_hdr_t), "..", 2u) == 0) {
            de->inode = new_parent_ino;
            (void)blk_write_block(din.i_block[0], buf);
            return;
        }
        off += de->rec_len;
    }
}

int ext2_rename_entry(uint32_t old_parent_ino, const char *old_name,
                      uint32_t new_parent_ino, const char *new_name, int is_dir,
                      uint32_t mtime, uint32_t *replaced_ino_out) {
    if (replaced_ino_out) {
        *replaced_ino_out = 0u;
    }
    uint32_t moved_ino = 0u;
    int rc = dir_remove_entry_disk(old_parent_ino, old_name, &moved_ino, mtime);
    if (rc < 0) {
        return rc;
    }
    uint32_t prev_dst = 0u;
    if (dir_remove_entry_disk(new_parent_ino, new_name, &prev_dst, mtime) ==
        0) {
        if (replaced_ino_out) {
            *replaced_ino_out = prev_dst;
        }
        ext2_inode_t rin = {0};
        if (read_inode(prev_dst, &rin) == 0) {
            rin.i_links_count = 0u;
            rin.i_dtime = mtime;
            (void)write_inode(prev_dst, &rin);
        }
    }
    uint8_t ftype = is_dir ? EXT2_FT_DIR : EXT2_FT_REG_FILE;
    rc = dir_add_entry_disk(new_parent_ino, new_name, moved_ino, ftype, mtime);
    if (rc < 0) {
        /* Roll back best-effort. */
        (void)dir_add_entry_disk(old_parent_ino, old_name, moved_ino, ftype,
                                 mtime);
        return rc;
    }
    if (is_dir && old_parent_ino != new_parent_ino) {
        dir_update_dotdot(moved_ino, new_parent_ino);
        ext2_inode_t opin = {0};
        if (read_inode(old_parent_ino, &opin) == 0 && opin.i_links_count > 2u) {
            opin.i_links_count--;
            (void)write_inode(old_parent_ino, &opin);
        }
        if (prev_dst == 0u) {
            ext2_inode_t npin = {0};
            if (read_inode(new_parent_ino, &npin) == 0) {
                npin.i_links_count++;
                (void)write_inode(new_parent_ino, &npin);
            }
        }
    }
    return flush_meta();
}
