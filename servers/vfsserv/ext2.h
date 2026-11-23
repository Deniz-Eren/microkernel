/**
 * @file     ext2.h
 * @brief    On-disk ext2 filesystem backend for Ring 3 vfsserv over /dev/blk
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#define EXT2_ROOT_INO 2u

typedef struct {
    uint32_t ino;
    uint16_t mode;
    uint16_t nlink;
    uint32_t size;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
} ext2_inode_info_t;

typedef int (*ext2_dir_cb_t)(const char *name, uint32_t ino, uint8_t file_type,
                             void *ctx);

/* Connect to /dev/blk and mount an existing ext2 filesystem, or format a fresh
 * ext2 filesystem if the disk is unformatted. Sets *formatted_out to 1 if
 * freshly formatted, 0 if an existing filesystem was mounted. Returns 0 on
 * success or -1 if /dev/blk is unavailable. */
int ext2_init(int *formatted_out, uint32_t *total_blocks_out);

int ext2_stat_inode(uint32_t ino, ext2_inode_info_t *out);
int ext2_iterate_dir(uint32_t dir_ino, ext2_dir_cb_t cb, void *ctx);

int ext2_create_file(uint32_t parent_ino, const char *name, uint16_t mode,
                     uint32_t mtime, uint32_t *ino_out);
int ext2_create_dir(uint32_t parent_ino, const char *name, uint16_t mode,
                    uint32_t mtime, uint32_t *ino_out);

long ext2_read_data(uint32_t ino, size_t offset, void *buf, size_t len);
long ext2_write_data(uint32_t ino, size_t offset, const void *buf, size_t len,
                     uint32_t mtime);
int ext2_truncate_inode(uint32_t ino, size_t new_size, uint32_t mtime);
int ext2_set_times(uint32_t ino, uint32_t atime, uint32_t mtime,
                   uint32_t ctime);

int ext2_unlink_entry(uint32_t parent_ino, const char *name, int is_dir,
                      uint32_t mtime, uint32_t *unlinked_ino_out);
int ext2_free_inode_and_blocks(uint32_t ino);

int ext2_rename_entry(uint32_t old_parent_ino, const char *old_name,
                      uint32_t new_parent_ino, const char *new_name, int is_dir,
                      uint32_t mtime, uint32_t *replaced_ino_out);
