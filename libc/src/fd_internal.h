/**
 * @file     fd_internal.h
 * @brief    Internal libc helpers bridging unistd and Ring 3 IPC servers
 *
 * @copyright
 * Copyright (c) 2026 Deniz Eren <deniz.eren@outlook.com>
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/fd.h>
#include <sys/net.h>
#include <sys/sock.h>
#include <sys/tty.h>
#include <sys/types.h>

ssize_t __libc_fd_read(int fd, void *buf, size_t n);
ssize_t __libc_fd_write(int fd, const void *buf, size_t n);
int __libc_fd_prefork(void);
void __libc_fd_postfork_rollback(void);
long __libc_fd_dup_entry(const struct __k_fd_entry *e);
void __libc_fd_close_entry(const struct __k_fd_entry *e);

ssize_t __libc_vfs_read(uint32_t handle, void *buf, size_t n);
ssize_t __libc_vfs_write(uint32_t handle, const void *buf, size_t n,
                         uint32_t status_flags);
long __libc_vfs_dup_handle(uint32_t handle);
void __libc_vfs_close_handle(uint32_t handle);
int __libc_vfs_resolve_handle(uint32_t handle, char *out, size_t out_cap,
                              uint32_t *mode_out);

int __libc_tty_is_srv(uint32_t srv_pid, uint32_t srv_chanid);
int __libc_tty_open_ptmx(int oflag);
int __libc_tty_open_pts(uint32_t pts_idx, int oflag);
long __libc_tty_dup_handle(uint32_t handle);
void __libc_tty_close_handle(uint32_t handle);
ssize_t __libc_tty_read(uint32_t handle, void *buf, size_t n,
                        uint32_t status_flags);
ssize_t __libc_tty_write(uint32_t handle, const void *buf, size_t n,
                         uint32_t status_flags);
long __libc_tty_poll(const tty_poll_item_t *items, uint32_t nfds,
                     int64_t timeout_ns, tty_poll_item_t *out_items);

int __libc_sock_is_net(uint32_t srv_pid, uint32_t srv_chanid);
ssize_t __libc_sock_read(const struct __k_fd_entry *e, void *buf, size_t n);
ssize_t __libc_sock_write(const struct __k_fd_entry *e, const void *buf,
                          size_t n);
long __libc_sock_dup_handle(const struct __k_fd_entry *e);
void __libc_sock_close_handle(const struct __k_fd_entry *e);
long __libc_sock_poll(const sock_poll_item_t *items, uint32_t nfds,
                      int64_t timeout_ns, sock_poll_item_t *out_items);
long __libc_net_poll(const net_poll_item_t *items, uint32_t nfds,
                     int64_t timeout_ns, net_poll_item_t *out_items);
